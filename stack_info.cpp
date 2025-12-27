#include "stack_info.h"
#include <cstddef>

#ifdef _WIN32
// Windows implementation
#include <thread>
#include <windows.h>

size_t get_stack_size() {
    ULONG_PTR low_limit, high_limit;
    GetCurrentThreadStackLimits(&low_limit, &high_limit);
    return static_cast<size_t>(high_limit - low_limit);
}

size_t get_free_stack_space() {
    ULONG_PTR low_limit, high_limit;
    GetCurrentThreadStackLimits(&low_limit, &high_limit);

    // Получаем текущий указатель стека
    void *stack_ptr = &stack_ptr;

    // Вычисляем свободное место
    return static_cast<size_t>(reinterpret_cast<ULONG_PTR>(stack_ptr) - low_limit);
}

#else
// Linux implementation
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

size_t get_stack_size() {
    pthread_attr_t attr;
    void *stack_addr;
    size_t stack_size;

    // Получаем атрибуты текущего потока
    pthread_getattr_np(pthread_self(), &attr);

    // Получаем адрес и размер стека
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);

    // Освобождаем атрибуты
    pthread_attr_destroy(&attr);

    return stack_size;
}

size_t get_free_stack_space() {
    pthread_attr_t attr;
    void *stack_addr;
    size_t stack_size;
    void *current_addr = __builtin_frame_address(0);

    // Получаем атрибуты текущего потока
    pthread_getattr_np(pthread_self(), &attr);

    // Получаем адрес и размер стека
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);

    // Освобождаем атрибуты
    pthread_attr_destroy(&attr);

    // Вычисляем свободное место на стеке
    // Стек растет вниз, поэтому вычисляем разницу между текущим адресом и нижней границей стека
    char *stack_bottom = static_cast<char *>(stack_addr);
    char *current_ptr = static_cast<char *>(current_addr);

    if (current_ptr >= stack_bottom && current_ptr < stack_bottom + stack_size) {
        return static_cast<size_t>(current_ptr - stack_bottom);
    }

    return 0;
}

thread_local stack_info stack_info::addr;

bool stack_info::get_stack_info(char *&top, char *&bottom) {
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);

    size_t stack_size;
    pthread_attr_getstack(&attr, (void **)&bottom, &stack_size);

    pthread_attr_destroy(&attr);

    top = static_cast<char *>(bottom) + stack_size;

    return top > bottom;
}

#endif
