#!/usr/bin/env bash

set -euo pipefail

printUsage() {
    cat <<'USAGE'
Usage:
  stack_usage.sh <directory>

Scans the specified directory (recursively) and prints found files with .su extension.
For each .su file, analyzes the content and finds the function with maximum stack size.
Results are sorted by stack size in descending order.

Exit codes:
  0 - found >= 0 files
  2 - parameter/directory error
USAGE
}

# Process command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            printUsage
            exit 0
            ;;
        -*)
            echo "Unknown argument: $1" >&2
            printUsage >&2
            exit 2
            ;;
        *)
            # This should be a directory path
            if [[ -n "${rootDir:-}" ]]; then
                echo "Error: exactly one argument (directory path) is expected." >&2
                printUsage >&2
                exit 2
            fi
            rootDir="$1"
            shift
            ;;
    esac
done

# Check that a directory path is provided
if [[ -z "${rootDir:-}" ]]; then
    echo "Error: exactly one argument (directory path) is expected." >&2
    printUsage >&2
    exit 2
fi

if [[ ! -d "$rootDir" ]]; then
    echo "Error: '$rootDir' is not a directory." >&2
    exit 2
fi

# Find all .su files
suFiles=$(find "$rootDir" -type f -name '*.su' -print0 | sort -z | xargs -0 -r)

# If no files found, exit successfully
if [[ -z "$suFiles" ]]; then
    echo "No .su files found in directory '$rootDir'."
    exit 0
fi

# Temporary file to store maximum entries for sorting
tempFile=$(mktemp)

# Process each .su file
for file in $suFiles; do
    # Check that file exists and is not empty
    if [[ -f "$file" && -s "$file" ]]; then
        # Variables to track maximum stack size in current file
        localMaxStackSize=0
        localMaxStackFunction=""
        
        # Process each line in the file
        while IFS=$'\t' read -r functionName stackSize allocationType _; do
            # Check that we have at least 3 fields
            if [[ -n "$functionName" && -n "$stackSize" ]]; then
                # Check that stack size is a number
                if [[ "$stackSize" =~ ^[0-9]+$ ]]; then
                    # Compare with current maximum value in file
                    if (( stackSize > localMaxStackSize )); then
                        localMaxStackSize=$stackSize
                        localMaxStackFunction=$functionName
                    fi
                fi
            fi
        done < "$file"
        
        # Store maximum entry for sorting
        if [[ $localMaxStackSize -gt 0 ]]; then
            echo -e "$localMaxStackSize\t$localMaxStackFunction\t$file" >> "$tempFile"
        fi
    fi
done

# Print sorted information
if [[ -s "$tempFile" ]]; then
    # Sort by stack size (descending) and print
    echo "Functions with maximum stack usage per file (sorted by stack size, descending):"
    sort -k1,1nr "$tempFile" | while IFS=$'\t' read -r stackSize functionName file; do
        echo "Stack size: $stackSize, Function: $functionName, File: $file"
    done
else
    echo "No valid entries found in .su files."
fi

# Clean up
rm -f "$tempFile"
