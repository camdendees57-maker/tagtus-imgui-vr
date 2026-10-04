#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "tagtus_api.h"
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

extern "C" int tagtus_hook(void* target, void* repl, void** orig);
void tagtus_menu_poll_file();
void tagtus_menu_toggle();
void tagtus_draw_menu();
void tagtus_draw_esp();
