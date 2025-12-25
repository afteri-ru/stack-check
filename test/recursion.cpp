// RUN: %clangxx -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/trusted-cpp_clang.so \
// RUN: -Xclang -add-plugin -Xclang trust \
// RUN: -Xclang -plugin-arg-trust -Xclang verbose  %s 2>&1 \
// RUN: | FileCheck %s

// CHECK: Enable verbose mode
#include <stddef.h>

void function() {
    // CHECK-NOT: verbose
}

size_t recursion(size_t arg) {
    // CHECK: verbose: Recursion call 'recursion'
    if (arg) {
        return arg + recursion(arg - 1);
    }
    return 0;
}


// Правильное использование атрибута - на методе класса
class Class {
  public:
    size_t not_recursion(size_t arg) { return 0; }
    size_t recursion(size_t arg) {
        // CHECK: verbose: Recursion call 'recursion'
        if (arg) {
            return arg + recursion(arg - 1);
        }
        return 0;
    }
};
