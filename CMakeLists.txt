cmake_minimum_required(VERSION 3.20)

project(trust-cpp VERSION 1.0.0 LANGUAGES C CXX)

# Используем clang++ напрямую, как в исходных Makefile
set(CMAKE_CXX_COMPILER clang++-21)

# Установим стандарт C++20, как в оригинальном Makefile
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавим флаги оптимизации
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-usage")

# Найдем необходимые компоненты
find_package(LLVM REQUIRED CONFIG)
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Определим функцию для установки общих свойств целей
function(set_common_target_properties TARGET_NAME)
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

# Определим функцию для настройки целей тестов
function(setup_test_target TARGET_NAME TEST_FILE OPTIMIZATION_LEVEL IS_UNIT_TEST)
    # Создадим цель для тестов
    add_executable(${TARGET_NAME}
        ${TEST_FILE}
        stack_info.cpp
    )

    # Настроим тесты
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    # Используем общую функцию для установки свойств
    set_common_target_properties(${TARGET_NAME})

    target_compile_options(${TARGET_NAME} PRIVATE
        -g
        ${OPTIMIZATION_LEVEL}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE
        BUILD_UNITTEST
    )

    target_link_libraries(${TARGET_NAME} 
        pthread
        gmp
        gmpxx
    )

    # Добавим определение BUILD_UNITTEST и библиотеки только для unit-тестов
    if(IS_UNIT_TEST)
        target_link_libraries(${TARGET_NAME} 
            GTest::GTest 
            GTest::Main 
        )
    endif()
endfunction()



# Создадим библиотеку плагина clang
add_library(trusted-cpp_clang SHARED
    stack_check_clang.cpp
)

# Настроим плагин
target_include_directories(trusted-cpp_clang PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(trusted-cpp_clang PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(trusted-cpp_clang PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Используем общую функцию для установки свойств
set_common_target_properties(trusted-cpp_clang)

target_link_libraries(trusted-cpp_clang
    LLVMCore
    LLVMSupport
    LLVMAnalysis
    LLVMTransformUtils
)

setup_test_target(uint-test-O0 test/unit_test.cpp -O0 TRUE)
setup_test_target(uint-test-O3 test/unit_test.cpp -O3 TRUE)

setup_test_target(speed-test-O0 test/speed_test.cpp -O0 FALSE)
setup_test_target(speed-test-O3 test/speed_test.cpp -O3 FALSE)

setup_test_target(prime-check-O0 test/prime_check.cpp -O0 FALSE)
setup_test_target(prime-check-O3 test/prime_check.cpp -O3 FALSE)

# Создадим цель для запуска тестов
add_custom_target(run_tests
    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/uint-test-O0
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/uint-test-O0

    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/uint-test-O3
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/uint-test-O3

    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/speed-test-O0
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/speed-test-O0

    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/speed-test-O3
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/speed-test-O3

    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/prime-check-O0
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/prime-check-O0 100000 1

    COMMAND echo Run: ${CMAKE_CURRENT_SOURCE_DIR}/test/prime-check-O3
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/prime-check-O3 100000 1

    COMMAND echo Run: LLVM Integrated Tester in ${CMAKE_CURRENT_SOURCE_DIR}/test
    COMMAND ${PYTHON_EXECUTABLE} /usr/lib/llvm-21/build/utils/lit/lit.py ${CMAKE_CURRENT_SOURCE_DIR}/test -v

    DEPENDS uint-test-O0 uint-test-O3 speed-test-O0 speed-test-O3 prime-check-O0 prime-check-O3 trusted-cpp_clang
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    COMMENT "Running trust unit tests with both -O0 and -O3 optimization levels and LIT tests"
)

# Создадим псевдоним для обратной совместимости
add_custom_target(uint-test)
add_dependencies(uint-test uint-test-O0 uint-test-O3)
