// RUN: not %clangxx -I%shlibdir -std=c++20 -fsyntax-only %s 2>&1 | FileCheck %s -check-prefix=OFF

// RUN: %clangxx -I%shlibdir -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose  %s 2>&1  \
// RUN: | FileCheck %s -check-prefix=ON



#include "stack_check.h"

// ON: Enable verbose mode

// Правильное использование атрибута - на функции
STACK_CHECK_ATTR()
void correct_function() {
    // OFF: {{.*}}: error:
    // ON: {{.*}}: verbose: Apply attr 'stack_check' to correct_function
}


void other_function() {
    // ON-NOT: {{.*}}verbose
}

// Правильное использование атрибута - на методе класса
class CorrectClass {
  public:
    STACK_CHECK_ATTR(100)
    void correct_method() {
        // OFF: {{.*}}error
        // ON: {{.*}}: verbose: Apply attr 'stack_check' to CorrectClass::correct_method
    }
};

int main(){

    STACK_CHECK_IGNORE_NEXT(0);
    // OFF: {{.*}}: error:
    // ON-NOT: {{.*}}error
    correct_function();
    
    STACK_CHECK_LIMIT(0);
    // OFF: {{.*}}: error:
    // ON: {{.*}}: verbose: Apply attr 'stack_check_limit'
    
    other_function();

    return 0;
}
