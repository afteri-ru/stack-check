#include <algorithm>
#include <gtest/gtest.h>

#include <array>
#include <iostream>

#include "stack_info.h"


// Тест для проверки получения размера стека
TEST(StackInfoTest, GetStackSize) {
    size_t stack_size = stack_info::get_stack_size();

    // Проверяем, что размер стека больше нуля
    EXPECT_GT(stack_size, 0);

    // Обычно размер стека по умолчанию 8MB или около того
    // Проверяем, что размер разумный (от 1KB до 100MB)
    EXPECT_GE(stack_size, 1024);
    EXPECT_LE(stack_size, 100 * 1024 * 1024);

    char *top;
    char *bottom;
    EXPECT_TRUE(stack_info::get_stack_info(top, bottom));
    EXPECT_TRUE(top > bottom);
    EXPECT_EQ(top - bottom, stack_size);

    char *current_frame = static_cast<char *>(__builtin_frame_address(0));
    
    EXPECT_TRUE(current_frame < top);
    EXPECT_TRUE(current_frame > bottom);

    size_t size = top - bottom;
    EXPECT_GE(size, 1024);
    EXPECT_LE(size, 100 * 1024 * 1024);
}

// Тест для проверки получения свободного места на стеке
TEST(StackInfoTest, GetFreeStackSpace) {
    size_t stack_size = stack_info::get_stack_size();
    size_t free_space = stack_info::get_free_stack_space();//get_staget_free_stack_space();

    // Проверяем, что свободное место не превышает общий размер стека
    EXPECT_LE(free_space, stack_size);

    // Проверяем, что свободное место больше нуля
    EXPECT_GT(free_space, 0);

    char *current_frame = static_cast<char *>(__builtin_frame_address(0));

    EXPECT_TRUE(current_frame < stack_info::addr.top);
    EXPECT_TRUE(current_frame > stack_info::addr.bottom);

    EXPECT_EQ(stack_size, stack_info::addr.top - stack_info::addr.bottom);
    stack_size = stack_info::addr.top - stack_info::addr.bottom;
    free_space = current_frame - stack_info::addr.bottom;

    EXPECT_GT(free_space, 0);
    EXPECT_LE(free_space, stack_size);

    try {
        stack_info::check_overflow(stack_size + 1);
        FAIL();
    } catch (stack_overflow &stack) {
        EXPECT_EQ(stack.m_size, stack_size + 1);
        EXPECT_TRUE(stack.m_frame < stack_info::addr.top);
        EXPECT_TRUE(stack.m_frame > stack_info::addr.bottom);
        EXPECT_TRUE(stack.m_frame < stack_info::addr.bottom + stack_size + 1);
    }
}

struct StackInfoTest {
    size_t StackSize;
    size_t FreeSpace;
};

void *test_func_1000(void *info) {
    std::array<uint8_t, 1000> data = {0};
    size_t size = 0;
    for (size_t value : data) {
        size += value;
    }
    if (info) {
        static_cast<StackInfoTest *>(info)->StackSize = stack_info::get_stack_size();//get_stack_size_thread();
        static_cast<StackInfoTest *>(info)->FreeSpace = stack_info::get_free_stack_space();//get_free_stack_space();
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
        static_cast<StackInfoTest *>(info)->StackSize = stack_info::get_stack_size();//get_stack_size_thread();
        static_cast<StackInfoTest *>(info)->FreeSpace = stack_info::get_free_stack_space();//get_free_stack_space();
    }
    return (void *)size; // prevent possible optimization
}

// Тест для проверки работы в разных функциях (использование стека)
TEST(StackInfoTest, StackUsageInFunction) {
    StackInfoTest info_1000;
    StackInfoTest info_1000000;

    test_func_1000(&info_1000);
    test_func_1000000(&info_1000000);

    EXPECT_EQ(info_1000.StackSize, stack_info::get_stack_size());
    EXPECT_EQ(info_1000000.StackSize, stack_info::get_stack_size());

    // Проверяем, что свободное место уменьшилось (или осталось таким же)
    EXPECT_TRUE(info_1000.FreeSpace < stack_info::get_stack_size());
    EXPECT_TRUE(info_1000000.FreeSpace < stack_info::get_stack_size());
    EXPECT_TRUE(info_1000000.FreeSpace <= info_1000.FreeSpace) << info_1000000.FreeSpace << "  " << info_1000.FreeSpace;
}

// Тест для проверки работы в отдельном потоке
TEST(StackInfoTest, StackInfoInThread) {
    StackInfoTest info_1000;
    StackInfoTest info_1000000;

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

    EXPECT_EQ(info_1000.StackSize, 1'000'000);
    EXPECT_EQ(info_1000000.StackSize, 2'000'000);

    EXPECT_TRUE(info_1000.FreeSpace < info_1000.StackSize);
    EXPECT_TRUE(info_1000000.FreeSpace < info_1000000.StackSize);
}

size_t recursion(StackInfoTest &info, size_t count) {
    size_t size = 0;
    std::array<size_t, 1000> data;
    std::fill(data.begin(), data.end(), count);
    for (size_t value : data) {
        size += value;
    }

    if (count) {
        stack_info::check_overflow(8 * 1000);

        info.FreeSpace = static_cast<char *>(__builtin_frame_address(0)) - stack_info::addr.bottom;

        return size + recursion(info, count - 1);
    }
    char * top;
    info.StackSize = stack_info::addr.top - stack_info::addr.bottom;
    info.FreeSpace = static_cast<char *>(__builtin_frame_address(0)) - stack_info::addr.bottom;
    return 0;
}

// Тест для проверки работы в разных функциях (использование стека)
TEST(StackInfoTest, StackOverflow) {
    StackInfoTest info_0;
    EXPECT_NO_THROW(recursion(info_0, 0));
    EXPECT_TRUE(info_0.FreeSpace <= info_0.StackSize);

    StackInfoTest info_1;
    EXPECT_NO_THROW(recursion(info_1, 1));
    EXPECT_TRUE(info_1.FreeSpace <= info_1.StackSize);

    StackInfoTest info_10;
    EXPECT_NO_THROW(recursion(info_10, 10));
    EXPECT_TRUE(info_10.FreeSpace <= info_10.StackSize);

    try {
        stack_info::check_overflow(1'000'000'000);
    } catch (stack_overflow &stack) {
        std::cout << stack.what() << "\n";
    }

    // StackInfoTest info_1000000;
    // EXPECT_THROW(recursion(info_1000000, 1000000), stack_overflow);
    // std::cout << "Recursion with call depth " << info_1000000.Counter << " terminates when there are " << info_1000000.FreeSpace
    //           << " bytes of free stack space left.\n";
}

// Основная функция для запуска тестов
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
