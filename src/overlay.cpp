#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <atomic>
#include <cstdint>
#include "dobby.h"

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using eglSwapBuffers_t = EGLBoolean (*)(EGLDisplay, EGLSurface);
static eglSwapBuffers_t o_swap = nullptr;
static std::atomic<bool> g_init{false};
static std::atomic<bool> g_hooked{false};

void tagtus_menu_poll_file();
void tagtus_draw_menu();
void tagtus_draw_esp();
bool tagtus_menu_open();
void tagtus_menu_set(bool v);

static void style_tagtus() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    s.WindowRounding = 8.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.06f, 0.94f);
    c[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.10f, 0.06f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.45f, 0.32f, 0.08f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.73f, 0.19f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.91f, 0.73f, 0.19f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.28f, 0.20f, 0.06f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.55f, 0.38f, 0.08f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.45f, 0.32f, 0.08f, 0.7f);
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
    LOGI("imgui up");
}

static EGLBoolean hk_swap(EGLDisplay dpy, EGLSurface surface) {
    ensure_imgui();
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    if (w > 0 && h > 0) {
        tagtus_menu_poll_file();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)w, (float)h);
        io.DeltaTime = 1.0f / 72.0f;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        tagtus_draw_menu();
        tagtus_draw_esp();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    return o_swap(dpy, surface);
}

static void install() {
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    if (!egl) {
        LOGE("libEGL missing");
        return;
    }
    void* sym = dlsym(egl, "eglSwapBuffers");
    if (!sym) {
        LOGE("eglSwapBuffers missing");
        return;
    }
    int rc = DobbyHook(sym, (void*)hk_swap, (void**)&o_swap);
    LOGI("hook rc=%d sym=%p", rc, sym);
    g_hooked.store(rc == 0);
}

static void* boot(void*) {
    for (int i = 0; i < 40 && !g_hooked.load(); ++i) {
        install();
        if (g_hooked.load()) break;
        usleep(250 * 1000);
    }
    return nullptr;
}

__attribute__((constructor)) static void on_load() {
    LOGI("tagtusimgui loaded");
    pthread_t t;
    pthread_create(&t, nullptr, boot, nullptr);
    pthread_detach(t);
}

extern "C" void tagtus_toggle_menu() {
    tagtus_menu_set(!tagtus_menu_open());
}
