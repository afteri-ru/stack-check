cmake_minimum_required(VERSION 3.14)
project(StackInfo)

# Установка стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Установка компилятора clang
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

# Опции компиляции
set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic -O0")

# Поиск Google Test
find_package(GTest REQUIRED)

# Создание библиотеки stack_info
add_library(stack_info stack_info.cpp)

# Создание исполняемого файла для тестов
add_executable(stack_info_test test.cpp)

# Линковка библиотек
target_link_libraries(stack_info_test stack_info GTest::gtest GTest::gtest_main pthread)

# Установка свойств для Linux
if(UNIX AND NOT APPLE)
    target_compile_definitions(stack_info PRIVATE LINUX)
endif()

# Установка свойств для Windows
if(WIN32)
    target_compile_definitions(stack_info PRIVATE _WIN32)
endif()
