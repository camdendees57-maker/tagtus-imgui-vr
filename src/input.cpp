#include <android/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <vector>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

static std::atomic<float> g_x{0}, g_y{0};
static std::atomic<int> g_down{0};
static std::atomic<int> g_toggle{0};
static std::atomic<bool> g_run{true};

float tagtus_touch_x() { return g_x.load(); }
float tagtus_touch_y() { return g_y.load(); }
int tagtus_touch_down() { return g_down.load(); }
int tagtus_consume_toggle() { return g_toggle.exchange(0); }

static void* input_thread(void*) {
    std::vector<int> fds;
    DIR* d = opendir("/dev/input");
    if (d) {
        dirent* e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "event", 5) != 0) continue;
            char path[64];
            snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) fds.push_back(fd);
        }
        closedir(d);
    }
    LOGI("input fds %zu", fds.size());
    while (g_run.load()) {
        for (int fd : fds) {
            input_event ev;
            while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type == EV_KEY && ev.value == 1 &&
                    (ev.code == KEY_VOLUMEDOWN || ev.code == KEY_VOLUMEUP)) {
                    g_toggle.store(1);
                }
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) g_x.store((float)ev.value);
                    if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) g_y.store((float)ev.value);
                }
                if (ev.type == EV_KEY && (ev.code == BTN_TOUCH || ev.code == BTN_LEFT)) {
                    g_down.store(ev.value ? 1 : 0);
                    if (ev.value) g_toggle.store(0);
                }
            }
        }
        usleep(4 * 1000);
    }
    for (int fd : fds) close(fd);
    return nullptr;
}

void tagtus_input_start() {
    pthread_t t;
    pthread_create(&t, nullptr, input_thread, nullptr);
    pthread_detach(t);
}
