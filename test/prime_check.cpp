#include <chrono>
#include <cstdlib>
#include <gmpxx.h>
#include <iostream>
#include <vector>

#include "stack_check.h"

using namespace trust;

// Глобальные переменные для отслеживания глубины рекурсии и количества вызовов
int currentDepth = 0;
int maxDepth = 0;
int callCount = 0;

int currentDepth_safe = 0;
int maxDepth_safe = 0;
int callCount_safe = 0;

// Рекурсивная функция для проверки, является ли число простым
// n - проверяемое число
// divisor - текущий делитель, с которого начинаем проверку
bool isPrime(const mpz_class &n, const mpz_class &divisor = 2) {
    // Увеличиваем счетчик вызовов
    callCount++;

    // Увеличиваем текущую глубину рекурсии
    currentDepth++;

    // Обновляем максимальную глубину рекурсии
    if (currentDepth > maxDepth) {
        maxDepth = currentDepth;
    }

    // Базовые случаи
    // Если число меньше 2, оно не является простым
    if (n < 2) {
        currentDepth--;
        return false;
    }

    // Если квадрат делителя больше n, значит мы проверили все возможные делители
    // и число является простым
    mpz_class divSquare = divisor * divisor;
    if (divSquare > n) {
        currentDepth--;
        return true;
    }

    // Если n делится на divisor, то число не является простым
    if (n % divisor == 0) {
        currentDepth--;
        return false;
    }

    // Рекурсивный вызов с увеличенным делителем
    bool result = isPrime(n, divisor + 1);

    // Уменьшаем текущую глубину рекурсии при возврате
    currentDepth--;

    return result;
}

// Рекурсивная функция для проверки, является ли число простым
// n - проверяемое число
// divisor - текущий делитель, с которого начинаем проверку
bool isPrimeSafe(const mpz_class &n, const mpz_class &divisor = 2) {
    // Увеличиваем счетчик вызовов
    callCount_safe++;

    // Увеличиваем текущую глубину рекурсии
    currentDepth_safe++;

    // Обновляем максимальную глубину рекурсии
    if (currentDepth_safe > maxDepth_safe) {
        maxDepth_safe = currentDepth_safe;
    }

    // Базовые случаи
    // Если число меньше 2, оно не является простым
    if (n < 2) {
        currentDepth_safe--;
        return false;
    }

    // Если квадрат делителя больше n, значит мы проверили все возможные делители
    // и число является простым
    mpz_class divSquare = divisor * divisor;
    if (divSquare > n) {
        currentDepth_safe--;
        return true;
    }

    // Если n делится на divisor, то число не является простым
    if (n % divisor == 0) {
        currentDepth_safe--;
        return false;
    }

    // Рекурсивный вызов с увеличенным делителем
    stack_info::check_overflow(10000);
    bool result = isPrimeSafe(n, divisor + 1);

    // Уменьшаем текущую глубину рекурсии при возврате
    currentDepth_safe--;

    return result;
}

int main(int argc, char *argv[]) {
    // Проверяем, что передан хотя бы один аргумент командной строки
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <start_number> [count]" << std::endl;
        return 1;
    }

    // Преобразуем первый аргумент в число (начальное число)
    mpz_class startNumber;
    if (mpz_set_str(startNumber.get_mpz_t(), argv[1], 10) != 0) {
        std::cerr << "Error: Invalid start number format" << std::endl;
        return 1;
    }

    // Проверяем, что начальное число неотрицательное
    if (startNumber < 0) {
        std::cerr << "Error: Start number must be non-negative" << std::endl;
        return 1;
    }

    // Преобразуем второй аргумент в число (количество простых чисел для поиска)
    int count = 10; // По умолчанию ищем 10 простых чисел
    if (argc >= 3) {
        char *endPtr;
        long countArg = std::strtol(argv[2], &endPtr, 10);

        // Проверяем, что преобразование прошло успешно
        if (*endPtr != '\0') {
            std::cerr << "Error: Invalid count format" << std::endl;
            return 1;
        }

        // Проверяем, что countArg положительное
        if (countArg <= 0) {
            std::cerr << "Error: Count must be positive" << std::endl;
            return 1;
        }

        count = static_cast<int>(countArg);
    }

    std::vector<mpz_class> output;
    output.reserve(count);

    // ---------------------------------------------------------------------
    // Warming up memory and cache
    int foundCount = 0;
    mpz_class number = startNumber;
    try {
        while (foundCount < count) {
            bool isNumberPrime = isPrimeSafe(number);

            // Выводим простые числа
            if (isNumberPrime) {
                output.push_back(number);
                foundCount++;
            }

            number++;
        }

    } catch (stack_overflow &stack) {
        std::cout << "Stack overflow exception at: " << maxDepth_safe << " call depth." << std::endl;
        std::cout << "Stack top: " << stack_info::addr.top << " bottom: " << stack_info::addr.bottom
                  << " (stack size: " << stack_info::get_stack_size() << ")" << std::endl;
        std::cout << "Query size: " << stack.m_size << " end frame: " << stack.m_frame
                  << " (free space: " << (static_cast<char *>(stack.m_frame) - static_cast<char *>(stack_info::addr.bottom)) << ")"
                  << std::endl;
        return 1;
    }

    // ---------------------------------------------------------------------

    // Засекаем время начала выполнения
    auto start_safe = std::chrono::high_resolution_clock::now();

    // Ищем заданное количество простых чисел, начиная с начального
    foundCount = 0;
    number = startNumber;

    try {
        while (foundCount < count) {
            bool isNumberPrime = isPrimeSafe(number);

            // Выводим простые числа
            if (isNumberPrime) {
                output.push_back(number);
                foundCount++;
            }

            number++;
        }

    } catch (stack_overflow &stack) {
        std::cout << "Stack overflow exception at: " << maxDepth_safe << " call depth." << std::endl;
        std::cout << "Stack top: " << stack_info::addr.top << " bottom: " << stack_info::addr.bottom
                  << " (stack size: " << stack_info::get_stack_size() << ")" << std::endl;
        std::cout << "Query size: " << stack.m_size << " end frame: " << stack.m_frame
                  << " (free space: " << (static_cast<char *>(stack.m_frame) - static_cast<char *>(stack_info::addr.bottom)) << ")"
                  << std::endl;
        return 1;
    }

    // Засекаем время окончания выполнения
    auto end_safe = std::chrono::high_resolution_clock::now();

    // Вычисляем время выполнения
    auto duration_safe = std::chrono::duration_cast<std::chrono::microseconds>(end_safe - start_safe);

    // ---------------------------------------------------------------------
    std::cout << "\n";

    for (auto value : output) {
        std::cout << value << std::endl;
    }
    output.clear();
    output.reserve(count);

    // Засекаем время начала выполнения
    auto start = std::chrono::high_resolution_clock::now();

    // Ищем заданное количество простых чисел, начиная с начального
    foundCount = 0;
    number = startNumber;

    while (foundCount < count) {
        bool isNumberPrime = isPrime(number);

        // Выводим простые числа
        if (isNumberPrime) {
            output.push_back(number);
            foundCount++;
        }

        number++;
    }

    // Засекаем время окончания выполнения
    auto end = std::chrono::high_resolution_clock::now();

    // Вычисляем время выполнения
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    for (auto value : output) {
        std::cout << value << std::endl;
    }
    // ---------------------------------------------------------------------

    // Выводим максимальную глубину рекурсии, количество вызовов и время выполнения
    std::cout << "Max recursion depth SAFE: " << maxDepth_safe << std::endl;
    std::cout << "Number of recursive calls SAFE: " << callCount_safe << std::endl;
    std::cout << "Execution time SAFE: " << duration_safe.count() << " microseconds" << std::endl;

    std::cout << "\n";

    // Выводим максимальную глубину рекурсии, количество вызовов и время выполнения
    std::cout << "Max recursion depth: " << maxDepth << std::endl;
    std::cout << "Number of recursive calls: " << callCount << std::endl;
    std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;

    std::cout << "Difference in execution time: " << 100.0 * (duration_safe.count() - duration.count()) / duration.count() << " %"
              << std::endl;

    return 0;
}
