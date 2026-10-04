#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "tagtus_api.h"
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <atomic>
#include <cstdint>
#include <ctime>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

extern "C" int tagtus_hook(void* target, void* repl, void** orig);
void tagtus_menu_poll_file();
void tagtus_menu_toggle();
void tagtus_draw_menu();
void tagtus_draw_esp();

using swap_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
using swap_dmg_t = EGLBoolean (*)(EGLDisplay, EGLSurface, const EGLint*, EGLint);
static swap_t o_swap = nullptr;
static swap_dmg_t o_swap_dmg = nullptr;
static std::atomic<bool> g_init{false};
static std::atomic<bool> g_booted{false};
static std::atomic<int> g_frames{0};
static uint64_t g_last_draw = 0;

static uint64_t now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull;
}

static void style_tagtus() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    s.WindowRounding = 10.0f;
    s.FrameRounding = 6.0f;
    s.GrabRounding = 6.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.06f, 0.94f);
    c[ImGuiCol_Button] = ImVec4(0.55f, 0.40f, 0.08f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.55f, 0.10f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.91f, 0.73f, 0.19f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.73f, 0.19f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.91f, 0.73f, 0.19f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.45f, 0.32f, 0.08f, 1.0f);
}

static void ensure_imgui() {
    if (g_init.load()) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    style_tagtus();
    ImGui_ImplOpenGL3_Init("#version 300 es");
    g_init.store(true);
    tagtus_audio_init();
    tagtus_input_start();
    tagtus_boot_sound();
    LOGI("imgui up");
}

static void pump_input(int w, int h) {
    ImGuiIO& io = ImGui::GetIO();
    float x = tagtus_touch_x();
    float y = tagtus_touch_y();
    if (x > w) x = (x / 4096.f) * w;
    if (y > h) y = (y / 4096.f) * h;
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, tagtus_touch_down() != 0);
    if (tagtus_consume_toggle()) tagtus_menu_toggle();
}

static void draw_frame(EGLDisplay dpy, EGLSurface surface) {
    uint64_t ms = now_ms();
    if (ms - g_last_draw < 8) return;
    g_last_draw = ms;
    ensure_imgui();
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;

    GLint prog = 0, vp[4] = {}, sc[4] = {};
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_SCISSOR_BOX, sc);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, h - 10, w, 10);
    glClearColor(0.91f, 0.73f, 0.19f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    tagtus_menu_poll_file();
    pump_input(w, h);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DeltaTime = 1.f / 72.f;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    tagtus_draw_menu();
    tagtus_draw_esp();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glUseProgram(prog);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glScissor(sc[0], sc[1], sc[2], sc[3]);
    if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    int f = g_frames.fetch_add(1);
    if (f == 0 || f == 30) LOGI("frame %d %dx%d", f, w, h);
}

static EGLBoolean hk_swap(EGLDisplay dpy, EGLSurface surface) {
    draw_frame(dpy, surface);
    return o_swap(dpy, surface);
}
static EGLBoolean hk_swap_dmg(EGLDisplay dpy, EGLSurface surface, const EGLint* rects, EGLint n) {
    draw_frame(dpy, surface);
    return o_swap_dmg(dpy, surface, rects, n);
}

static void install() {
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    if (!egl) return;
    if (!o_swap) {
        void* sym = dlsym(egl, "eglSwapBuffers");
        if (sym) LOGI("swap hook %d", tagtus_hook(sym, (void*)hk_swap, (void**)&o_swap));
    }
    if (!o_swap_dmg) {
        void* sym = dlsym(egl, "eglSwapBuffersWithDamageKHR");
        if (!sym) sym = dlsym(egl, "eglSwapBuffersWithDamageEXT");
        if (sym) LOGI("swapdmg hook %d", tagtus_hook(sym, (void*)hk_swap_dmg, (void**)&o_swap_dmg));
    }
    g_booted.store(o_swap || o_swap_dmg);
}

static void* boot(void*) {
    for (int i = 0; i < 80 && !g_booted.load(); ++i) {
        install();
        if (g_booted.load()) break;
        usleep(250 * 1000);
    }
    LOGI("boot done swap=%p dmg=%p", o_swap, o_swap_dmg);
    return nullptr;
}

__attribute__((constructor)) static void on_load() {
    LOGI("tagtusimgui loaded");
    pthread_t t;
    pthread_create(&t, nullptr, boot, nullptr);
    pthread_detach(t);
}

extern "C" void tagtus_toggle_menu() { tagtus_menu_toggle(); }
