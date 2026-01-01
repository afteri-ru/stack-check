// RUN: not %clangxx -I%shlibdir -std=c++20 -fsyntax-only %s 2>&1 | FileCheck %s -check-prefix=OFF

// RUN: %clangxx -I%shlibdir -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose  %s 2>&1  \
// RUN: | FileCheck %s -check-prefix=ON



#include "stack_check.h"


// OFF: {{.*}}The 'stack_check' attribute is not supported
// OFF-NOT: {{.*}}warning: unknown attribute 'stack_check' ignored 

// ON: Enable verbose mode

// Правильное использование атрибута - на функции
STACK_CHECK()
void correct_function() {
    // ON: {{.*}}: verbose: Apply attr 'stack_check' to correct_function
}


void other_function() {
    
}

// Правильное использование атрибута - на методе класса
class CorrectClass {
  public:
    STACK_CHECK()
    void correct_method() {
        // ON: {{.*}}: verbose: Apply attr 'stack_check' to CorrectClass::correct_method
    }
};

int main(){

    correct_function();
    other_function();

    return 0;
}
