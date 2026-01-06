// RUN: not  \
// RUN: %clangxx -std=c++20 -fsyntax-only \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose %s 2>&1  \
// RUN: | FileCheck %s

// CHECK: Enable verbose mode

// Правильное использование атрибута - на функции
[[stack_check_size(0)]]
void correct_function() {
  // CHECK: verbose: Apply attr 'stack_check_size(0)' to correct_function
}

[[stack_check_limit(0)]]
void correct_function_limit() {
  // CHECK: verbose: Apply attr 'stack_check_limit(0)' to correct_function_limit
}

[[stack_check_size(100)]]
void correct_function2();
  // CHECK: verbose: Apply attr 'stack_check_size(100)' to correct_function2

[[stack_check_size(3.3)]]
void correct_function2();
// CHECK: error: The attribute argument must be a single integer.
// CHECK-NEXT: stack_check_size(3.3)

[[stack_check_size(4, 4)]]
void correct_function3();
// CHECK: error: 'stack_check_size' attribute takes one argument
// CHECK-NEXT: stack_check_size(4, 4)

// Правильное использование атрибута - на методе класса
class CorrectClass {
public:
  [[stack_check_size(0)]]
  void correct_method() {
    // CHECK: verbose: Apply attr 'stack_check_size(0)' to CorrectClass::correct_method
  }
};

// Неправильное использование атрибута - на переменной
[[stack_check_size(99 )]]
int incorrect_variable = 0;
// CHECK: error: The attribute 'stack_check_size(99 )' for 'Var' is not applicable.
// CHECK-NEXT: stack_check_size(99 )


// Неправильное использование атрибута - на поле класса
class IncorrectClass {
  [[stack_check_size(0)]]
  int incorrect_field;
  // CHECK: error: The attribute 'stack_check_size(0)' for 'Field' is not applicable.
};

// Неправильное использование атрибута - в пространстве имен
namespace [[stack_check_size(-1)]] IncorrectNamespace {
// CHECK: error: The attribute argument must be a single integer.
// CHECK-NEXT: stack_check_size(-1)
}

// Неправильное использование атрибута - на типе
using InvalidType [[stack_check_limit(77)]] = int;
// CHECK: error: The attribute 'stack_check_limit(77)' for 'TypeAlias' is not applicable.
// CHECK-NEXT: stack_check_limit(77)
