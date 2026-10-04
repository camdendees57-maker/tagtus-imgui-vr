#pragma once

struct TagtusActor {
    float x, y, z;
    float sx, sy;
    int on_screen;
    float dist;
    int team;
    char name[48];
};

struct TagtusToggles {
    int fly;
    int speed;
    int noclip;
    int god;
    int inf_ammo;
    int no_recoil;
    int esp_box;
    int esp_name;
    int esp_snap;
    float speed_mul;
    float fly_speed;
};

int tagtus_collect(TagtusActor* out, int max);
void tagtus_apply(const TagtusToggles* t);
const char* tagtus_status_line();
void tagtus_set_filter(const char* s);
const char* tagtus_filter();

extern "C" {
void tagtus_audio_init();
void tagtus_click();
void tagtus_thock();
void tagtus_boot_sound();
}

void tagtus_input_start();
float tagtus_touch_x();
float tagtus_touch_y();
int tagtus_touch_down();
int tagtus_consume_toggle();
