
// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -fstack-usage -fstack-size-section \
// RUN: -O0 %s -o %p/temp/stack-sizes-O0
// RUN: %t | FileCheck %s -check-prefix=O0
// RUN:FileCheck %s -check-prefix=SU-O0 < %p/temp/stack-sizes-O0.su

// RUN: %clangxx -I%shlibdir -std=c++20 \
// RUN: -fstack-usage -fstack-size-section \
// RUN: -O3 %s -o %p/temp/stack-sizes-O3
// RUN: %t | FileCheck %s -check-prefix=O3
// RUN:FileCheck %s -check-prefix=SU-O3 < %p/temp/stack-sizes-O3.su

// O0-NOT: error
// SU-O0-NOT: error

// O3-NOT: error
// SU-O3-NOT: error

#include <cstdint>
#include <cstring>
#include <elf.h>
#include <fcntl.h>
#include <iostream>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>

static uint64_t decode_uleb128(const uint8_t **ptr) {
    uint64_t result = 0;
    int shift = 0;
    uint8_t byte;

    do {
        byte = **ptr;
        (*ptr)++;
        result |= (uint64_t)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    return result;
}

// Получить базовый адрес загрузки программы
uint64_t get_base_address() {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp)
        return 0;

    char line[256];
    uint64_t base = 0;

    // Ищем первый сегмент исполняемого файла
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "r-xp") || strstr(line, "r--p")) {
            // Формат: адрес-адрес права смещение dev:inode путь
            if (sscanf(line, "%lx-", &base) == 1) {
                fclose(fp);
                return base;
            }
        }
    }

    fclose(fp);
    return 0;
}

// Альтернативный способ через dl_iterate_phdr
struct BaseAddrContext {
    uint64_t base_addr;
    bool found;
};

static int find_base_callback(struct dl_phdr_info *info, size_t size, void *data) {
    BaseAddrContext *ctx = (BaseAddrContext *)data;

    // Главная программа имеет пустое имя
    if (info->dlpi_name == nullptr || info->dlpi_name[0] == '\0') {
        ctx->base_addr = info->dlpi_addr;
        ctx->found = true;
        return 1; // Останавливаем итерацию
    }

    return 0;
}

uint64_t get_base_address_dl() {
    BaseAddrContext ctx = {0, false};
    dl_iterate_phdr(find_base_callback, &ctx);
    return ctx.base_addr;
}

// Получить размер стека функции
uint64_t get_stack_size(void *func_addr) {
    // Получаем базовый адрес
    uint64_t base_addr = get_base_address_dl();

    // Вычисляем относительный адрес функции (как в ELF файле)
    uint64_t real_addr = (uint64_t)func_addr;
    uint64_t relative_addr = real_addr - base_addr;

    // Открываем файл
    int fd = open("/proc/self/exe", O_RDONLY);
    if (fd < 0)
        return 0;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return 0;
    }

    void *mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED)
        return 0;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)mapped;
    Elf64_Shdr *shdr = (Elf64_Shdr *)((char *)mapped + ehdr->e_shoff);
    Elf64_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
    const char *shstrtab_data = (const char *)mapped + shstrtab->sh_offset;

    uint64_t result = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = shstrtab_data + shdr[i].sh_name;

        if (strcmp(name, ".stack_sizes") == 0) {
            const uint8_t *data = (const uint8_t *)mapped + shdr[i].sh_offset;
            const uint8_t *end = data + shdr[i].sh_size;

            while (data < end) {
                uint64_t addr = *(uint64_t *)data;
                data += 8;
                uint64_t size = decode_uleb128(&data);

                // Сравниваем относительный адрес
                if (addr == relative_addr) {
                    result = size;
                    break;
                }
            }
            break;
        }
    }

    munmap(mapped, st.st_size);
    return result;
}

// Вывести все записи
void print_all_stack_sizes() {
    uint64_t base_addr = get_base_address_dl();

    std::cout << "Базовый адрес программы: 0x" << std::hex << base_addr << std::dec << std::endl;

    int fd = open("/proc/self/exe", O_RDONLY);
    if (fd < 0) {
        std::cerr << "Ошибка открытия /proc/self/exe" << std::endl;
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return;
    }

    void *mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED)
        return;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)mapped;
    Elf64_Shdr *shdr = (Elf64_Shdr *)((char *)mapped + ehdr->e_shoff);
    Elf64_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
    const char *shstrtab_data = (const char *)mapped + shstrtab->sh_offset;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = shstrtab_data + shdr[i].sh_name;

        if (strcmp(name, ".stack_sizes") == 0) {
            std::cout << "\nСекция .stack_sizes найдена" << std::endl;
            std::cout << "Размер: " << shdr[i].sh_size << " байт" << std::endl;
            std::cout << "\nВиртуальный адрес   | Реальный адрес      | Размер стека" << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;

            const uint8_t *data = (const uint8_t *)mapped + shdr[i].sh_offset;
            const uint8_t *end = data + shdr[i].sh_size;

            int count = 0;
            while (data < end) {
                uint64_t addr = *(uint64_t *)data;
                data += 8;
                uint64_t size = decode_uleb128(&data);

                printf("0x%016lx | 0x%016lx | %lu байт\n", addr, addr + base_addr, size);
                count++;
            }

            std::cout << "\nВсего функций: " << count << std::endl;
            break;
        }
    }

    munmap(mapped, st.st_size);
}

// Тестовые функции
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

class stack_check {
  public:
    void __attribute__((noinline)) check() {
        char buffer[40] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Constructor" << std::endl;
    }

    static __attribute__((noinline)) void method() {
        char buffer[100] = {0};
        asm volatile("" : : "r,m"(buffer) : "memory");
        std::cout << "Method" << std::endl;
    }
};

/*
 *
 *
 */
struct ctor_t {};
struct dtor_t {};
inline constexpr ctor_t ctor{};
inline constexpr dtor_t dtor{};

template <typename R, typename... Args> void *getAddr(R (*f)(Args...)) { return reinterpret_cast<void *>(f); }

template <typename M> std::enable_if_t<std::is_member_function_pointer_v<M>, void *> getAddr(M m) {
    struct {
        void *a;
        ptrdiff_t d;
    } *p = reinterpret_cast<decltype(p)>(&m);
    return p->a;
}

template <typename M> std::enable_if_t<std::is_member_function_pointer_v<M>, void *> getAddr(M m, void *o) {
    struct {
        void *a;
        ptrdiff_t d;
    } *p = reinterpret_cast<decltype(p)>(&m);
    if (reinterpret_cast<uintptr_t>(p->a) & 1) {
        return o ? (*reinterpret_cast<void ***>(o))[(reinterpret_cast<ptrdiff_t>(p->a) - 1) / sizeof(void *)] : nullptr;
    }
    return p->a;
}

template <typename T> void *getAddr(ctor_t, T * = nullptr) {
    static auto w = [](void *p) { new (p) T(); };
    return reinterpret_cast<void *>(+w);
}

template <typename T> void *getAddr(dtor_t, T *o = nullptr) {
    if constexpr (std::is_polymorphic_v<T>)
        if (o)
            return (*reinterpret_cast<void ***>(o))[1];
    static auto w = [](void *p) { static_cast<T *>(p)->~T(); };
    return reinterpret_cast<void *>(+w);
}

// ============= ПОЛНЫЙ ТЕСТ =============

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

/*
 *
 *
 *
 */

int main() {

    std::cout << "=== Все функции в .stack_sizes ===" << std::endl;
    print_all_stack_sizes();

    std::cout << "\n=== Размеры конкретных функций ===" << std::endl;

    void *addr_func1 = (void *)func1;
    void *addr_func2 = (void *)func2;
    void *addr_func3 = (void *)func3;
    void *addr_main = (void *)main;

    std::cout << "Адрес func1 в памяти: " << addr_func1 << std::endl;
    std::cout << "Адрес func2 в памяти: " << addr_func2 << std::endl;
    std::cout << "Адрес func3 в памяти: " << addr_func3 << std::endl;
    std::cout << "Адрес main в памяти:  " << addr_main << std::endl;

    std::cout << "\nfunc1: " << get_stack_size(addr_func1) << " байт" << std::endl;
    std::cout << "func2: " << get_stack_size(addr_func2) << " байт" << std::endl;
    std::cout << "func3: " << get_stack_size(addr_func3) << " байт" << std::endl;
    std::cout << "main:  " << get_stack_size(addr_main) << " байт" << std::endl;

    stack_check obj;
    obj.check();
    obj.method();

    std::cout << "\nstack_check::check: " << get_stack_size(getAddr(&stack_check::check)) << " байт" << std::endl;
    std::cout << "stack_check::method: " << get_stack_size(getAddr(stack_check::method)) << " байт" << std::endl;
    std::cout << "stack_check::method: " << get_stack_size((void *)&stack_check::method) << " байт" << std::endl;

    func1();
    func2();
    func3();

    Simple s;
    Virtual v;
    Derived d;

    std::cout << "\n\n=== Simple Class ===\n";
    std::cout << "Constructor: " << getAddr(ctor, &s) << " stack: " << get_stack_size(getAddr(ctor, &s)) << "\n";
    std::cout << "Destructor:  " << getAddr(dtor, &s) << " stack: " << get_stack_size(getAddr(dtor, &s)) << "\n";
    std::cout << "Method:      " << getAddr(&Simple::method) << " stack: " << get_stack_size(getAddr(&Simple::method)) << "n";

    std::cout << "\n=== Virtual Class ===\n";
    std::cout << "Constructor:    " << getAddr(ctor, &v) << " stack: " << get_stack_size(getAddr(ctor, &v)) << "\n";
    std::cout << "Dtor (wrapper): " << getAddr(dtor, (Virtual *)nullptr) << " stack: " << get_stack_size(getAddr(dtor, (Virtual *)nullptr))
              << "\n";
    std::cout << "Dtor (vtable):  " << getAddr(dtor, &v) << " stack: " << get_stack_size(getAddr(dtor, &v)) << "\n";
    std::cout << "Method:         " << getAddr(&Virtual::method) << " stack: " << get_stack_size(getAddr(&Virtual::method)) << "\n";
    std::cout << "VMethod (base): " << getAddr(&Virtual::vmethod, &v) << " stack: " << get_stack_size(getAddr(&Virtual::vmethod, &v))
              << "\n";
    std::cout << "VMethod (der):  " << getAddr(&Virtual::vmethod, &d) << " stack: " << get_stack_size(getAddr(&Virtual::vmethod, &d))
              << "\n";

    std::cout << "\n=== Testing Wrappers ===\n";
    alignas(Simple) char buf[sizeof(Simple)];
    auto ctor_fn = reinterpret_cast<void (*)(void *)>(getAddr(ctor, &s));
    auto dtor_fn = reinterpret_cast<void (*)(void *)>(getAddr(dtor, &s));

    std::cout << "Calling ctor wrapper:\n";
    ctor_fn(buf);
    std::cout << "Calling dtor wrapper:\n";
    dtor_fn(buf);

    return 0;
}