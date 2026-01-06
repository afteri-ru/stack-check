## Mandatory Rules for this Project
- Development is done in C++ (using C is only allowed in cases where C++ is not applicable, such as when handling hardware interrupts)
- Using compiler clang++-21 or above
- Always add or re-introduce tests for new or changed code, even if no one has requested them.
- Always run unit tests after completing a task or before committing changes.
- Do not create missing files unless their creation is specified in the current task.

## CodeStyle Rules
- Always use spaces for indentation, 4 spaces wide.
- Use camelCase for variable names and the 'm_' prefix for class fields.
- Explain your reasoning before submitting code.
- Focus on code readability and maintainability.
- Upon task completion, the final report should include the model name, the initial task, and all summary statistics for interactions with the model.
- Don't use `using namespace`, always specify namespaces explicitly.

## File Rules
- `stack_check.h` contains stack overflow checker.
- `stack_check_clang.cpp` is a clang plugin designed for compilation into a dynamic library.
- Dir `test` contains all test programma and any test files for LIT - LLVM Integrated Tester and Google Test Framework.
