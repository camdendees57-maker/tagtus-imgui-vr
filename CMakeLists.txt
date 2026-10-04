cmake_minimum_required(VERSION 3.22)
project(tagtusimgui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp")
  message(FATAL_ERROR "imgui missing. CI clones it.")
endif()

add_library(tagtusimgui SHARED
  src/overlay.cpp
  src/menu.cpp
  src/symbols.cpp
  src/hook.cpp
  src/sound.cpp
  src/input.cpp
  src/sl2.cpp
  src/vk_overlay.cpp
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
  third_party/imgui/backends/imgui_impl_vulkan.cpp
)

target_compile_definitions(tagtusimgui PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_include_directories(tagtusimgui PRIVATE
  include
  third_party/imgui
  third_party/imgui/backends
)
target_link_libraries(tagtusimgui PRIVATE android EGL GLESv3 log dl aaudio vulkan)
