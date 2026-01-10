# Proof of concept for automatic stack overflow checking when calling functions.

## Description of Stack Overflow Protection for C++

This project is intended to prevent segmentation faults due to stack overflow, which always lead to abnormal termination of the application (the project was created as part of implementing the concept of [trusted programming in C++](https://github.com/afteri-ru/trusted-cpp), which provides safe programming guarantees at the source code level).

The main idea is to check the available stack space before calling a protected function, and if it is insufficient, throw a `stack_overflow` program exception, which can be caught and handled within the application without waiting for a segmentation fault caused by a program/thread stack overflow.

Checking the size of available stack space can be performed by calling the function `stack_info::check_overflow(size_t)` with a specified size, or by using the function `stack_info::check_limit()`, which checks the maximum possible stack size obtained based on data from the `.stack_sizes` segment. For preserving information about the stack sizes for each function, the program must be compiled with the `-fstack-size-section` flag.

The `stack_check.h` file contains the necessary program primitives, and the `stack_check_clang.cpp` file implements a Clang plugin that, during IR code generation, automatically inserts calls to stack overflow checking functions before the protected functions. Protected functions can be marked individually in C++ code using an attribute, ~~or they can be specified using a name mask by passing it in the compiler plugin parameters.~~ **\***

### Usage examples

To mark functions and class methods that require checking the free space on the stack before calling them, C++ custom attributes are used, which are expanded using the `STACK_CHECK_SIZE(size)` and `STACK_CHECK_LIMT` macros:

- The `STACK_CHECK_SIZE(size)` attribute takes a single integer argument—the size of the free stack space that will be automatically checked before calling the protected function. ~~If the argument is zero, the size of the free stack space will be computed automatically when generating the executable code of the protected function~~. **\*\***

- The `STACK_CHECK_LIMT` attribute also checks the size of the free stack space, which is specified at application compile time. The stack usage size for each function can be determined by specifying the -fstack-usage option during compilation, which saves to a \*.su file a list of all functions and the stack size required for them.

Automatic insertion of code before a protected function can be disabled. To do this, insert a call in the C++ code to the helper static method `ignore_next_check(const size_t)`, passing the number of upcoming code insertions that will be skipped (removed) from the generated (executable) file.

```cpp
#include "stack_check.h"

using namespace trust;

const thread_local stack_check stack_check::info;

// Function without automatic stack overflow checking
int func() {
    ...
}

// Before each call to the function, code will be inserted to check the specified free stack space
[[stack_check_size(100)]]
int guard_size_func() {
    char data[92];
    ...
}

// Before each call to the function, the minimum free stack space will be checked
STACK_CHECK_LIMIT
int guard_limit_func() {
    ...
}

int main() {

    // Code for stack overflow control will be automatically added here
    guard_size_func();

    stack_check::ignore_next_check(1); // The next automatic stack check insertion will be ignored
    guard_size_func();

    // Code to check the minimum free stack space will be automatically added here
    guard_limit_func();

    stack_check::check_overflow(10000); // Manual check of free stack space
    func();

    return 0;
}
```


After this, the file is compiled using the clang plugin:
```bash
clang++ -std=c++20 -Xclang -load -Xclang stack_check_clang.so -Xclang -add-plugin -Xclang stack_check -lpthread filename.cpp
```

----
**\***) - specifying protected functions using a name mask is not yet implemented  
**\*\***) - this functionality is not implemented in the compiler plugin, as no way was found to insert an analyzer pass after generating machine code.

## Implementation details

The final stack size required for a function call depends on many factors, such as the target platform, the optimization level of the program, the calling convention of the specific function, etc. Because of this, it cannot be computed using a static analyzer based on the AST or by analyzing the IR; it can only be determined after generating machine instructions for the specific target platform.

Moreover, for the purposes of automatic checking (for functions marked with the `stack_check_size` or `stack_check_limit` attributes), the minimum size of the free stack space cannot be less than a certain fixed threshold required to create a program exception with error information. The size of such a threshold depends on the implementation and is influenced by the target platform, operating system, optimization level, and other factors.

The main stack overflow control functionality is in the `trust::stack_check` class. Information about the stack size is stored in static class fields, individually for each thread (Thread Local — thread-local storage, TLS), which allows querying stack parameters once per thread when initializing the structure, and when checking the free stack space using the `stack_check::check_overflow(N)` method, comparing the current stack pointer with the lower bound of the memory region allocated for the stack.

To use it, you must define the static variable `const thread_local trust::stack_check trust::stack_check::info`, and to specify the minimum free stack space limit, assign the corresponding value to the `STACK_SIZE_LIMIT` macro.

## Overhead

Checking the stack for overflow, even with maximum optimization, cannot be shorter than two machine instructions (a compare operation and a short branch), which inevitably adds time to a function call.

As a “speed meter,” a recursive prime finder program from `prime_check.cpp` was used (as tests, the Tower of Hanoi and a recursive algorithm for summing digits of a long number were also tried, but the stack depth required to check overflow in the first case requires a very long runtime, and writing out the long number that triggers a stack overflow takes several screens, which is also inconvenient for testing purposes).

```bash
$ ./prime-check-O3
Usage: ./prime-check-O3 <start_number> [count]

$ ./prime-check-O3 9999999999999999
Stack overflow exception at: 104588 call depth.
Stack top: 0x7fffdc634000 bottom: 0x7fffdbe36000 (stack size: 8380416)
Query size: 10000 end frame: 0x7fffdbe386a0 (free space: 9888)

$ ./prime-check-O3 10000000000 1000
10000000019
10000000033
10000000061
...
10000022899
10000022909
Max recursion depth SAFE: 100000
Number of recursive calls SAFE: 117539009
Execution time SAFE: 18227634 microseconds

Max recursion depth: 100000
Number of recursive calls: 117539009
Execution time: 17925080 microseconds
Difference in execution time: 1.68788 %
```

Assessment of the impact of stack overflow checking on application performance: without optimization (-O0), the execution time increases by approximately *1%–5%*, and with maximum optimization (-O3), by approximately *0.5%–2%* (the total application runtime is about *15 seconds*).

Ideally (to minimize overhead), it is best to compute the stack size for all functions in the program and always use the maximum value (since loading a value into a register before the compare also requires CPU cycles and memory access). In this case, for any sequence of function calls within one block, it is sufficient to check the free stack space only before the first call.


--------
--------
--------

## Описание защиты стека от переполнения для C++

Данный проект предназначен для предотвращения ошибок сегментации памяти из-за переполнения стека, которые всегда приводят к аварийному завершению работы приложения (проект создан в рамках реализации концепции [доверенного программирования на C++](https://github.com/afteri-ru/trusted-cpp), которая обеспечивает гарантии безопасного программирования на уровне исходного текста программы).

Основная идея заключается в проверке свободного места на стеке перед вызовом защищаемой функции, и если его недостаточно, то выбрасывается программное исключение `stack_overflow`, которое можно перехватить и обработать изнутри приложения, не дожидаясь возникновения ошибки сегментирования из-за переполнения стека программы/потока.

Проверка размера свободного места на стеке может выполняться с помощью вызова функции `stack_info::check_overflow(size_t)` с указанием конкретного размера либо с помощью функции `stack_info::check_limit()`, которая проверяет максимально возможный размер стека, полученный на основании данных из сегмента `.stack_sizes`. **Для сохранения информации о размерах стека для каждой функции программа должна быть скомпилирована с ключом `-fstack-size-section`.**

В файле `stack_check.h` находятся необходимые программные примитивы, а в файле `stack_check_clang.cpp` реализован плагин для Clang, который на этапе генерации IR-кода автоматически вставляет вызовы функций контроля переполнения стека перед защищаемыми функциями. Защищаемые функции могут быть отмечены индивидуально в коде C++ с помощью атрибута, ~~либо их можно указать с помощью маски имён, передав её в параметрах плагина компилятора.~~ **\***

### Примеры использования

Для маркировки функций и методов классов, перед вызовом которых требуется проверка свободного места на стеке, используются пользовательские атрибуты C++, которые раскрываются с помощью макросов `STACK_CHECK_SIZE(size)` и `STACK_CHECK_LIMT`:

- Атрибут `STACK_CHECK_SIZE(size)` принимает один аргумент в виде целого числа - размер свободного пространства на стеке, который будет автоматически проверяться перед вызовом защищаемой функции. ~~Если в качестве аргумента указан ноль, то размер свободного пространства на стеке будет вычисляться автоматически при генерации исполняемого кода защищаемой функции~~. **\*\***

- Атрибут `STACK_CHECK_LIMIT` тоже проверяет размер свободного пространства на стеке, который задаётся при компиляции приложения. *Размер использования стека для каждой функции можно выяснить, указав при компиляции опцию -fstack-usage, которая сохраняет в файле \*.su список всех функций и требуемый для них размер стека*.

Автоматическую вставку кода перед защищаемой функцией можно отменить. Для этого требуется вставить в C++ коде вызов вспомогательного статического метода `ignore_next_check(const size_t)`, которому передаётся количество следующих вставок кода, которые будут пропущены (удалены) из генерируемого (исполняемого) файла.

Пример кода для использования библиотеки:
```cpp
#include "stack_check.h"

using namespace trust;

const thread_local stack_check stack_check::info;

// Функция без автоматической проверки стека от переполнения
int func() {
    ...
}

// Перед каждым вызовом функции будет вставлен код проверки указанного свободного места на стеке
[[stack_check_size(100)]]
int guard_size_func() {
    char data[92];
    ...
}

// Перед каждым вызовом функции будет проверяться минимальный размер свободного места на стеке
STACK_CHECK_LIMIT
int guard_limit_func() { 
    ...
}

int main() {

    // Тут будет автоматически добавлен код для контроля стека от переполнения
    guard_size_func();

    stack_check::ignore_next_check(1); // Следующая автоматическая вставка проверки стека будет проигнорирована
    guard_size_func();

    // Тут будет автоматически добавлен код для проверки минимального размера свободного места на стеке
    guard_limit_func();

    stack_check::check_overflow(10000); // Ручная проверка свободного места на стеке
    func();

    return 0;
}
```
После чего файл компилируется с подключением clang плагина:
```bash
clang++ -std=c++20 -Xclang -load -Xclang stack_check_clang.so -Xclang -add-plugin -Xclang stack_check -lpthread filename.cpp
```

----
**\***) - указание защищаемых функций с помощью маски имён пока не реализовано  
**\*\***) - данная функциональность в плагине компилятора не реализована, так как не нашёл способа встроить проход анализатора на этапе генерации машинного кода.

## Детали реализации

Финальный размер стека для вызова функции зависит от множества факторов, таких как целевая платформа, степень оптимизации программы, соглашение о вызове конкретной функции и т. д., из-за чего его нельзя вычислить с помощью статического анализатора кода на основе AST или проанализировав IR-представление, а можно определить только на этапе генерации машинных инструкций под конкретную целевую платформу.

Причём для целей автоматического контроля (для функций, отмеченных атрибутом `stack_check_size` или `stack_check_limit`) минимальный размер свободного пространства на стеке не может быть меньше определённого фиксированного порога, который требуется для создания программного исключения с информацией об ошибке. Размер такого порога зависит от реализации, и на него влияет целевая платформа, операционная система, степень оптимизации программы и прочие факторы.

Основная функциональность контроля переполнения стека находится в классе `trust::stack_check`. Информация о размере стека хранится в статических полях класса, индивидуально для каждого потока (*Thread Local* - локальное хранилище потоков, TLS), что позволяет однократно запрашивать параметры стека для каждого потока при инициализации структуры, а при проверке размера свободного места на стеке с помощью метода `stack_check::check_overflow(N)` сравнивать текущий указатель стека с нижней границей выделенной под стек области памяти.

Для использования необходимо определить статическую переменную `const thread_local trust::stack_check trust::stack_check::info`, а для указания минимального лимита свободного пространства на стеке присвоить соответствующее значение макросу `STACK_SIZE_LIMIT`.

## Накладные расходы

Проверка стека на переполнение, даже в случае максимальной оптимизации, не может быть короче двух машинных инструкций (операции сравнения и инструкции короткого перехода), что, безусловно, добавляет время к вызову функции.

*В качестве "измерителя скорости" использовал программу нахождения простых чисел рекурсивным методом из файла `prime_check.cpp` (в качестве тестов также пробовал ханойские башни и рекурсивный алгоритм подсчёта суммы цифр у длинного числа, но глубина стека для проверки переполнения в первом случае требует очень большой продолжительности работы алгоритма, а запись длинного числа, при котором возникает переполнение стека, занимает несколько экранов, что тоже неудобно для целей тестирования).*

```bash
$ ./prime-check-O3
Usage: ./prime-check-O3 <start_number> [count]

$ ./prime-check-O3 9999999999999999
Stack overflow exception at: 104588 call depth.
Stack top: 0x7fffdc634000 bottom: 0x7fffdbe36000 (stack size: 8380416)
Query size: 10000 end frame: 0x7fffdbe386a0 (free space: 9888)

$ ./prime-check-O3 10000000000 1000
10000000019
10000000033
10000000061
...
10000022899
10000022909
Max recursion depth SAFE: 100000
Number of recursive calls SAFE: 117539009
Execution time SAFE: 18227634 microseconds

Max recursion depth: 100000
Number of recursive calls: 117539009
Execution time: 17925080 microseconds
Difference in execution time: 1.68788 %
```

Оценка влияния контроля переполнения стека на скорость работы приложения: без оптимизации (-O0) - время выполнения увеличивается примерно на *1%*-*5%*, а при максимальной оптимизации (-O3) - примерно на *0,5-2%* (общее время выполнения приложения около *15 секунд*).

В идеальном виде (если стремиться к минимальным накладным расходам) лучше всего вычислять размер стека для всех функций программы и всегда использовать максимальное значение (ведь загрузка значения в регистр перед операцией сравнения также требует тактов процессора и обращения к памяти). В этом случае при любых последовательных вызовах функций в одном блоке достаточно будет проконтролировать свободное место на стеке только перед вызовом первой функции.
