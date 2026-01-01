#ifndef STACK_CHECK_H
#define STACK_CHECK_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#if defined __has_attribute

#if __has_attribute(trust)

#define STACK_CHECK(...) [[trust("stack_check" __VA_OPT__(,) __VA_ARGS__)]]
#define STACK_CHECK_LIMIT(value) [[trust("stack_check_limit", value)]] trust::stack_check::check_limit()
#define STACK_CHECK_IGNORE_NEXT(value) trust::stack_check::ignore_next_check(value)

#elif __has_attribute(stack_check)

#define STACK_CHECK(...) [[stack_check(__VA_ARGS__)]]
#define STACK_CHECK_LIMIT(value) [[stack_check_limit(value)]] trust::stack_check::check_limit()
#define STACK_CHECK_IGNORE_NEXT(value) trust::stack_check::ignore_next_check(value)

#else // plugins not loaded
#define STACK_CHECK(...)                                                                                                                   \
    static_assert(!"The 'stack_check' attribute is not supported. Run the compiler with the 'stack_check' or 'trusted-cpp' plugins.");
#define STACK_CHECK_LIMIT(value)                                                                                                           \
    static_assert(!"The 'stack_check_limit' attribute is not supported. Run the compiler with the 'stack_check' or 'trusted-cpp' plugins.");
#define STACK_CHECK_IGNORE_NEXT(value)                                                                                                    \
    static_assert(!"The 'stack_check' attribute is not supported. Run the compiler with the 'stack_check' or 'trusted-cpp' plugins.");
#endif

#else // #if defined __has_attribute
#define STACK_CHECK(...) static_assert(!"The __has_attribute macro is not supported.");
#define STACK_CHECK_LIMIT static_assert(!"The __has_attribute macro is not supported.");
#define STACK_CHECK_IGNORE_NEXT static_assert(!"The __has_attribute macro is not supported.");
#endif

namespace trust {

struct stack_check;
struct stack_overflow : public std::runtime_error {
    size_t size;
    const stack_check * info;
    stack_overflow(size_t size, const stack_check *stack) : std::runtime_error("Stack overflow"), size(size), info(stack) {}
};

struct stack_check {
    void *top;
    void *bottom;
    void *check;
    void *frame;
    const size_t limit;

    static const thread_local stack_check info;

    stack_check(const size_t check_limit=4096) : top(nullptr), bottom(nullptr), check(nullptr), frame(nullptr), limit(0) { 
        update(check_limit); }

    static bool get_stack_info(void *&top, void *&bottom);
    
    static inline const stack_check *update(const size_t check_limit) {
        get_stack_info(const_cast<stack_check *>(&info)->top, const_cast<stack_check *>(&info)->bottom);
        *const_cast<size_t *>(&info.limit) = check_limit;
        *const_cast<void **>(&info.check) = static_cast<char *>(info.bottom) + check_limit;
        return &info;
    }

    static inline size_t get_stack_size() { return static_cast<char *>(info.top) - static_cast<char *>(info.bottom); }

    static inline size_t get_free_stack_space() {
        if (static_cast<char *>(__builtin_frame_address(0)) > static_cast<char *>(info.bottom)) {
            return static_cast<char *>(__builtin_frame_address(0)) - static_cast<char *>(info.bottom);
        }
        return 0;
    }

    static inline void check_overflow(const size_t size) {
        void *current_frame = static_cast<void *>(__builtin_frame_address(0));
        if (static_cast<char *>(current_frame) < (static_cast<char *>(info.bottom) + size)) {
            *const_cast<void **>(&info.frame) = current_frame;
            throw stack_overflow(size, &info);
        }
    }

    static inline void check_limit() {
        void *current_frame = static_cast<void *>(__builtin_frame_address(0));
        // No need for addition operator before comparison and more opportunities for optimization
        if (static_cast<char *>(current_frame) < (static_cast<char *>(info.check))) {
            *const_cast<void **>(&info.frame) = current_frame;
            throw stack_overflow(info.limit, &info);
        }
    }
    
    [[clang::optnone]] static void ignore_next_check(const size_t size) {}

};


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

#include <pthread.h>

inline bool stack_check::get_stack_info(void *&top, void *&bottom) {
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);

    size_t stack_size;
    pthread_attr_getstack(&attr, (void **)&bottom, &stack_size);

    pthread_attr_destroy(&attr);

    top = static_cast<char *>(bottom) + stack_size;

    return top > bottom;
}

#endif

}; // namespace trust


#endif // STACK_CHECK_H
