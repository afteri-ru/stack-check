#ifndef STACK_INFO_H
#define STACK_INFO_H

#include <cstddef>
#include <format>
#include <stdexcept>

/**
 * @brief Получает общий размер стека для текущего потока
 * @return Размер стека в байтах
 */
size_t get_stack_size();

/**
 * @brief Получает размер свободного места на стеке
 * @return Размер свободного места в байтах
 */
size_t get_free_stack_space();

/**
 * @brief Проверяет размер свободного места на стеке
 * и вызывает исключение @ref stack_overflow, если он меньше указанного
 * @return Размер свободного места в байтах
 */
class stack_overflow : public std::runtime_error {
  public:
    stack_overflow(size_t size)
        : std::runtime_error(std::format("Stack overflow. {} bytes of stack space required, but only {} of {} are available.", size,
                                         get_free_stack_space(), get_stack_size())) {}
};

inline void check_stack_overflow(size_t size) {
    // Stack overflow. 1000000000 bytes of stack space required, but only 8380288 of 8384512 are available.(4224)
    // Recursion with call depth 998968 terminates when there are 17296 bytes of free stack space left.
    // Stack overflow. 1000000000 bytes of stack space required, but only 8377472 of 8380416 are available.(2944)
    // Recursion with call depth 998964 terminates when there are 15952 bytes of free stack space left.
    if (get_free_stack_space() < (size + 4500)) {
        throw stack_overflow(size);
    }
}

#endif // STACK_INFO_H
