# Proof of concept for automatic stack overflow checking when calling functions.

The project is implemented within the concept of [trusted programming in C++](https://github.com/afteri-ru/trusted-cpp) and the realization of secure programming guarantees at the source code level.

The implementation is provided as an auxiliary `stack_chack.h` file and a Clang plugin that processes the program’s source text and adds to the generated code the checks required to protect the stack from overflow.

The source-level implementation consists in adding a custom C++ attribute `[[stack_check]]`, which is handled by the compiler plugin and can be used either at a function (class method) definition or immediately before a function (class method) call as follows:

- `[[stack_check(N)]]`, where N is greater than zero — insert a manual check of free stack space and, if the free stack space is less than N, a stack_overflow exception will be generated. If the attribute is used at the function definition, the stack overflow check will be inserted automatically before each call to that function. The attribute can be specified immediately before any function call, in which case the free‑stack‑space check code will be inserted at that location once.

- `[[stack_check(-1)]]` — cancel insertion of code to check free stack space. The attribute can be specified immediately before any function call, in which case the free‑stack‑space check code will not be inserted at that location, even if the function was previously defined with the *stack_check* attribute.

- `[[stack_check]]` or `[[stack_check(0)]]` at the function definition — enable automatic free‑stack‑space checking before each call to this function, with the stack size to check computed automatically.

Computing the required stack size for a function call depends on many factors, such as the target platform, optimization level, the calling convention of the particular function, etc., which is why it cannot be computed by an AST‑based static analyzer and can only be determined after machine instructions are generated.

Moreover, if the stack size for a function with stack overflow protection is to be computed automatically, then all functions it calls must also be protected against stack overflow. This can be implemented either by using functions with automatic stack overflow protection, or by placing the attribute directly before the remaining function calls with the manually specified free‑stack‑space size to check, i.e., `[[stack_check(N)]]`, where N is an integer greater than zero.

```cpp
[[stack_check]]
int stack_guard_func() {
    int temp[10] = {0};

    [[stack_check(100)]]
    stack_unguard_func();

    return 0;
}
```

## Implementation details and overhead

The required amount of free stack space to call a function consists of two components:
- the amount of free stack space sufficient for the direct call of the current function;
- a fixed reserve size sufficient for creating an exception with error information and for performing all necessary accompanying function calls, which may also require stack space in the current thread.

The fixed reserve size for error handling depends on the implementation and is affected by the target platform, operating system, program optimization level, and other factors. At present, a reserve size of 4500 bytes is used for error handling, and during free‑stack‑space checks it is automatically added to the stack size being checked for a function call.

The core stack overflow checking functionality resides in the file `stack_check.h`, in the `trust::stack_check` structure. Information about the stack size is stored in static fields of the structure that are per-thread (Thread-Local Storage, TLS), which allows querying the stack parameters once per thread during structure initialization, and when checking the amount of free stack space using the `stack_check::check_overflow(N)` method, only comparing the current stack pointer against the lower boundary of the memory region allocated for the stack.

A stack overflow check, even under maximum optimization, cannot be shorter than two machine instructions (a compare operation and a short branch), which inevitably adds time to a function (subroutine) call.

As a “speed gauge,” a program for finding prime numbers by a recursive method from the file `prime_check.cpp` was used (as tests, the Towers of Hanoi and a recursive algorithm for summing the digits of a long number were also tried, but in the first case the stack depth needed to trigger an overflow requires a very long runtime, and writing out a long number that causes a stack overflow takes several screens, which is also inconvenient for testing).

Assessment of the impact of stack overflow checking on application performance: without optimization (**-O0**), execution time increases by approximately *8%–14%*, and with maximum optimization (**-O3**) by roughly *0.5%–1.5%* (total application runtime about *2 seconds*).

Ideally (to minimize overhead), it is best to compute the stack usage for all functions in the program and always use the maximum value (since loading the value into a register before the comparison also consumes CPU cycles and memory accesses). In this case, for any sequential calls to functions within a single block, it is sufficient to check the available stack space only before calling the first function.


--------


## Проверка концепции защиты стека от переполнения для C++

Проект реализуется в рамках концепции [доверенного программирования на C++](https://github.com/afteri-ru/trusted-cpp) и реализации гарантий безопасного программирования на уровне исходного текста программы.

Реализация выполнена в виде вспомогательного файла `stack_check.h` и плагина для Clang, который обрабатывает исходный текст программы и добавляет в генерируемый код необходимые проверки для защиты стека от переполнения.

Реализация на уровне исходного текста программы заключается в добавлении  нового  C++ атрибута `[[stack_check]]`, который обрабатывается плагином компилятора и может быть использован при определении функции (метода класса) или перед вызовом функции (метода класса) следующим образом:

- `[[stack_check(N)]]`, где **N** больше нуля - вставить ручную проверку свободного места на стеке и, в случае если свободного места на стеке будет меньше **N**, будет сгенерировано исключение *stack_overflow*. Если атрибут использован при определении функции, тогда проверка переполнения стека будет вставляться перед каждым вызовом функции автоматически. Атрибут может быть указан непосредственно перед вызовом любой функции, тогда код проверки свободного места на стеке будет вставлен в этом месте однократно.
- `[[stack_check(-1)]]` - отменить вставку кода для проверки свободного места на стеке. Атрибут может быть указан непосредственно перед вызовом любой функции, тогда код проверки свободного места на стеке не будет вставлен в этом месте, даже если функция ранее была определена с атрибутом *stack_check*.
- `[[stack_check]]` или `[[stack_check(0)]]` при определении функции - включить автоматическую проверку свободного места на стеке перед каждым вызовом данной функции, а размер стека для проверки будет вычисляться автоматически.

Вычисление необходимого размера стека для вызова функции зависит от множества факторов, таких как целевая платформа, степень оптимизации, соглашение о вызове конкретной функции и т. д., из-за чего его нельзя вычислить с помощью статического анализатора кода на основе AST, а можно определить только после генерации машинных инструкций.

Причем, если размер стека у функции с защитой от переполнения стека должен вычисляться автоматически, то все вызываемые из нее функции также должны быть защищены от переполнения стека. Это может быть реализовано либо за счет функций с автоматической защитой стека от переполнения, либо установкой атрибута непосредственно перед вызовом остальных функций с указанием проверяемого размера свободного места на стеке в ручном режиме, т. е. ``[stack_check(N)]]`, где N - целое число больше нуля.

```cpp
[[stack_check]]
int stack_guard_func() {
    int temp[10] = {0};
    
    [[stack_check(100)]]
    stack_unguard_func();

    return 0;
}
```

## Детали реализации и накладные расходы

Необходимый размер свободного места на стеке для вызова функции состоит из двух компонентов:
- размер свободного места на стеке, достаточного для непосредственного вызова текущей функции;
- фиксированный резервный размер, достаточный для создания исключения с информацией об ошибке и выполнения всех необходимых для этого вызовов функций, которым также может потребоваться стек в текущем потоке.

Фиксированный резервный размер для обработки ошибки зависит от реализации, и на него влияет целевая платформа, операционная система, степень оптимизации программы и прочие факторы. В настоящий момент для обработки ошибки используется резервный размер 4500 байт, и при проверке свободного места на стеке он автоматически добавляется к проверяемому размеру стека при вызове функции.

Основная функциональность контроля переполнения стека находится в файле `stack_check.h` в структуре `trust::stack_check`. Информация о размере стека хранится в статических полях структуры, индивидуальных для каждого потока (*Thread Local* - локальное хранилище потоков, TLS), что позволяет однократно запрашивать параметры стека для каждого потока при инициализации структуры, а при проверке размера свободного места на стеке с помощью метода `stack_check::check_overflow(N)` - только сравнивать текущий указатель стека с нижней границей выделенной под стек области памяти.

Проверка стека на переполнение, даже в случае максимальной оптимизации, не может быть короче двух машинных инструкций (операции сравнения и инструкции короткого перехода), что, безусловно, добавляет время к вызову функции (вызову подпрограммы).

*В качестве "измерителя скорости" использовал программу нахождения простых чисел рекурсивным методом из файла `prime_check.cpp` (в качестве тестов также пробовал ханойские башни и рекурсивный алгоритм подсчета суммы цифр у длинного числа, но глубина стека для проверки переполнения в первом случае требует очень большой продолжительности работы алгоритма, а запись длинного числа, при котором возникает переполнение стека, занимает несколько экранов, что тоже неудобно для тестирования).*

Оценка влияния контроля переполнения стека на скорость работы приложения: без оптимизации (**-O0**) - время выполнения увеличивается примерно на *8%*-*14%*, а при максимальной оптимизации (**-O3**) - примерно на *0,5-1,5%* (общее время выполнения приложения около *2 секунд*).

В идеальном виде (если стремиться к минимальным накладным расходам) лучше всего вычислять размер стека для всех функций программы и всегда использовать максимальное значение (ведь загрузка значения в регистр перед операцией сравнения также требует тактов процессора и обращения к памяти). В этом случае при любых последовательных вызовах функций в одном блоке достаточно будет проконтролировать свободное место на стеке только перед вызовом первой функции.