#ifndef STACK_CHECK_H
#define STACK_CHECK_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace trust {

struct stack_info;
struct stack_overflow : public std::runtime_error {
    size_t m_size;
    void *m_frame;
    stack_overflow(size_t size, void *frame) : std::runtime_error("Stack overflow"), m_size(size), m_frame(frame) {}
};

struct stack_info {
    void *top;
    void *bottom;
    void *frame;

    static thread_local stack_info addr;

    stack_info() : top(nullptr), bottom(nullptr), frame(nullptr) { get_stack_info(addr.top, addr.bottom); }

    static bool get_stack_info(void *&top, void *&bottom);

    static inline size_t get_stack_size() { return static_cast<char *>(addr.top) - static_cast<char *>(addr.bottom); }

    static inline size_t get_free_stack_space() {
        if (static_cast<char *>(__builtin_frame_address(0)) > static_cast<char *>(addr.bottom)) {
            return static_cast<char *>(__builtin_frame_address(0)) - static_cast<char *>(addr.bottom);
        }
        return 0;
    }

    static inline void check_overflow(const size_t size) {
        void *current_frame = static_cast<void *>(__builtin_frame_address(0));
        if (static_cast<char *>(current_frame) < (static_cast<char *>(addr.bottom) + size)) {
            addr.frame = current_frame;
            throw stack_overflow(size, current_frame);
        }
    }
};

}; // namespace trust

#endif // STACK_CHECK_H
