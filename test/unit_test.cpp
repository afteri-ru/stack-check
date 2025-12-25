#include <algorithm>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "stack_info.h"

// Оптимизация чтения размера стека текущего потока
thread_local size_t m_stack_size = 0;
thread_local size_t m_read_counter = 0;
size_t get_stack_size_thread() {
    if (m_stack_size == 0) {
        m_stack_size = get_stack_size();
        m_read_counter++;
    }
    return m_stack_size;
}

// Тест для проверки получения размера стека
TEST(StackInfoTest, GetStackSize) {
    size_t stack_size = get_stack_size_thread();

    // Проверяем, что размер стека больше нуля
    EXPECT_GT(stack_size, 0);

    // Обычно размер стека по умолчанию 8MB или около того
    // Проверяем, что размер разумный (от 1KB до 100MB)
    EXPECT_GE(stack_size, 1024);
    EXPECT_LE(stack_size, 100 * 1024 * 1024);
}

// Тест для проверки получения свободного места на стеке
TEST(StackInfoTest, GetFreeStackSpace) {
    size_t stack_size = get_stack_size_thread();
    size_t free_space = get_free_stack_space();

    EXPECT_EQ(1, m_read_counter);

    // Проверяем, что свободное место не превышает общий размер стека
    EXPECT_LE(free_space, stack_size);

    // Проверяем, что свободное место больше нуля
    EXPECT_GT(free_space, 0);
}

struct StackInfo {
    size_t StackSize;
    size_t FreeSpace;
    size_t Counter;
};

void *test_func_1000(void *info) {
    std::array<uint8_t, 1000> data = {0};
    size_t size = 0;
    for (size_t value : data) {
        size += value;
    }
    if (info) {
        static_cast<StackInfo *>(info)->StackSize = get_stack_size_thread();
        static_cast<StackInfo *>(info)->FreeSpace = get_free_stack_space();
        static_cast<StackInfo *>(info)->Counter = m_read_counter;
    }
    return (void *)size; // prevent possible optimization
}

void *test_func_1000000(void *info) {
    std::array<uint8_t, 1'000'000> data = {0};
    size_t size = 0;
    for (size_t value : data) {
        size += value;
    }
    if (info) {
        static_cast<StackInfo *>(info)->StackSize = get_stack_size_thread();
        static_cast<StackInfo *>(info)->FreeSpace = get_free_stack_space();
        static_cast<StackInfo *>(info)->Counter = m_read_counter;
    }
    return (void *)size; // prevent possible optimization
}

// Тест для проверки работы в разных функциях (использование стека)
TEST(StackInfoTest, StackUsageInFunction) {
    StackInfo info_1000;
    StackInfo info_1000000;

    test_func_1000(&info_1000);
    test_func_1000000(&info_1000000);

    EXPECT_EQ(info_1000.StackSize, get_stack_size());
    EXPECT_EQ(info_1000000.StackSize, get_stack_size());
    EXPECT_EQ(info_1000.StackSize, get_stack_size_thread());
    EXPECT_EQ(info_1000000.StackSize, get_stack_size_thread());

    EXPECT_EQ(1, m_read_counter);

    // Проверяем, что свободное место уменьшилось (или осталось таким же)
    EXPECT_TRUE(info_1000.FreeSpace < get_stack_size_thread());
    EXPECT_TRUE(info_1000000.FreeSpace < get_stack_size_thread());
    EXPECT_TRUE(info_1000000.FreeSpace <= info_1000.FreeSpace) << info_1000000.FreeSpace << "  " << info_1000.FreeSpace;

    EXPECT_EQ(1, m_read_counter);
}

// Тест для проверки работы в отдельном потоке
TEST(StackInfoTest, StackInfoInThread) {
    StackInfo info_1000;
    StackInfo info_1000000;

    test_func_1000((void *)&info_1000);
    test_func_1000000((void *)&info_1000000);

    pthread_attr_t attribute;
    pthread_t thread_1000;
    pthread_t thread_1000000;

    pthread_attr_init(&attribute);
    pthread_attr_setstacksize(&attribute, 1'000'000);
    pthread_create(&thread_1000, &attribute, &test_func_1000, (void *)&info_1000);

    pthread_attr_setstacksize(&attribute, 2'000'000);
    pthread_create(&thread_1000000, &attribute, &test_func_1000000, (void *)&info_1000000);

    pthread_join(thread_1000, 0);
    pthread_join(thread_1000000, 0);

    EXPECT_EQ(info_1000.Counter, 1);
    EXPECT_EQ(info_1000000.Counter, 1);

    EXPECT_EQ(info_1000.StackSize, 1'000'000);
    EXPECT_EQ(info_1000000.StackSize, 2'000'000);

    EXPECT_TRUE(info_1000.FreeSpace < info_1000.StackSize);
    EXPECT_TRUE(info_1000000.FreeSpace < info_1000000.StackSize);
}

size_t recursion(StackInfo &info, size_t count) {
    size_t size = 0;
    std::array<size_t, 1000> data;
    std::fill(data.begin(), data.end(), count);
    for (size_t value : data) {
        size += value;
    }

    if (count) {
        check_stack_overflow(8 * 1000);

        info.FreeSpace = get_free_stack_space();
        info.Counter = count;

        return size + recursion(info, count - 1);
    }
    info.StackSize = get_stack_size_thread();
    info.FreeSpace = get_free_stack_space();
    return 0;
}

// Тест для проверки работы в разных функциях (использование стека)
TEST(StackInfoTest, StackOverflow) {
    StackInfo info_0;
    EXPECT_NO_THROW(recursion(info_0, 0));
    EXPECT_TRUE(info_0.FreeSpace <= info_0.StackSize);

    StackInfo info_1;
    EXPECT_NO_THROW(recursion(info_1, 1));
    EXPECT_TRUE(info_1.FreeSpace <= info_1.StackSize);

    StackInfo info_10;
    EXPECT_NO_THROW(recursion(info_10, 10));
    EXPECT_TRUE(info_10.FreeSpace <= info_10.StackSize);

    try {
        check_stack_overflow(1'000'000'000);
    } catch (stack_overflow &stack) {
        std::cout << stack.what() << "\n";
    }

    StackInfo info_1000000;
    EXPECT_THROW(recursion(info_1000000, 1000000), stack_overflow);
    std::cout << "Recursion with call depth " << info_1000000.Counter << " terminates when there are " << info_1000000.FreeSpace
              << " bytes of free stack space left.\n";
}

// Основная функция для запуска тестов
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
