#include <iostream>
#include <chrono>
#include <cstdlib>
#include <gmpxx.h>

// Глобальные переменные для отслеживания глубины рекурсии и количества вызовов
int currentDepth = 0;
int maxDepth = 0;
int callCount = 0;

// Рекурсивная функция для проверки, является ли число простым
// n - проверяемое число
// divisor - текущий делитель, с которого начинаем проверку
bool isPrime(const mpz_class& n, const mpz_class& divisor = 2) {
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

int main(int argc, char* argv[]) {
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
        char* endPtr;
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
    
    // Засекаем время начала выполнения
    auto start = std::chrono::high_resolution_clock::now();
    
    // Ищем заданное количество простых чисел, начиная с начального
    int foundCount = 0;
    mpz_class number = startNumber;
    
    while (foundCount < count) {
        bool isNumberPrime = isPrime(number);
        
        // Выводим простые числа
        if (isNumberPrime) {
            std::cout << number << std::endl;
            foundCount++;
        }
        
        number++;
    }
    
    // Засекаем время окончания выполнения
    auto end = std::chrono::high_resolution_clock::now();
    
    // Вычисляем время выполнения
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Выводим максимальную глубину рекурсии, количество вызовов и время выполнения
    std::cout << "Max recursion depth: " << maxDepth << std::endl;
    std::cout << "Number of recursive calls: " << callCount << std::endl;
    std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;
    
    return 0;
}
