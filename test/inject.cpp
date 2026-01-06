
// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -emit-llvm -O0  \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose \
// RUN:-c %s -o %p/temp/inject-O0.S  > %p/temp/inject-O0.out \
// RUN: && FileCheck %s -check-prefix=O0 < %p/temp/inject-O0.S

// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -Xclang -load -Xclang %shlibdir/stack_check_clang.so \
// RUN: -Xclang -add-plugin -Xclang stack_check \
// RUN: -Xclang -emit-llvm -O3 \
// RUN: -Xclang -plugin-arg-stack_check -Xclang verbose \
// RUN: -c %s -o %p/temp/inject-O3.S > %p/temp/inject-O3.out \
// RUN: && FileCheck %s -check-prefix=O3 < %p/temp/inject-O3.S

#include "stack_check.h"

// Правильное использование атрибута - на функции
STACK_CHECK_SIZE(100)
[[clang::optnone]] void inject_function() { char buffer[92]; }

STACK_CHECK_LIMIT
[[clang::optnone]] void inject_limit() { char buffer[55]; }

[[clang::optnone]] void other_function() {}

// Правильное использование атрибута - на методе класса
class TestClass {
  public:
    STACK_CHECK_SIZE(99)
    [[clang::optnone]] static void inject_method() {}
};


const thread_local trust::stack_check info;  

size_t inplace_code(size_t size){
  if (static_cast<char *>(__builtin_frame_address(0)) < (static_cast<char *>(info.bottom) + size)) {
    trust::stack_check::throw_stack_overflow(size, info);
  }

  // No need for addition operator before comparison and more opportunities for optimization
  if (static_cast<char *>(__builtin_frame_address(0)) < (static_cast<char *>(info.bottom_limit))) {
      trust::stack_check::throw_stack_overflow(info.limit, info);
  }  
  return size;
}

int main() {
    // O0: define dso_local noundef i32 @main()
    // O3: define dso_local noundef i32 @main() local_unnamed_addr

    // The next  two injected code fragments are ignored
    trust::stack_check::ignore_next_check(2);

    // O0-NOT: ignore_next_check
    // O3-NOT: ignore_next_check

    // First ignored injected fragments
    inject_function();
    // O0: call void @_Z15inject_functionv()
    // O3: call void @_Z15inject_functionv()

    // Second ignored injected fragments
    inject_limit();
    // O0-NEXT: call void @_Z12inject_limitv()
    // O3-NEXT: call void @_Z12inject_limitv()

    other_function();
    // O0-NEXT: call void @_Z14other_functionv()
    // O3-NEXT: call void @_Z14other_functionv()

    // The automatic code injection skip counter has been exhausted,
    // now it is necessary to automatically insert the code to check for free space in the stack.
    
    inject_function();
    // O0-NEXT: call void @_ZN5trust11stack_check14check_overflowEm(i64 100)
    // O0-NEXT: call void @_Z15inject_functionv()

    // The function call is converted into inline code by the optimizer.
    // COM: O3-NEXT: call void @_ZN5trust11stack_check14check_overflowEm(i64 100)
    //O3: call void @_Z15inject_functionv()

    inject_limit();
    // O0-NEXT: call void @_ZN5trust11stack_check11check_limitEv()
    // O0-NEXT: call void @_Z12inject_limitv()

    // The function call is converted into inline code by the optimizer.
    // COM: O3-NEXT: call void @_ZN5trust11stack_check11check_limitEv()
    //O3: call void @_Z12inject_limitv()

    TestClass::inject_method();
    // O0-NEXT: call void @_ZN5trust11stack_check14check_overflowEm(i64 99)
    // O0-NEXT: call void @_ZN9TestClass13inject_methodEv()

    // The function call is converted into inline code by the optimizer.
    // COM: O3-NEXT: call void @_ZN5trust11stack_check14check_overflowEm(i64 99) 
    //O3: call void @_ZN9TestClass13inject_methodEv()

    return 0;
}
