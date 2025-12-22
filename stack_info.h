#ifndef STACK_INFO_H
#define STACK_INFO_H

#include <cstddef>

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

#endif // STACK_INFO_H
