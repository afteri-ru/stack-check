// RUN: %clangxx -std=c++20 %s -o frame-test-O0 -O0 && \
// RUN: ./frame-test-O0 | FileCheck %s -check-prefix=O0

// RUN: %clangxx -std=c++20 %s -o frame-test-O1 -O1 &&  \
// RUN: ./frame-test-O1 | FileCheck %s -check-prefix=O1

// RUN: %clangxx -std=c++20 %s -o frame-test-O2 -O2 && \
// RUN: ./frame-test-O2 | FileCheck %s -check-prefix=O2

// RUN: %clangxx -std=c++20 %s -o frame-test-O3 -O3 && \
// RUN: ./frame-test-O3 | FileCheck %s -check-prefix=O3

//
// Without optimization, different functions have different stack frames.
//

// O0-NOT: FRAME EQUAL!!!!
// O0-NOT: STACK EQUAL!!!

//
// When compiling with optimization, the stack of different functions may be in the same frame,
// which means that __builtin_frame_address(0) will point to the same memory address
//

// O1-NOT: FRAME EQUAL!!!!
// O1-NOT: STACK EQUAL!!!

// O2: FRAME EQUAL!!!!
// O2: STACK EQUAL!!!

// O3: FRAME EQUAL!!!!
// O3: STACK EQUAL!!!

#include <cstdlib>
#include <iostream>

void *stack_pointer() {
    void *stack_pointer_value = 0;

#if defined(_MSC_VER)
    //--- Syntax for Microsoft Visual C++ ---
    __asm {
#if defined(_WIN64)
        // 64-bit system, use the RSP register
        mov stack_pointer_value, rsp
#else
        // 32-bit system, use the ESP register
        mov stack_pointer_value, esp
#endif
    }

#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__)
    // 64-bit system, use the RSP register
    asm volatile("movq %%rsp, %0"
                 : "=r"(stack_pointer_value) // %0 is the output operand, "=r" is in any general-purpose register
    );
#else
    asm volatile("movl %%esp, %0" : "=r"(stack_pointer_value));
#endif

#else
#error "Unsupported compiler for inline assembler!"
#endif
    return stack_pointer_value;
}

void *frame_main = nullptr;
void *frame_func = nullptr;

void *stack_main = 0;
void *stack_func = 0;

void *func(size_t &count) {
    if (count) {
        count--;
        return func(count);
    }
    frame_func = __builtin_frame_address(0);
    return stack_pointer();
}

int main(int argc, char *argv[]) {
    frame_main = __builtin_frame_address(0);
    stack_main = stack_pointer();

    size_t counter = 1000;
    stack_func = func(counter);

    std::cout << "frame_main: " << frame_main << " frame_func: " << frame_func << (frame_func == frame_main ? " FRAME EQUAL!!!!\n" : "\n");
    std::cout << "stack_main: " << stack_main << " stack_func: " << stack_func << (stack_main == stack_func ? " STACK EQUAL!!!\n!" : "\n");
    return 0;
}
