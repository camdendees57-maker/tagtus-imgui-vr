#include <android/log.h>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

struct TagtusModInfo {
    const char* id;
    const char* version;
    const char* version_long;
};

extern "C" void setup(TagtusModInfo& info) {
    info.id = "TagtusVR";
    info.version = "0.1.0";
    info.version_long = "0.1.0";
    LOGI("scotland2 setup");
}

extern "C" void load() {
    LOGI("scotland2 load");
}

extern "C" void late_load() {
    LOGI("scotland2 late_load");
}
