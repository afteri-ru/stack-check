#ifndef STACK_INFO_H
#define STACK_INFO_H

#include <cstddef>
#include <stdexcept>

struct stack_info;
struct stack_overflow : public std::runtime_error {
    size_t m_size;
    volatile void *m_frame;
    stack_overflow(size_t size, volatile void *frame) : std::runtime_error("Stack overflow"), m_size(size), m_frame(frame) {}
};

struct stack_info {
    char *top;
    char *bottom;
    char *frame;

    static thread_local stack_info addr;

    stack_info() : top(nullptr), bottom(nullptr), frame(nullptr) { get_stack_info(addr.top, addr.bottom); }

    static bool get_stack_info(char *&top, char *&bottom);

    static inline size_t get_stack_size() { return addr.top - addr.bottom; }

    static inline size_t get_free_stack_space() {
        if (__builtin_frame_address(0) > addr.bottom) {
            return (char *)__builtin_frame_address(0) - addr.bottom;
        }
        return 0;
    }

    static inline void check_overflow(const size_t size) {
        char *current_frame = static_cast<char *>(__builtin_frame_address(0));
        if (current_frame < (addr.bottom + size)) {
            addr.frame = current_frame;
            throw stack_overflow(size, current_frame);
        }
    }
};

#endif // STACK_INFO_H
