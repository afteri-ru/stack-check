// RUN: not  \
// RUN: %clangxx -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/trusted-cpp_clang.so \
// RUN: -Xclang -add-plugin -Xclang trust \
// RUN: -Xclang -plugin-arg-trust -Xclang verbose  %s 2>&1  \
// RUN: | FileCheck %s

// CHECK: Enable verbose mode

// Правильное использование атрибута - на функции
[[trust]]
void correct_function() {
    // CHECK: {{.*}}: verbose: Apply attr 'trust' to correct_function
}

// Правильное использование атрибута - на методе класса
class CorrectClass {
  public:
    [[trust]]
    void correct_method() {
        // CHECK: {{.*}}: verbose: Apply attr 'trust' to CorrectClass::correct_method
    }
};

// Неправильное использование атрибута - на переменной
[[trust]]
int incorrect_variable = 0;
// CHECK: {{.*}}: error: The attribute 'trust' for 'Var' is not applicable.

// Неправильное использование атрибута - на поле класса
class IncorrectClass {
    [[trust]]
    int incorrect_field;
    // CHECK: {{.*}}: error: The attribute 'trust' for 'Field' is not applicable.
};

// Неправильное использование атрибута - в пространстве имен
namespace [[trust]] IncorrectNamespace {
// CHECK: {{.*}}: error: The attribute 'trust' for 'Namespace' is not applicable.
}

// Неправильное использование атрибута - на типе
using InvalidType [[trust]] = int;
// CHECK: {{.*}}: error: The attribute 'trust' for 'TypeAlias' is not applicable.
