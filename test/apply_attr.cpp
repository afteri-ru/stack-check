// RUN: not  \
// RUN: %clangxx -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose  %s 2>&1  \
// RUN: | FileCheck %s

// CHECK: Enable verbose mode

// Правильное использование атрибута - на функции
[[stack_check]]
void correct_function() {
    // CHECK: {{.*}}: verbose: Apply attr 'stack_check' to correct_function
}

// Правильное использование атрибута - на методе класса
class CorrectClass {
  public:
    [[stack_check]]
    void correct_method() {
        // CHECK: {{.*}}: verbose: Apply attr 'stack_check' to CorrectClass::correct_method
    }
};

// Неправильное использование атрибута - на переменной
[[stack_check]]
int incorrect_variable = 0;
// CHECK: {{.*}}: error: The attribute 'stack_check' for 'Var' is not applicable.

// Неправильное использование атрибута - на поле класса
class IncorrectClass {
    [[stack_check]]
    int incorrect_field;
    // CHECK: {{.*}}: error: The attribute 'stack_check' for 'Field' is not applicable.
};

// Неправильное использование атрибута - в пространстве имен
namespace [[stack_check]] IncorrectNamespace {
// CHECK: {{.*}}: error: The attribute 'stack_check' for 'Namespace' is not applicable.
}

// Неправильное использование атрибута - на типе
using InvalidType [[stack_check]] = int;
// CHECK: {{.*}}: error: The attribute 'stack_check' for 'TypeAlias' is not applicable.
