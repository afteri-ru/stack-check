# Создадим цель для тестов с оптимизацией -O0
add_executable(stack-check-test-O0
    stack_check_test.cpp
    stack_info_test.cpp
    ../plugin/stack_info.cpp
)

# Настроим тесты с оптимизацией -O0
set_target_properties(stack-check-test-O0 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Используем общую функцию для установки свойств
set_common_target_properties(stack-check-test-O0)

target_compile_definitions(stack-check-test-O0 PRIVATE
    BUILD_UNITTEST
)

target_compile_options(stack-check-test-O0 PRIVATE
    -O0
)

target_include_directories(stack-check-test-O0 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugin
    ${YAML_CPP_INCLUDE_DIRS}
)

target_link_libraries(stack-check-test-O0 
    GTest::GTest 
    GTest::Main 
    yaml-cpp
    pthread
)

# Создадим цель для тестов с оптимизацией -O3
add_executable(stack-check-test-O3
    stack_check_test.cpp
    stack_info_test.cpp
    ../plugin/stack_info.cpp
)

# Настроим тесты с оптимизацией -O3
set_target_properties(stack-check-test-O3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Используем общую функцию для установки свойств
set_common_target_properties(stack-check-test-O3)

target_compile_definitions(stack-check-test-O3 PRIVATE
    BUILD_UNITTEST
)

target_compile_options(stack-check-test-O3 PRIVATE
    -O3
)

target_include_directories(stack-check-test-O3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugin
    ${YAML_CPP_INCLUDE_DIRS}
)

target_link_libraries(stack-check-test-O3 
    GTest::GTest 
    GTest::Main 
    yaml-cpp
    pthread
)

# Убедимся, что плагин собран перед запуском тестов
add_dependencies(stack-check-test-O0 trusted-cpp_clang)
add_dependencies(stack-check-test-O3 trusted-cpp_clang)

# Создадим цель для запуска тестов
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/stack-check-test-O0
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/stack-check-test-O3
    DEPENDS stack-check-test-O0 stack-check-test-O3
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running trust unit tests with both -O0 and -O3 optimization levels"
)

# Создадим псевдоним для обратной совместимости
add_custom_target(stack-check-test)
add_dependencies(stack-check-test stack-check-test-O0)
