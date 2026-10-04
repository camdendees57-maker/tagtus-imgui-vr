#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

static void* alloc_exec(size_t n) {
    void* p = mmap(nullptr, n, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

static void write_abs_jump(uint8_t* dst, void* target) {
    uint32_t ldr = 0x58000051; // ldr x17, #8
    uint32_t br  = 0xD61F0220; // br x17
    memcpy(dst, &ldr, 4);
    memcpy(dst + 4, &br, 4);
    uint64_t addr = (uint64_t)target;
    memcpy(dst + 8, &addr, 8);
}

extern "C" int tagtus_hook(void* target, void* repl, void** orig) {
    if (!target || !repl) return -1;
    long page = sysconf(_SC_PAGESIZE);
    uint8_t* t = (uint8_t*)target;
    void* tramp = alloc_exec(64);
    if (!tramp) return -2;
    memcpy(tramp, t, 16);
    write_abs_jump((uint8_t*)tramp + 16, t + 16);
    __builtin___clear_cache((char*)tramp, (char*)tramp + 32);
    if (orig) *orig = tramp;

    uintptr_t base = (uintptr_t)t & ~((uintptr_t)page - 1);
    if (mprotect((void*)base, (size_t)page * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        return -3;
    write_abs_jump(t, repl);
    __builtin___clear_cache((char*)t, (char*)t + 16);
    mprotect((void*)base, (size_t)page * 2, PROT_READ | PROT_EXEC);
    return 0;
}
