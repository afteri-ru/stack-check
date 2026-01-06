// RUN: not %clangxx -I%shlibdir -std=c++20 -fsyntax-only %s 2>&1 | FileCheck %s -check-prefix=OFF

// RUN: not %clangxx -I%shlibdir -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose  %s 2>&1  \
// RUN: | FileCheck %s -check-prefix=ON



#include "stack_check.h"

// ON: Enable verbose mode

STACK_CHECK_SIZE(0)
void correct_function() {
    // OFF: error:
    // ON: error: I couldn't find a way to call MachineFunctionPass from an AST
    // ON: verbose: Apply attr 'stack_check_size' to correct_function
}

STACK_CHECK_LIMIT
void correct_function_limit() {
    // OFF: error:
    // ON: verbose: Apply attr 'stack_check_limit' to correct_function_limit
}


void other_function() {
    // ON-NOT: verbose
}

class CorrectClass {
  public:
    STACK_CHECK_SIZE(100)
    void correct_method() {
        // OFF: error
        // ON: verbose: Apply attr 'stack_check_size' to CorrectClass::correct_method
    }
    STACK_CHECK_LIMIT
    void correct_method_limit() {
        // OFF: error
        // ON: verbose: Apply attr 'stack_check_limit' to CorrectClass::correct_method_limit
    }
};
