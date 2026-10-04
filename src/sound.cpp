#include <aaudio/AAudio.h>
#include <android/log.h>
#include <cmath>
#include <cstdint>
#include <pthread.h>
#include <vector>

#define TAG "TagtusVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

static AAudioStream* g_stream = nullptr;

struct Clip { std::vector<int16_t> pcm; };
static Clip g_click, g_thock, g_boot;

static Clip make_clip(float freq, float ms, float decay) {
    const int rate = 48000;
    int n = (int)(rate * ms / 1000.f);
    Clip c;
    c.pcm.resize(n);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float env = expf(-t * decay);
        float s = sinf(2.f * 3.1415926f * freq * t) * env;
        s += 0.35f * sinf(2.f * 3.1415926f * freq * 2.02f * t) * env * env;
        c.pcm[i] = (int16_t)(s * 14000.f);
    }
    return c;
}

extern "C" void tagtus_audio_init() {
    g_click = make_clip(980.f, 70.f, 28.f);
    g_thock = make_clip(196.f, 120.f, 16.f);
    g_boot = make_clip(660.f, 160.f, 10.f);
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(b, 1);
    AAudioStreamBuilder_setSampleRate(b, 48000);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    if (AAudioStreamBuilder_openStream(b, &g_stream) == AAUDIO_OK)
        AAudioStream_requestStart(g_stream);
    AAudioStreamBuilder_delete(b);
    LOGI("audio %p", g_stream);
}

struct PlayArg { const int16_t* pcm; int n; };
static void* play_thread(void* p) {
    PlayArg* a = (PlayArg*)p;
    if (g_stream && a->pcm && a->n > 0)
        AAudioStream_write(g_stream, a->pcm, a->n, 20 * 1000 * 1000);
    delete a;
    return nullptr;
}

static void play(const Clip& c) {
    if (!g_stream || c.pcm.empty()) return;
    auto* a = new PlayArg{c.pcm.data(), (int)c.pcm.size()};
    pthread_t t;
    pthread_create(&t, nullptr, play_thread, a);
    pthread_detach(t);
}

extern "C" void tagtus_click() { play(g_click); }
extern "C" void tagtus_thock() { play(g_thock); }
extern "C" void tagtus_boot_sound() { play(g_boot); }
