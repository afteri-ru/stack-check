#pragma once
#ifndef INCLUDED_TRUST_PLUGIN_H_
#define INCLUDED_TRUST_PLUGIN_H_

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <map>

// #include <yaml-cpp/yaml.h>
#include <stdio.h>
// #include <filesystem>
#include <fstream>
#include <chrono>




#ifdef BUILD_UNITTEST
// Removing all restrictions on access to protected and private fields for testing cases
#define SCOPE(scope)  public
#else
#define SCOPE(scope)  scope
#endif   

/**
 * @def SCOPE( scope )
 * 
 * Removing all restrictions on access to protected and private fields for testing cases
 * 
 */


#define STACK_CHECK_KEYWORD_ATTRIBUTE stack_check
#define STACK_CHECK_KEYWORD_PROFILE "profile"
#define STACK_CHECK_KEYWORD_STATUS "status"
#define STACK_CHECK_KEYWORD_UNSAFE "unsafe"
#define STACK_CHECK_KEYWORD_PRINT_AST "print-ast"

#define STACK_CHECK_KEYWORD_ENABLE "enable"
#define STACK_CHECK_KEYWORD_DISABLE "disable"
#define STACK_CHECK_KEYWORD_PUSH "push"
#define STACK_CHECK_KEYWORD_POP "pop"

// maximum diagnostic level of plugin message when error is detected in source code
#define STACK_CHECK_KEYWORD_LEVEL "level"
#define STACK_CHECK_KEYWORD_ERROR "error"
#define STACK_CHECK_KEYWORD_WARNING "warning"
#define STACK_CHECK_KEYWORD_NOTE "note"
#define STACK_CHECK_KEYWORD_REMARK "remark"
#define STACK_CHECK_KEYWORD_IGNORED "ignored"


#if defined __has_attribute
#if __has_attribute( STACK_CHECK_KEYWORD_ATTRIBUTE )
#define STACK_CHECK_ATTR(...) [[ STACK_CHECK_KEYWORD_ATTRIBUTE (__VA_ARGS__)]]
#define STACK_CHECK_BASELINE(number) STACK_CHECK_ATTR( STACK_CHECK_KEYWORD_BASELINE, #number) void trust_stub();
#endif
#endif

// Disable memory safety plugin attributes 
#ifndef STACK_CHECK_ATTR
#define STACK_CHECK_ATTR(...)
#define STACK_CHECK_BASELINE(number)
#define STACK_CHECK_DISABLE
#endif

#ifndef TO_STR
#define TO_STR2(ARG) #ARG
#define TO_STR(ARG) TO_STR2(ARG)
#endif


#define STACK_CHECK_PRINT_AST(filter) STACK_CHECK_ATTR(STACK_CHECK_KEYWORD_PRINT_AST, filter) void trust_stub();



namespace trust {

    namespace fs = std::filesystem;

    // Copy-Past form https://github.com/google/googletest

    inline void SplitString(const ::std::string& str, char delimiter,
            ::std::vector< ::std::string>* dest) {
        ::std::vector< ::std::string> parsed;
        ::std::string::size_type pos = 0;
        while (true) {
            const ::std::string::size_type colon = str.find(delimiter, pos);
            if (colon == ::std::string::npos) {
                parsed.push_back(str.substr(pos));
                break;
            } else {
                parsed.push_back(str.substr(pos, colon - pos));
                pos = colon + 1;
            }
        }
        dest->swap(parsed);
    }
    // Returns true if and only if the wildcard pattern matches the string. Each
    // pattern consists of regular characters, single-character wildcards (?), and
    // multi-character wildcards (*).
    // 
    // This function implements a linear-time string globbing algorithm based on
    // https://research.swtch.com/glob

    static bool PatternMatchesString(const std::string& name_str,
            const char* pattern, const char* pattern_end) {
        const char* name = name_str.c_str();
        const char* const name_begin = name;
        const char* const name_end = name + name_str.size();

        const char* pattern_next = pattern;
        const char* name_next = name;

        while (pattern < pattern_end || name < name_end) {
            if (pattern < pattern_end) {
                switch (*pattern) {
                    default: // Match an ordinary character.
                        if (name < name_end && *name == *pattern) {
                            ++pattern;
                            ++name;
                            continue;
                        }
                        break;
                    case '?': // Match any single character.
                        if (name < name_end) {
                            ++pattern;
                            ++name;
                            continue;
                        }
                        break;
                    case '*':
                        // Match zero or more characters. Start by skipping over the wildcard
                        // and matching zero characters from name. If that fails, restart and
                        // match one more character than the last attempt.
                        pattern_next = pattern;
                        name_next = name + 1;
                        ++pattern;
                        continue;
                }
            }
            // Failed to match a character. Restart if possible.
            if (name_begin < name_next && name_next <= name_end) {
                pattern = pattern_next;
                name = name_next;
                continue;
            }
            return false;
        }
        return true;
    }

    inline bool IsGlobPattern(const std::string& pattern) {
        return std::any_of(pattern.begin(), pattern.end(),
                [](const char c) {
                    return c == '?' || c == '*'; });
    }

    class StringMatcher {
    public:
        StringMatcher() = default;

        // Constructs a filter from a string of patterns separated by `:`.

        explicit StringMatcher(const std::string& filter, const char separator = ';') {
            Create(filter, separator);
        }

        void Create(const std::string& filter, const char separator = ';') {
            Clear();
            // By design "" filter matches "" string.
            std::vector<std::string> all_patterns;
            SplitString(filter, separator, &all_patterns);
            const auto exact_match_patterns_begin = std::partition(
                    all_patterns.begin(), all_patterns.end(), &IsGlobPattern);

            glob_patterns_.reserve(exact_match_patterns_begin - all_patterns.begin());
            std::move(all_patterns.begin(), exact_match_patterns_begin,
                    std::inserter(glob_patterns_, glob_patterns_.begin()));
            std::move(
                    exact_match_patterns_begin, all_patterns.end(),
                    std::inserter(exact_match_patterns_, exact_match_patterns_.begin()));
        }

        // Returns true if and only if name matches at least one of the patterns in
        // the filter.

        bool MatchesName(const std::string& name) const {
            return exact_match_patterns_.count(name) > 0 ||
                    std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
                    [&name](const std::string & pattern) {
                        return PatternMatchesString(
                                name, pattern.c_str(),
                                pattern.c_str() + pattern.size());
                    });
        }

        //        bool MatchesNameColor(std::string& name, const char *start = "",, const char *stop = "") const {
        //            if (exact_match_patterns_.count(name)) {
        //            } else {
        //            }
        //            return  ||
        //                    std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
        //                    [&name](const std::string & pattern) {
        //                        return PatternMatchesString(
        //                                name, pattern.c_str(),
        //                                pattern.c_str() + pattern.size());
        //                    });
        //        }

        bool isEmpty() {
            return glob_patterns_.empty() && exact_match_patterns_.empty();
        }

        void Clear() {
            glob_patterns_.clear();
            exact_match_patterns_.clear();
        }

        SCOPE(private) :
        std::vector<std::string> glob_patterns_;
        std::unordered_set<std::string> exact_match_patterns_;
    };

    std::string SeparatorRemove(const std::string_view number) {
        std::string result(number);
        auto removed = std::remove(result.begin(), result.end(), '\'');
        removed = std::remove(result.begin(), removed, '_');
        result.erase(removed, result.end());
        return result;
    }

    std::string SeparatorInsert(size_t number, const char sep = '\'') {
        std::string result = std::to_string(number);
        size_t pos = 3;
        while (pos < result.size()) {
            result = result.insert(result.size() - pos, 1, sep);
            pos += 4;
        }
        return result;
    }
}
#endif // INCLUDED_TRUST_PLUGIN_H_
