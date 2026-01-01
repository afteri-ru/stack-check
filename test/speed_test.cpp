#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "stack_check.h"

using namespace trust;

bool isNumber(const std::string &str, long &num) {
    if (str.empty())
        return false;

    // Проверяем, является ли строка числом
    char *end;
    num = std::strtol(str.c_str(), &end, 10);

    // Если end указывает на конец строки, значит вся строка - число
    // Также проверяем, что число больше нуля
    return (end != str.c_str() && *end == '\0' && num > 0);
}

size_t call_depth = 0;
size_t GetMaxDepth();

void TrustRecursion(size_t depth);
void UntrustRecursion(size_t depth);

int main(int argc, char *argv[]) {
    // Если передано больше одного аргумента (помимо имени программы)
    if (argc > 2) {
        std::cerr << "Using: speed_test <depth>\nBy default, the maximum possible call depth is used." << std::endl;
        return 1;
    }

    if (argc == 1) {
        try {
            call_depth = 0;
            TrustRecursion(-1);
        } catch (stack_overflow &info) {
            std::cout << "Recursive call depth used " << call_depth << std::endl;
        }
    }

    long num = call_depth - 1;

    if (argc == 2) {

        std::string arg = argv[1];

        // Проверяем, является ли аргумент числом больше нуля
        if (isNumber(arg, num)) {
            // Преобразуем аргумент в число и выводим его
            std::cout << num << std::endl;
        } else {
            // Если аргумент не число или число не больше нуля, выводим сообщение об ошибке
            std::cerr << "Ошибка: Аргумент должен быть целым числом больше нуля." << std::endl;
            return 1;
        }
    }

    auto start_trust = std::chrono::high_resolution_clock::now();
    TrustRecursion(num);
    auto end_trust = std::chrono::high_resolution_clock::now();
    auto trust_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_trust - start_trust);

    auto start_untrust = std::chrono::high_resolution_clock::now();
    UntrustRecursion(num);
    auto end_untrust = std::chrono::high_resolution_clock::now();
    auto untrust_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_untrust - start_untrust);

    std::cout << "TrustRecursion:   " << trust_duration.count() << " nanosecs" << std::endl;
    std::cout << "UntrustRecursion: " << untrust_duration.count() << " nanosecs" << std::endl;
    std::cout << "Stack overflow protection reduces function call speed: "
              << (trust_duration.count() - untrust_duration.count()) * 100.0 / untrust_duration.count() << "%" << std::endl;

    return 0;
}

size_t GetMaxDepth() {
    // sddd
    return 100;
}

void TrustRecursion(size_t depth) {
    call_depth++;
    if (!depth) {
        return;
    }
    stack_check::check_overflow(4500);
    if (call_depth > 10'000'000) {
        std::cout << "Oops. Very good optimization!\n";
        return;
    }
    TrustRecursion(depth - 1);
}

void UntrustRecursion(size_t depth) {
    call_depth++;
    if (!depth) {
        return;
    }
    if (call_depth > 10'000'000) {
        std::cout << "Oops. Very good optimization!\n";
        return;
    }
    UntrustRecursion(depth - 1);
}
