cmake_minimum_required(VERSION 3.20)

project(trust-cpp VERSION 1.0.0 LANGUAGES C CXX)

# Используем clang++ напрямую, как в исходных Makefile
set(CMAKE_CXX_COMPILER clang++-21)

# Установим стандарт C++20, как в оригинальном Makefile
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавим флаги оптимизации
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

# Найдем необходимые компоненты
find_package(LLVM REQUIRED CONFIG)
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)

# Определим функцию для установки общих свойств целей
function(set_common_target_properties TARGET_NAME)
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

# Определим функцию для настройки целей тестов
function(setup_test_target TARGET_NAME OPTIMIZATION_LEVEL)
    # Создадим цель для тестов
    add_executable(${TARGET_NAME}
        test/stack_info_test.cpp
        stack_info.cpp
    )

    # Настроим тесты
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    # Используем общую функцию для установки свойств
    set_common_target_properties(${TARGET_NAME})

    target_compile_definitions(${TARGET_NAME} PRIVATE
        BUILD_UNITTEST
    )

    target_compile_options(${TARGET_NAME} PRIVATE
        ${OPTIMIZATION_LEVEL}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${TARGET_NAME} 
        GTest::GTest 
        GTest::Main 
        pthread
    )
endfunction()

# # Определим функцию для анализа файлов с использованием плагина
# function(add_trust_analysis_command SOURCE_FILE OUTPUT_FILE DEPENDS_TARGET ARG1 ARG2)
#     add_custom_command(TARGET ${DEPENDS_TARGET} PRE_BUILD
#         COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -ferror-limit=500 -g -DBUILD_UNITTEST -I${CMAKE_CURRENT_SOURCE_DIR}
#                 -Xclang -load -Xclang "${CMAKE_SOURCE_DIR}/trusted-cpp_clang.so"
#                 -Xclang -add-plugin -Xclang trust
#                 -Xclang -plugin-arg-trust -Xclang log
#                 -Xclang -plugin-arg-trust -Xclang level=warning
#                 ${ARG1} ${ARG2}
#                 -MMD -MP -MF ${OUTPUT_FILE}.d
#                 -o ${OUTPUT_FILE}.o
#                 -c ${SOURCE_FILE}
#         DEPENDS ${DEPENDS_TARGET} ${DEPENDS_TARGET}_trust
#         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#         COMMENT "Analyzing ${SOURCE_FILE} with trust plugin"
#     )
# endfunction()


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

# Создадим цель для тестов с оптимизацией -O0
setup_test_target(stack-check-test-O0 -O0)

# Создадим цель для тестов с оптимизацией -O3
setup_test_target(stack-check-test-O3 -O3)

# Убедимся, что плагин собран перед запуском тестов
add_dependencies(stack-check-test-O0 trusted-cpp_clang)
add_dependencies(stack-check-test-O3 trusted-cpp_clang)

# Создадим цель для запуска тестов
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/stack-check-test-O0
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test/stack-check-test-O3
    DEPENDS stack-check-test-O0 stack-check-test-O3
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    COMMENT "Running trust unit tests with both -O0 and -O3 optimization levels"
)

# Создадим псевдоним для обратной совместимости
add_custom_target(stack-check-test)
add_dependencies(stack-check-test stack-check-test-O0)
