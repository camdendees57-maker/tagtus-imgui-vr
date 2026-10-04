#include "tagtus_api.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

struct MenuState {
    bool open = true;
    bool fly = false;
    bool speed = false;
    bool noclip = false;
    bool god = false;
    bool inf_ammo = false;
    bool no_recoil = false;
    bool esp_box = false;
    bool esp_name = false;
    bool esp_snap = false;
    float speed_mul = 2.4f;
    float fly_speed = 6.0f;
    char filter[64] = "Player";
};
static MenuState g_menu;

void tagtus_menu_poll_file() {
    FILE* f = fopen("/data/local/tmp/tagtus_menu", "r");
    if (!f) return;
    int v = 1;
    if (fscanf(f, "%d", &v) == 1) g_menu.open = v != 0;
    fclose(f);
}

void tagtus_menu_toggle() {
    g_menu.open = !g_menu.open;
    tagtus_thock();
}

bool tagtus_menu_open() { return g_menu.open; }

static bool click_check(const char* label, bool* v) {
    bool before = *v;
    ImGui::Checkbox(label, v);
    if (ImGui::IsItemClicked()) {
        if (*v != before) tagtus_thock();
        else tagtus_click();
        return true;
    }
    return false;
}

void tagtus_draw_menu() {
    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("tagtus_open", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button(g_menu.open ? "TAGTUS  CLOSE" : "TAGTUS  OPEN", ImVec2(220, 64))) {
        g_menu.open = !g_menu.open;
        tagtus_thock();
    }
    ImGui::End();

    if (!g_menu.open) return;
    ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(24, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.94f);
    if (!ImGui::Begin("TagtusVR", &g_menu.open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(tagtus_status_line());
    ImGui::Text("vol up/down toggles. touch the gold button.");
    ImGui::Separator();
    ImGui::InputText("symbol filter", g_menu.filter, sizeof(g_menu.filter));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        tagtus_set_filter(g_menu.filter);
        tagtus_click();
    }
    click_check("fly", &g_menu.fly);
    click_check("speed", &g_menu.speed);
    ImGui::SliderFloat("speed mul", &g_menu.speed_mul, 1.0f, 8.0f, "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()) tagtus_click();
    ImGui::SliderFloat("fly speed", &g_menu.fly_speed, 1.0f, 20.0f, "%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) tagtus_click();
    click_check("noclip", &g_menu.noclip);
    click_check("god", &g_menu.god);
    click_check("inf ammo", &g_menu.inf_ammo);
    click_check("no recoil", &g_menu.no_recoil);
    ImGui::Separator();
    click_check("esp box", &g_menu.esp_box);
    click_check("esp name", &g_menu.esp_name);
    click_check("esp snap", &g_menu.esp_snap);
    ImGui::End();

    TagtusToggles t{};
    t.fly = g_menu.fly; t.speed = g_menu.speed; t.noclip = g_menu.noclip;
    t.god = g_menu.god; t.inf_ammo = g_menu.inf_ammo; t.no_recoil = g_menu.no_recoil;
    t.esp_box = g_menu.esp_box; t.esp_name = g_menu.esp_name; t.esp_snap = g_menu.esp_snap;
    t.speed_mul = g_menu.speed_mul; t.fly_speed = g_menu.fly_speed;
    tagtus_apply(&t);
    tagtus_set_filter(g_menu.filter);
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
        ImU32 col = IM_COL32(232, 185, 49, 220);
        if (g_menu.esp_box) {
            float h = 70.0f * (8.0f / (a.dist + 8.0f));
            float w = h * 0.45f;
            dl->AddRect(ImVec2(a.sx - w, a.sy - h), ImVec2(a.sx + w, a.sy + h * 0.2f), col, 0.0f, 0, 2.0f);
        }
        if (g_menu.esp_snap) dl->AddLine(origin, ImVec2(a.sx, a.sy), col, 1.5f);
        if (g_menu.esp_name) {
            char line[64];
            snprintf(line, sizeof(line), "%s  %.0fm", a.name, a.dist);
            dl->AddText(ImVec2(a.sx + 8, a.sy - 18), col, line);
        }
    }
}
