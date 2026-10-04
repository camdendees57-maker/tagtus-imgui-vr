#include "tagtus_api.h"
#include "imgui.h"
#include <cstdio>

struct MenuState {
    bool open = true;
    bool fly = false;
    bool speed = false;
    bool noclip = false;
    bool god = false;
    bool inf_ammo = false;
    bool no_recoil = false;
    bool esp_box = true;
    bool esp_name = true;
    bool esp_snap = false;
    float speed_mul = 2.4f;
    float fly_speed = 6.0f;
};
static MenuState g_menu;

void tagtus_menu_poll_file() {
    FILE* f = fopen("/data/local/tmp/tagtus_menu", "r");
    if (!f) return;
    int v = 1;
    if (fscanf(f, "%d", &v) == 1) g_menu.open = v != 0;
    fclose(f);
}

void tagtus_draw_menu() {
    ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (!g_menu.open) return;
    if (!ImGui::Begin("TagtusVR", &g_menu.open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(tagtus_status_line());
    ImGui::Text("export table: 241 il2cpp_*  build b3f8dbd4");
    ImGui::Separator();
    int n = tagtus_assembly_count();
    ImGui::Text("images %d", n);
    for (int i = 0; i < n; ++i)
        ImGui::BulletText("%s", tagtus_assembly_name(i));
    if (n == 0) ImGui::TextDisabled("no images yet. lib not mapped, or domain still cold.");
    ImGui::Separator();
    ImGui::Checkbox("fly", &g_menu.fly);
    ImGui::Checkbox("speed", &g_menu.speed);
    ImGui::SliderFloat("speed mul", &g_menu.speed_mul, 1.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("fly speed", &g_menu.fly_speed, 1.0f, 20.0f, "%.1f");
    ImGui::Checkbox("noclip", &g_menu.noclip);
    ImGui::Checkbox("god", &g_menu.god);
    ImGui::Checkbox("inf ammo", &g_menu.inf_ammo);
    ImGui::Checkbox("no recoil", &g_menu.no_recoil);
    ImGui::Separator();
    ImGui::Checkbox("esp box", &g_menu.esp_box);
    ImGui::Checkbox("esp name", &g_menu.esp_name);
    ImGui::Checkbox("esp snap", &g_menu.esp_snap);
    ImGui::End();

    TagtusToggles t{};
    t.fly = g_menu.fly; t.speed = g_menu.speed; t.noclip = g_menu.noclip;
    t.god = g_menu.god; t.inf_ammo = g_menu.inf_ammo; t.no_recoil = g_menu.no_recoil;
    t.esp_box = g_menu.esp_box; t.esp_name = g_menu.esp_name; t.esp_snap = g_menu.esp_snap;
    t.speed_mul = g_menu.speed_mul; t.fly_speed = g_menu.fly_speed;
    tagtus_apply(&t);
}

void tagtus_draw_esp() {
    if (!g_menu.esp_box && !g_menu.esp_name && !g_menu.esp_snap) return;
    TagtusActor buf[64];
    int n = tagtus_collect(buf, 64);
    if (n < 0) n = 0;
    if (n > 64) n = 64;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 origin(display.x * 0.5f, display.y);
    for (int i = 0; i < n; ++i) {
        const TagtusActor& a = buf[i];
        if (!a.on_screen) continue;
        ImU32 col = a.team ? IM_COL32(232, 185, 49, 220) : IM_COL32(255, 90, 31, 220);
        if (g_menu.esp_box) {
            float h = 70.0f * (8.0f / (a.dist + 8.0f));
            float w = h * 0.45f;
            dl->AddRect(ImVec2(a.sx - w, a.sy - h), ImVec2(a.sx + w, a.sy + h * 0.2f), col, 0.0f, 0, 2.0f);
        }
        if (g_menu.esp_snap)
            dl->AddLine(origin, ImVec2(a.sx, a.sy), col, 1.5f);
        if (g_menu.esp_name) {
            char line[64];
            snprintf(line, sizeof(line), "%s  %.0fm", a.name, a.dist);
            dl->AddText(ImVec2(a.sx + 8, a.sy - 18), col, line);
        }
    }
}

bool tagtus_menu_open() { return g_menu.open; }
void tagtus_menu_set(bool v) { g_menu.open = v; }
