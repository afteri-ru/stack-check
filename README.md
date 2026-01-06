# Proof of concept for automatic stack overflow checking when calling functions.

The project is implemented within the concept of [trusted programming in C++](https://github.com/afteri-ru/trusted-cpp) and the realization of secure programming guarantees at the source code level.

The implementation is provided as an auxiliary `stack_chack.h` file and a Clang plugin that processes the program’s source text and adds to the generated code the checks required to protect the stack from overflow.

The source-level implementation consists in adding a custom C++ attribute `[[stack_check]]`, which is handled by the compiler plugin and can be used either at a function (class method) definition or immediately before a function (class method) call as follows:

- `[[stack_check_size(N)]]`, where N is greater than zero — insert a manual check of free stack space and, if the free stack space is less than N, a stack_overflow exception will be generated. If the attribute is used at the function definition, the stack overflow check will be inserted automatically before each call to that function. The attribute can be specified immediately before any function call, in which case the free‑stack‑space check code will be inserted at that location once.

- `[[stack_check_size(-1)]]` — cancel insertion of code to check free stack space. The attribute can be specified immediately before any function call, in which case the free‑stack‑space check code will not be inserted at that location, even if the function was previously defined with the *stack_check* attribute.

- `[[stack_check]]` or `[[stack_check_size(0)]]` at the function definition — enable automatic free‑stack‑space checking before each call to this function, with the stack size to check computed automatically.

Computing the required stack size for a function call depends on many factors, such as the target platform, optimization level, the calling convention of the particular function, etc., which is why it cannot be computed by an AST‑based static analyzer and can only be determined after machine instructions are generated.

Moreover, if the stack size for a function with stack overflow protection is to be computed automatically, then all functions it calls must also be protected against stack overflow. This can be implemented either by using functions with automatic stack overflow protection, or by placing the attribute directly before the remaining function calls with the manually specified free‑stack‑space size to check, i.e., `[[stack_check_size(N)]]`, where N is an integer greater than zero.

```cpp
[[stack_check]]
int stack_guard_func() {
    int temp[10] = {0};

    [[stack_check_size(100)]]
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


## Описание защиты стека от переполнения для C++

Проект создан в рамках реализации концепции [доверенного программирования на C++](https://github.com/afteri-ru/trusted-cpp), которая обеспечивает гарантии безопасного программирования на уровне исходного текста программы. Данный проект предназначен для предотвращения ошибок сегментации памяти из-за переполнения стека, которые всегда приводят к аварийному завершению работы приложения.

Основная идея заключается в проверке свободного места на стеке перед вызовм защищаемой функции, и если его не достаоточно, то выбрасывается программное исключение `stack_overflow`, которое можно перехватить и обработать изнутри приложения не дожидаясь возникновения ошшибки сигментирования из-за переполнения стека программы/потока.

В фйле `stack_check.h` находятся необходимые программные примитивы для ручного применения, тогда как в файле `stack_check_clang.cpp` релизован  плагин для Clang, который на этапе геренарции IR кода автоматичсеки вставялет вызовы функций контроля стека от переполнения перед защищаемыи функциями. Защищаемые функции могут быть отмечены индивидуально в коде C++ с помощью атрибута, ~~либо их можно указать с помощью маски имен, передав её в параметрах плагина компилятора.~~ **\***


### Примеры использования

Для маркировки функций и методов классов, перед вызвом которых требуется проверка свободного места на стеке, используются C++ атрибуты `[[stack_check_size( const size_t )]]` и `[[stack_check_limit]]`:

- Атрибут `[[stack_check_size( const size_t )]]` принимает один аргумент в виде целого числа - размер свободного пространства на стеке, который будет автоматически проверяться перед вызовом защищаемой функции. ~~Если в качестве арумента указан ноль, то размер свободного простарнства на стеке будет вычисляться автоматически при генерации исполняемого кода защищаемой функции~~.**\*\*** 

- Атрубет `[[stack_check_limit]]` тоже проверяет размер свободного пространства на стеке, который задается при компиляции приложения. *Размер использования стека для каждой функции можно выяснить, указав при компиляции опцию -fstack-usage, которая сохраняет в файле \*.su список всех функций и требуемый для них размер стека*.

Автоматическу вставку кода перед защищаемой функцией можно отметить. Для этого требуется вставить в С++ коде вызов вспомогательного статического метода `ignore_next_check(const size_t)`, которому передается количество следующих вставок кода, которые будет пропущены (уделны) из генерируемого (исполняемого) файла.


```cpp
#include "stack_check.h"

const thread_local trust::stack_check trust::stack_check::info;

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

// Перед каждым вызовом функции будет проверятся минимальный размер свободного места на стеке
[[stack_check_limit]]
int guard_limit_func() {
    ...
}

int main(){

    // Тут будут автоматически добавлен код для контроля стека от переполнения
    guard_size_func(); 

    stack_check::ignore_next_check(1); // Следующая автоматическая вставка проверки стека будет проигнорирована
    guard_size_func();

    // Тут будут автоматически добавлен код для проверки минимального размера свободного места на стеке
    guard_limit_func();

    stack_check::check_overflow(10000); // Ручная проврка свободного места на стеке
    func();

    return 0;
}
```

----
**\***) - указание защищаемых функций с помощью маски имен пока не реализовано
**\*\***) - даная функциональность в плагине компилятора не реализована, так как не нашел способа встроить проход анализатора после генерации машинного кода.


## Детали реализации

Финальный размера стека для вызова функции зависит от множества факторов, таких как целевая платформа, степень оптимизации программы, соглашение о вызове конкретной функции и т. д., из-за чего его нельзя вычислить с помощью статического анализатора кода на основе AST или проанализировав IR представление, а можно определить только после генерации машинных инструкций под конкретную целевую платформу.

Причем, для целей автоматического контроля (для фукнций, отмеченных атрибутом `stack_check_size` или `stack_check_limit`), минимальный размер совбодного простарнства на стеке не может быть меньше определенного фиксированного порога, который требуется для создания програмного исключения с информацией об ошибке. Размер такого прога зависит от реализации, и на него влияет целевая платформа, операционная система, степень оптимизации программы и прочие факторы. 

Основная функциональность контроля переполнения стека находится в классе `trust::stack_check`. Информация о размере стека хранится в статических полях класса, индивидуально для каждого потока (*Thread Local* - локальное хранилище потоков, TLS), что позволяет однократно запрашивать параметры стека для каждого потока при инициализации структуры, а при проверке размера свободного места на стеке с помощью метода `stack_check::check_overflow(N)` - только сравнивать текущий указатель стека с нижней границей выделенной под стек области памяти.

Для использования необходимо определить статическую переменную `const thread_local trust::stack_check trust::stack_check::info`, 
а для указания минимального лимита свободного пространтсва на стеке присвоить соотвествующее значение макросу `STACK_SIZE_LIMIT`.

## Накладные расходы

Проверка стека на переполнение, даже в случае максимальной оптимизации, не может быть короче двух машинных инструкций (операции сравнения и инструкции короткого перехода), что, безусловно, добавляет время к вызову функции.

*В качестве "измерителя скорости" использовал программу нахождения простых чисел рекурсивным методом из файла `prime_check.cpp` (в качестве тестов также пробовал ханойские башни и рекурсивный алгоритм подсчета суммы цифр у длинного числа, но глубина стека для проверки переполнения в первом случае требует очень большой продолжительности работы алгоритма, а запись длинного числа, при котором возникает переполнение стека, занимает несколько экранов, что тоже неудобно для целей тестирования).*

Оценка влияния контроля переполнения стека на скорость работы приложения: без оптимизации (**-O0**) - время выполнения увеличивается примерно на *8%*-*14%*, а при максимальной оптимизации (**-O3**) - примерно на *0,5-1,5%* (общее время выполнения приложения около *2 секунд*).

В идеальном виде (если стремиться к минимальным накладным расходам) лучше всего вычислять размер стека для всех функций программы и всегда использовать максимальное значение (ведь загрузка значения в регистр перед операцией сравнения также требует тактов процессора и обращения к памяти). В этом случае при любых последовательных вызовах функций в одном блоке достаточно будет проконтролировать свободное место на стеке только перед вызовом первой функции.