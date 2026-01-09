
// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -fstack-usage -fstack-size-section \
// RUN: -O0 %s -o %p/temp/stack-sizes-O0
// RUN: %p/temp/stack-sizes-O0 | FileCheck %s -check-prefix=O0
// RUN: FileCheck %s -check-prefix=SU-O0 < %p/temp/stack-sizes-O0.su

// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -fstack-usage -fstack-size-section \
// RUN: -O3 %s -o %p/temp/stack-sizes-O3
// RUN: %p/temp/stack-sizes-O3 | FileCheck %s -check-prefix=O3
// RUN: FileCheck %s -check-prefix=SU-O3 < %p/temp/stack-sizes-O3.su

#include <iostream>

#include "stack_check.h"

using namespace trust;

// Test functions
__attribute__((noinline)) void func1() {
    char buf[64] = {0};
    asm volatile("" : : "r,m"(buf) : "memory");
}

__attribute__((noinline)) void func2() {
    char buf[256] = {0};
    asm volatile("" : : "r,m"(buf) : "memory");
}

__attribute__((noinline)) void func3() {
    char buf[1024] = {0};
    asm volatile("" : : "r,m"(buf) : "memory");
}

// ============= FULL TEST =============

class Simple {
  public:
    Simple() {
        char buffer[40] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Simple ctor\n";
    }
    ~Simple() {
        char buffer[40] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Simple dtor\n";
    }
    void method() {
        char buffer[40] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Simple method\n";
    }
};

class Virtual {
  public:
    Virtual() {
        char buffer[80] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Virtual ctor\n";
    }
    virtual ~Virtual() {
        char buffer[80] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Virtual dtor\n";
    }
    void method() {
        char buffer[80] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Virtual method\n";
    }
    virtual void vmethod() {
        char buffer[80] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Virtual vmethod\n";
    }
};

class Derived : public Virtual {
  public:
    void vmethod() override {
        char buffer[200] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Derived vmethod\n";
    }
};

std::string getStackSizeString(StackSizesSection &section, void *func_addr) {
    uint64_t addr = section.getStackSize(func_addr);
    if (func_addr && addr) {
        return std::to_string(addr);
    }
    return "error";
}

/*
 * Main entry point for the program
 */

int main() {

    std::cout << "=== All functions in .stack_sizes ===" << std::endl;

    StackSizesSection section;

    uint64_t base_addr = section.get_base_address_dl();
    std::cout << "Program base address: 0x" << std::hex << base_addr << std::dec << std::endl;
    std::cout << ".stack_sizes section found" << std::endl;
    std::cout << "Size: " << section.size << " bytes" << std::endl;

    // CHECK: Program base address:
    // CHECK-NEXT: .stack_sizes section found
    // CHECK-NEXT: Size: [[#]] bytes

    // std::cout << "\nVirtual address    | Real address       | Stack size" << std::endl;
    // std::cout << "-------------------------------------------------------------------" << std::endl;

    // const uint8_t *data = section.data;
    // const uint8_t *end = data + section.size;

    // int count = 0;
    // while (data < end) {
    //     uint64_t addr = *(uint64_t *)data;
    //     data += 8;
    //     uint64_t size = MappedELF::decode_uleb128(&data);

    //     printf("0x%016lx | 0x%016lx | %lu bytes\n", addr, addr + base_addr, size);
    //     count++;
    // }

    // std::cout << "\nTotal functions: " << count << std::endl;

    void *addr_func1 = (void *)func1;
    void *addr_func2 = (void *)func2;
    void *addr_func3 = (void *)func3;
    void *addr_main = (void *)main;

    std::cout << "Address of func1 in memory: " << addr_func1 << std::endl;
    std::cout << "Address of func2 in memory: " << addr_func2 << std::endl;
    std::cout << "Address of func3 in memory: " << addr_func3 << std::endl;
    std::cout << "Address of main in memory:  " << addr_main << std::endl;

    // CHECK-NEXT: Address of func1 in memory:
    // CHECK-NEXT: Address of func2 in memory:
    // CHECK-NEXT: Address of func3 in memory:
    // CHECK-NEXT: Address of main in memory:

    std::cout << "\n=== Specific function stack sizes ===" << std::endl;
    std::cout << "func1: " << section.getStackSize(addr_func1) << std::endl;
    std::cout << "func2: " << section.getStackSize(addr_func2) << std::endl;
    std::cout << "func3: " << section.getStackSize(addr_func3) << std::endl;
    std::cout << "main:  " << section.getStackSize(addr_main) << std::endl;

    // O0: === Specific function stack sizes ===
    // O0-NEXT: func1: {{[1-9][0-9]*}}
    // O0-NEXT: func2: {{[1-9][0-9]*}}
    // O0-NEXT: func3: {{[1-9][0-9]*}}
    // O0-NEXT: main:  {{[1-9][0-9]*}}

    // O3: === Specific function stack sizes ===
    // O3-NEXT: func1: 0
    // O3-NEXT: func2: {{[1-9][0-9]*}}
    // O3-NEXT: func3: {{[1-9][0-9]*}}
    // O3-NEXT: main:  {{[1-9][0-9]*}}

    func1();
    func2();
    func3();

    Simple s;
    Virtual v;
    Derived d;

    // CHECK-NEXT: Simple ctor
    // CHECK-NEXT: Virtual ctor
    // CHECK-NEXT: Virtual ctor

    std::cout << "\n=== Simple Class ===\n";
    std::cout << "Constructor: " << getAddr(ctor, &s) << " stack: " << getStackSizeString(section, getAddr(ctor, &s)) << "\n";
    std::cout << "Destructor:  " << getAddr(dtor, &s) << " stack: " << getStackSizeString(section, getAddr(dtor, &s)) << "\n";
    std::cout << "Method:      " << getAddr(&Simple::method) << " stack: " << getStackSizeString(section, getAddr(&Simple::method)) << "\n";

    // O0: === Simple Class ===
    // O0-NEXT: Constructor: 0x{{[0-9a-f]+}} stack: 24
    // O0-NEXT: Destructor:  0x{{[0-9a-f]+}} stack: 24
    // O0-NEXT: Method:      0x{{[0-9a-f]+}} stack: 72

    // O3: === Simple Class ===
    // O3-NEXT: Constructor: 0x{{[0-9a-f]+}} stack: 56
    // O3-NEXT: Destructor:  0x{{[0-9a-f]+}} stack: 56
    // O3-NEXT: Method:      0x{{[0-9a-f]+}} stack: 56

    std::cout << "\n=== Virtual Class ===\n";
    std::cout << "Constructor:    " << getAddr(ctor, &v) << " stack: " << getStackSizeString(section, getAddr(ctor, &v)) << "\n";
    std::cout << "Dtor (wrapper): " << getAddr(dtor, (Virtual *)nullptr)
              << " stack: " << getStackSizeString(section, getAddr(dtor, (Virtual *)nullptr)) << "\n";
    std::cout << "Dtor (vtable):  " << getAddr(dtor, &v) << " stack: " << getStackSizeString(section, getAddr(dtor, &v)) << "\n";
    std::cout << "Method:         " << getAddr(&Virtual::method) << " stack: " << getStackSizeString(section, getAddr(&Virtual::method))
              << "\n";
    std::cout << "VMethod (base): " << getAddr(&Virtual::vmethod, &v)
              << " stack: " << getStackSizeString(section, getAddr(&Virtual::vmethod, &v)) << "\n";
    std::cout << "VMethod (der):  " << getAddr(&Virtual::vmethod, &d)
              << " stack: " << getStackSizeString(section, getAddr(&Virtual::vmethod, &d)) << "\n";

    // O0: === Virtual Class ===
    // O0-NEXT: Constructor:    0x{{[0-9a-f]+}} stack: 24
    // O0-NEXT: Dtor (wrapper): 0x{{[0-9a-f]+}} stack: 24
    // O0-NEXT: Dtor (vtable):  0x{{[0-9a-f]+}} stack: 24
    // O0-NEXT: Method:         0x{{[0-9a-f]+}} stack: 120
    // O0-NEXT: VMethod (base): 0x{{[0-9a-f]+}} stack: 120
    // O0-NEXT: VMethod (der):  0x{{[0-9a-f]+}} stack: 232

    // O3: === Virtual Class ===
    // O3-NEXT: Constructor:    0x{{[0-9a-f]+}} stack: 104
    // O3-NEXT: Dtor (wrapper): 0x{{[0-9a-f]+}} stack: error
    // O3-NEXT: Dtor (vtable):  0x{{[0-9a-f]+}} stack: 104
    // O3-NEXT: Method:         0x{{[0-9a-f]+}} stack: 104
    // O3-NEXT: VMethod (base): 0x{{[0-9a-f]+}} stack: 104
    // O3-NEXT: VMethod (der):  0x{{[0-9a-f]+}} stack: 216

    std::cout << "\n=== Testing Wrappers ===\n";
    alignas(Simple) char buf[sizeof(Simple)];
    auto ctor_fn = reinterpret_cast<void (*)(void *)>(getAddr(ctor, &s));
    auto dtor_fn = reinterpret_cast<void (*)(void *)>(getAddr(dtor, &s));

    std::cout << "Calling ctor wrapper:\n";
    ctor_fn(buf);
    std::cout << "Calling dtor wrapper:\n";
    dtor_fn(buf);

    // CHECK: === Testing Wrappers ===
    // CHECK-NECT: Calling ctor wrapper:
    // CHECK-NECT: Simple ctor
    // CHECK-NECT: Calling dtor wrapper:
    // CHECK-NECT: Simple dtor
    // CHECK-NECT: Virtual dtor
    // CHECK-NECT: Virtual dtor
    // CHECK-NECT: Simple dtor

    return 0;
}

// SU-O0: stack_sizes.cpp:[[#]]:_Z5func1v   88      static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_Z5func2v   280     static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_Z5func3v   1048    static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_Z18getStackSizeStringB5cxx11R17StackSizesSectionPv 120     static

// SU-O0: stack_sizes.cpp:[[#]]:_ZN6SimpleC2Ev      72      static
// SU-O0: stack_sizes.cpp:[[#]]:_ZN7VirtualC2Ev     120     static
// SU-O0: stack_sizes.cpp:[[#]]:_ZN7DerivedC2Ev     24      static
// SU-O0: stack_sizes.cpp:[[#]]:_ZN7VirtualD2Ev     120     static
// SU-O0: stack_sizes.cpp:[[#]]:_ZN6SimpleD2Ev      72      static

// SU-O0: stack_sizes.cpp:[[#]]:_ZN7VirtualD0Ev     24      static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_ZN7Virtual7vmethodEv       120     static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_ZN7DerivedD0Ev     24      static
// SU-O0-NEXT: stack_sizes.cpp:[[#]]:_ZN7Derived7vmethodEv       232     static

//
//
//

// SU-O3: stack_sizes.cpp:[[#]]:_Z5func1v   0       static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_Z5func2v   152     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_Z5func3v   1048    static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_Z18getStackSizeStringB5cxx11R17StackSizesSectionPv 56      static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:main        248     static

// SU-O3: stack_sizes.cpp:[[#]]:_ZN6Simple6methodEv 56      static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN7Virtual6methodEv        104     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN7VirtualD2Ev     104     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN6SimpleD2Ev      56      static

// SU-O3: stack_sizes.cpp:[[#]]:_ZN7VirtualD0Ev     104     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN7Virtual7vmethodEv       104     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN7DerivedD0Ev     104     static
// SU-O3-NEXT: stack_sizes.cpp:[[#]]:_ZN7Derived7vmethodEv       216     static
