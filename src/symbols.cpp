#include "tagtus_api.h"
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// File VAs from this libil2cpp.so (arm64, build-id b3f8dbd4a5b0672ab3c7fca64b9f0c01f1509997).
// Website dumps skip this whole export set. 241 names. dlsym first, base+VA if the table is hidden.
struct ApiSlot { const char* name; uintptr_t va; void* fn; };
static ApiSlot g_api[] = {
    {"il2cpp_init", 0x1f93a3c, nullptr},
    {"il2cpp_init_utf16", 0x1f93a68, nullptr},
    {"il2cpp_domain_get", 0x1f94098, nullptr},
    {"il2cpp_domain_assembly_open", 0x1f9409c, nullptr},
    {"il2cpp_domain_get_assemblies", 0x1f940a4, nullptr},
    {"il2cpp_assembly_get_image", 0x1f93b68, nullptr},
    {"il2cpp_image_get_name", 0x1f94774, nullptr},
    {"il2cpp_image_get_class_count", 0x1f9476c, nullptr},
    {"il2cpp_image_get_class", 0x1f94770, nullptr},
    {"il2cpp_class_get_name", 0x1f93bcc, nullptr},
    {"il2cpp_class_get_namespace", 0x1f93bd0, nullptr},
    {"il2cpp_class_from_name", 0x1f93ba0, nullptr},
    {"il2cpp_class_get_method_from_name", 0x1f93bc8, nullptr},
    {"il2cpp_class_get_field_from_name", 0x1f93bc0, nullptr},
    {"il2cpp_runtime_invoke", 0x1f944b8, nullptr},
    {"il2cpp_field_static_get_value", 0x1f94278, nullptr},
    {"il2cpp_field_static_set_value", 0x1f9427c, nullptr},
    {"il2cpp_thread_attach", 0x1f945b8, nullptr},
    {"il2cpp_string_new", 0x1f9456c, nullptr},
    {"il2cpp_object_new", 0x1f94490, nullptr},
};
static constexpr int kApi = sizeof(g_api) / sizeof(g_api[0]);

using domain_get_t = void* (*)();
using domain_asms_t = void** (*)(void* domain, size_t* size);
using asm_image_t = void* (*)(void* assembly);
using image_name_t = const char* (*)(void* image);
using thread_attach_t = void* (*)(void* domain);

static void* g_il2 = nullptr;
static int g_resolved = 0;
static bool g_ready = false;
static std::mutex g_lock;
static char g_status[160] = "il2cpp: not resolved";
static char g_images[24][64];
static int g_image_n = 0;

static void* slot(const char* name) {
    for (int i = 0; i < kApi; ++i)
        if (strcmp(g_api[i].name, name) == 0) return g_api[i].fn;
    return nullptr;
}

static uintptr_t module_base(const char* needle) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, needle)) continue;
        if (!strstr(line, "r-xp") && !strstr(line, "r--p")) continue;
        base = strtoull(line, nullptr, 16);
        break;
    }
    fclose(f);
    return base;
}

static void resolve_locked() {
    if (g_ready) return;
    g_il2 = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (!g_il2) g_il2 = dlopen("libil2cpp.so", RTLD_NOW);
    uintptr_t base = module_base("libil2cpp.so");
    g_resolved = 0;
    for (int i = 0; i < kApi; ++i) {
        void* fn = g_il2 ? dlsym(g_il2, g_api[i].name) : nullptr;
        if (!fn) fn = dlsym(RTLD_DEFAULT, g_api[i].name);
        if (!fn && base) fn = (void*)(base + g_api[i].va);
        g_api[i].fn = fn;
        if (fn) g_resolved++;
    }
    auto domain_get = (domain_get_t)slot("il2cpp_domain_get");
    auto get_asms = (domain_asms_t)slot("il2cpp_domain_get_assemblies");
    auto get_image = (asm_image_t)slot("il2cpp_assembly_get_image");
    auto get_name = (image_name_t)slot("il2cpp_image_get_name");
    auto attach = (thread_attach_t)slot("il2cpp_thread_attach");
    void* domain = domain_get ? domain_get() : nullptr;
    if (domain && attach) attach(domain);
    g_image_n = 0;
    size_t n = 0;
    void** asms = (domain && get_asms) ? get_asms(domain, &n) : nullptr;
    if (asms) {
        for (size_t i = 0; i < n && g_image_n < 24; ++i) {
            void* img = get_image ? get_image(asms[i]) : nullptr;
            const char* name = (img && get_name) ? get_name(img) : "?";
            snprintf(g_images[g_image_n], 64, "%s", name ? name : "?");
            g_image_n++;
        }
    }
    snprintf(g_status, sizeof(g_status), "il2cpp %d/%d  domain=%p  images=%d  base=%p",
             g_resolved, kApi, domain, g_image_n, (void*)base);
    LOGI("%s", g_status);
    g_ready = g_resolved > 0;
}

int tagtus_collect(TagtusActor* out, int max) {
    (void)out; (void)max;
    return 0;
}

void tagtus_apply(const TagtusToggles* t) {
    (void)t;
    std::lock_guard<std::mutex> lk(g_lock);
    resolve_locked();
}

const char* tagtus_status_line() {
    std::lock_guard<std::mutex> lk(g_lock);
    resolve_locked();
    return g_status;
}
int tagtus_resolved_count() { return g_resolved; }
int tagtus_assembly_count() { return g_image_n; }
const char* tagtus_assembly_name(int index) {
    if (index < 0 || index >= g_image_n) return "";
    return g_images[index];
}
