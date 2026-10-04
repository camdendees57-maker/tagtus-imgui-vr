# tagtus-imgui-vr

Quest arm64 Dear ImGui overlay. Game symbols stay empty until you drop them in.

Build kicks on push. Artifact name: `libtagtusimgui`.

Plug game code into `src/symbols.cpp`. Header is `include/tagtus_api.h`.

Load with Frida or your injector. First `eglSwapBuffers` paints the menu.
Toggle file: `/data/local/tmp/tagtus_menu` (write `1` or `0`).
