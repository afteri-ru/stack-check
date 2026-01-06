// RUN: %clangxx -I%shlibdir -std=c++20 -Xclang -load -Xclang %shlibdir/stack_check_clang.so -I.. \
// RUN: -Xclang -add-plugin -Xclang stack_check -lpthread %s

#include "stack_check.h"

using namespace trust;

const thread_local stack_check stack_check::info;

// Функция без автоматической проверки стека от переполнения
int func() { return 0; }

// Перед каждым вызовом функции будет вставлен код проверки указанного свободного места на стеке
[[stack_check_size(100)]]
int guard_size_func() {
    char data[92];
    return 0;
}

// Перед каждым вызовом функции будет проверяться минимальный размер свободного места на стеке
STACK_CHECK_LIMIT
int guard_limit_func() { return 0; }

int main() {

    // Тут будет автоматически добавлен код для контроля стека от переполнения
    guard_size_func();

    stack_check::ignore_next_check(1); // Следующая автоматическая вставка проверки стека будет проигнорирована
    guard_size_func();

    // Тут будет автоматически добавлен код для проверки минимального размера свободного места на стеке
    guard_limit_func();

    stack_check::check_overflow(10000); // Ручная проверка свободного места на стеке
    func();

    return 0;
}