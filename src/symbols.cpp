#include "tagtus_api.h"
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

extern "C" int tagtus_hook(void* target, void* repl, void** orig);

static void* (*o_invoke)(void*, void*, void**, void**) = nullptr;
static const char* (*o_method_name)(void*) = nullptr;

static bool g_fly = false, g_speed = false, g_god = false, g_ammo = false, g_norecoil = false;
static char g_filter[64] = "Player";
static char g_last[96] = "invoke: idle";
static int g_hits = 0;
static std::mutex g_lock;
static bool g_hooked = false;

static uintptr_t module_base() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "libil2cpp.so")) continue;
        base = strtoull(line, nullptr, 16);
        break;
    }
    fclose(f);
    return base;
}

static void* resolve(const char* name, uintptr_t va) {
    void* h = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (!h) h = dlopen("libil2cpp.so", RTLD_NOW);
    void* fn = h ? dlsym(h, name) : nullptr;
    if (!fn) fn = dlsym(RTLD_DEFAULT, name);
    if (!fn) {
        uintptr_t b = module_base();
        if (b) fn = (void*)(b + va);
    }
    return fn;
}

static void* hk_invoke(void* method, void* obj, void** params, void** exc) {
    const char* name = (o_method_name && method) ? o_method_name(method) : nullptr;
    if (name && name[0]) {
        bool want = false;
        {
            std::lock_guard<std::mutex> lk(g_lock);
            want = g_fly || g_speed || g_god || g_ammo || g_norecoil;
            if (want && strstr(name, g_filter)) {
                g_hits++;
                snprintf(g_last, sizeof(g_last), "hit %s  #%d", name, g_hits);
            }
        }
    }
    return o_invoke(method, obj, params, exc);
}

static void install_invoke() {
    if (g_hooked) return;
    void* invoke = resolve("il2cpp_runtime_invoke", 0x1f944bc);
    void* mname = resolve("il2cpp_method_get_name", 0x1f943dc);
    if (!invoke || !mname) {
        snprintf(g_last, sizeof(g_last), "invoke hook waiting");
        return;
    }
    o_method_name = (const char* (*)(void*))mname;
    int rc = tagtus_hook(invoke, (void*)hk_invoke, (void**)&o_invoke);
    g_hooked = rc == 0 && o_invoke;
    snprintf(g_last, sizeof(g_last), g_hooked ? "invoke hooked" : "invoke hook failed %d", rc);
    LOGI("%s invoke=%p", g_last, invoke);
}

int tagtus_collect(TagtusActor*, int) { return 0; }

void tagtus_apply(const TagtusToggles* t) {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!t) return;
    g_fly = t->fly; g_speed = t->speed; g_god = t->god;
    g_ammo = t->inf_ammo; g_norecoil = t->no_recoil;
    install_invoke();
}

const char* tagtus_status_line() { return g_last; }
int tagtus_resolved_count() { return g_hooked ? 1 : 0; }
int tagtus_assembly_count() { return 0; }
const char* tagtus_assembly_name(int) { return ""; }
void tagtus_set_filter(const char* s) {
    std::lock_guard<std::mutex> lk(g_lock);
    snprintf(g_filter, sizeof(g_filter), "%s", s ? s : "");
}
const char* tagtus_filter() { return g_filter; }
