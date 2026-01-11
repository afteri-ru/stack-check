#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "stack_check.h"

using namespace trust;

/*
The second unit test file will for checks whether the two object files are correctly linked into one program.
*/

// Тест для проверки получения размера стека
TEST(StackInfoTest2, GetStackSize) {
    size_t stack_size = stack_check::get_stack_size();

    // Проверяем, что размер стека больше нуля
    EXPECT_GT(stack_size, 0);

    // Обычно размер стека по умолчанию 8MB или около того
    // Проверяем, что размер разумный (от 1KB до 100MB)
    EXPECT_GE(stack_size, 1024);
    EXPECT_LE(stack_size, 100 * 1024 * 1024);

    void *top;
    void *bottom;
    EXPECT_TRUE(stack_check::get_stack_info(top, bottom));
    EXPECT_TRUE(static_cast<char *>(top) > static_cast<char *>(bottom));
    EXPECT_EQ(static_cast<char *>(top) - static_cast<char *>(bottom), stack_size);

    char *current_frame = static_cast<char *>(__builtin_frame_address(0));

    EXPECT_TRUE(current_frame < static_cast<char *>(top));
    EXPECT_TRUE(current_frame > static_cast<char *>(bottom));

    size_t size = static_cast<char *>(top) - static_cast<char *>(bottom);
    EXPECT_GE(size, 1024);
    EXPECT_LE(size, 100 * 1024 * 1024);
}

// Тест для проверки получения свободного места на стеке
TEST(StackInfoTest2, GetFreeStackSpace) {
    size_t stack_size = stack_check::get_stack_size();
    size_t free_space = stack_check::get_free_stack_space(); // get_staget_free_stack_space();

    // Проверяем, что свободное место не превышает общий размер стека
    EXPECT_LE(free_space, stack_size);

    // Проверяем, что свободное место больше нуля
    EXPECT_GT(free_space, 0);

    char *current_frame = static_cast<char *>(__builtin_frame_address(0));

    EXPECT_TRUE(current_frame < stack_check::info.top);
    EXPECT_TRUE(current_frame > stack_check::info.bottom);

    EXPECT_EQ(stack_size, static_cast<char *>(stack_check::info.top) - static_cast<char *>(stack_check::info.bottom));
    stack_size = static_cast<char *>(stack_check::info.top) - static_cast<char *>(stack_check::info.bottom);
    free_space = static_cast<char *>(current_frame) - static_cast<char *>(stack_check::info.bottom);

    EXPECT_GT(free_space, 0);
    EXPECT_LE(free_space, stack_size);

    try {
        stack_check::check_overflow(stack_size + 1);
        FAIL();
    } catch (stack_overflow &stack) {
        EXPECT_EQ(stack.size, stack_size + 1);
        ASSERT_TRUE(stack.info);
        EXPECT_TRUE(static_cast<char *>(stack.info->frame) < static_cast<char *>(stack_check::info.top));
        EXPECT_TRUE(static_cast<char *>(stack.info->frame) > static_cast<char *>(stack_check::info.bottom));
        EXPECT_TRUE(static_cast<char *>(stack.info->frame) < static_cast<char *>(stack_check::info.bottom) + stack_size + 1);
    }
}

[[clang::optnone]] int func_large_stack() {
    char data[2'000'000] = {0};
    return 0;
}

[[clang::optnone]] int func_dyn_stack(const size_t size) {
    char data[size];
    for (size_t i = 0; i < size; i++) {
        data[i] = i;
    }
    return 0;
}

TEST(StackSizesSection, GetFreeStackSpace) {
    trust::StackSizesSection section;
    trust::AddrListType all_list = section.getAddrList();

    ASSERT_TRUE(all_list.size() > 0);

    try {
        std::vector<void *> incude = {nullptr};
        const trust::stack_check test(&incude);
        FAIL();
    } catch (std::runtime_error &err) {
    }

    try {
        std::vector<void *> incude = {(void *)&func_large_stack};
        const trust::stack_check test(&incude);
    } catch (std::runtime_error &err) {
        FAIL();
    }

    bool found;
    size_t size = section.getStackSize((void *)&func_large_stack, &found);
    ASSERT_TRUE(found);
    EXPECT_LE(2'000'000, size);

    ASSERT_EQ(0, section.getStackSize((void *)&func_dyn_stack, &found));
    ASSERT_FALSE(found);

    uint64_t stack_max = trust::stack_check::get_stack_limit();
    EXPECT_LE(2'000'000, stack_max);

    uint64_t stack_min = trust::stack_check::get_stack_limit(nullptr, &all_list);
    EXPECT_EQ(0, stack_min);

    trust::AddrListType exclude = {(void *)&func_large_stack};
    uint64_t stack_without_large = trust::stack_check::get_stack_limit(nullptr, &exclude);
    EXPECT_LE(stack_min, stack_without_large);
    EXPECT_GT(stack_max, stack_without_large);
}