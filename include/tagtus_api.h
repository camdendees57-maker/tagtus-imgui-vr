#pragma once
// Fill these in src/symbols.cpp when the game symbols land.
// Overlay calls them every frame. Return 0 if you got nothing yet.

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
