#include "imgui.h"
#include "imgui_impl_vulkan.h"
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
int tagtus_consume_toggle();
void tagtus_audio_init();
void tagtus_input_start();
void tagtus_boot_sound();

static PFN_vkQueuePresentKHR o_present = nullptr;
static PFN_vkCreateDevice o_create_device = nullptr;
static PFN_vkCreateSwapchainKHR o_create_swap = nullptr;
static PFN_vkGetDeviceProcAddr o_get_dev_proc = nullptr;

static VkInstance g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_phys = VK_NULL_HANDLE;
static VkDevice g_dev = VK_NULL_HANDLE;
static VkQueue g_queue = VK_NULL_HANDLE;
static uint32_t g_family = 0;
static VkSwapchainKHR g_swap = VK_NULL_HANDLE;
static VkRenderPass g_pass = VK_NULL_HANDLE;
static VkCommandPool g_pool = VK_NULL_HANDLE;
static VkDescriptorPool g_dpool = VK_NULL_HANDLE;
static std::vector<VkImage> g_images;
static std::vector<VkImageView> g_views;
static std::vector<VkFramebuffer> g_fbs;
static std::vector<VkCommandBuffer> g_cmds;
static VkExtent2D g_extent{};
static VkFormat g_format = VK_FORMAT_B8G8R8A8_UNORM;
static bool g_ready = false;
static int g_frames = 0;

static void style_tagtus() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10.f;
    s.Colors[ImGuiCol_Button] = ImVec4(0.55f, 0.40f, 0.08f, 1.f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.91f, 0.73f, 0.19f, 1.f);
    s.Colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.73f, 0.19f, 1.f);
}

static void kill_frames() {
    if (!g_dev) return;
    vkDeviceWaitIdle(g_dev);
    for (auto fb : g_fbs) vkDestroyFramebuffer(g_dev, fb, nullptr);
    for (auto v : g_views) vkDestroyImageView(g_dev, v, nullptr);
    g_fbs.clear(); g_views.clear(); g_images.clear(); g_cmds.clear();
}

static bool build_frames() {
    if (!g_dev || !g_swap || !g_pass) return false;
    uint32_t n = 0;
    vkGetSwapchainImagesKHR(g_dev, g_swap, &n, nullptr);
    if (!n) return false;
    g_images.resize(n);
    vkGetSwapchainImagesKHR(g_dev, g_swap, &n, g_images.data());
    g_views.resize(n); g_fbs.resize(n); g_cmds.resize(n);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = g_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = n;
    if (vkAllocateCommandBuffers(g_dev, &ai, g_cmds.data()) != VK_SUCCESS) return false;
    for (uint32_t i = 0; i < n; ++i) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = g_images[i]; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = g_format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(g_dev, &vi, nullptr, &g_views[i]);
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = g_pass; fi.attachmentCount = 1; fi.pAttachments = &g_views[i];
        fi.width = g_extent.width ? g_extent.width : 1024;
        fi.height = g_extent.height ? g_extent.height : 1024;
        fi.layers = 1;
        vkCreateFramebuffer(g_dev, &fi, nullptr, &g_fbs[i]);
    }
    return true;
}

static bool init_imgui_vk() {
    if (g_ready || !g_dev || !g_queue) return g_ready;
    VkAttachmentDescription att{};
    att.format = g_format; att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount = 1; sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1; rp.pAttachments = &att; rp.subpassCount = 1; rp.pSubpasses = &sub;
    if (vkCreateRenderPass(g_dev, &rp, nullptr, &g_pass) != VK_SUCCESS) return false;
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pi.queueFamilyIndex = g_family;
    if (vkCreateCommandPool(g_dev, &pi, nullptr, &g_pool) != VK_SUCCESS) return false;
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32}};
    VkDescriptorPoolCreateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    di.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; di.maxSets = 32; di.poolSizeCount = 1; di.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(g_dev, &di, nullptr, &g_dpool) != VK_SUCCESS) return false;
    if (!build_frames()) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    style_tagtus();
    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = g_instance; info.PhysicalDevice = g_phys; info.Device = g_dev;
    info.QueueFamily = g_family; info.Queue = g_queue; info.DescriptorPool = g_dpool;
    info.MinImageCount = (uint32_t)g_images.size(); info.ImageCount = (uint32_t)g_images.size();
    info.RenderPass = g_pass; info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&info)) return false;
    tagtus_audio_init();
    tagtus_input_start();
    tagtus_boot_sound();
    g_ready = true;
    LOGI("vulkan imgui up images=%zu", g_images.size());
    return true;
}

static void draw_vk(uint32_t image_index) {
    if (!init_imgui_vk()) return;
    if (image_index >= g_cmds.size()) return;
    tagtus_menu_poll_file();
    if (tagtus_consume_toggle()) tagtus_menu_toggle();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(g_extent.width ? g_extent.width : 1920), (float)(g_extent.height ? g_extent.height : 1080));
    io.DeltaTime = 1.f / 72.f;
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    tagtus_draw_menu();
    tagtus_draw_esp();
    ImGui::Render();
    VkCommandBuffer cmd = g_cmds[image_index];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = g_pass; rbi.framebuffer = g_fbs[image_index];
    rbi.renderArea.extent = g_extent.width ? g_extent : VkExtent2D{1920, 1080};
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_queue);
    if (g_frames++ == 0) LOGI("vulkan frame 0");
}

static VkResult VKAPI_CALL hk_present(VkQueue queue, const VkPresentInfoKHR* info) {
    g_queue = queue;
    uint32_t idx = (info && info->pImageIndices) ? info->pImageIndices[0] : 0;
    if (info && info->swapchainCount && info->pSwapchains) g_swap = info->pSwapchains[0];
    draw_vk(idx);
    return o_present(queue, info);
}

static VkResult VKAPI_CALL hk_create_swap(VkDevice device, const VkSwapchainCreateInfoKHR* ci, const VkAllocationCallbacks* alloc, VkSwapchainKHR* swap) {
    g_dev = device;
    if (ci) { g_extent = ci->imageExtent; g_format = ci->imageFormat; }
    VkResult r = o_create_swap(device, ci, alloc, swap);
    if (r == VK_SUCCESS && swap) {
        g_swap = *swap;
        kill_frames();
        g_ready = false;
        LOGI("swapchain %ux%u fmt=%d", g_extent.width, g_extent.height, (int)g_format);
    }
    return r;
}

static void grab_queue(VkDevice device) {
    if (!g_instance || !g_phys) return;
    auto get = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_DEFAULT, "vkGetInstanceProcAddr");
    if (!get) return;
    auto enumq = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get(g_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    if (!enumq) return;
    uint32_t n = 0; enumq(g_phys, &n, nullptr);
    g_family = 0;
    vkGetDeviceQueue(device, g_family, 0, &g_queue);
}

static VkResult VKAPI_CALL hk_create_device(VkPhysicalDevice phys, const VkDeviceCreateInfo* ci, const VkAllocationCallbacks* alloc, VkDevice* dev) {
    g_phys = phys;
    VkResult r = o_create_device(phys, ci, alloc, dev);
    if (r == VK_SUCCESS && dev) {
        g_dev = *dev;
        grab_queue(*dev);
        LOGI("vk device %p queue %p", g_dev, g_queue);
    }
    return r;
}

extern "C" void tagtus_vk_install() {
    void* vk = dlopen("libvulkan.so", RTLD_NOW);
    if (!vk) { LOGE("no libvulkan"); return; }
    void* present = dlsym(vk, "vkQueuePresentKHR");
    void* cdev = dlsym(vk, "vkCreateDevice");
    void* cswap = dlsym(vk, "vkCreateSwapchainKHR");
    if (present) LOGI("vk present hook %d", tagtus_hook(present, (void*)hk_present, (void**)&o_present));
    if (cdev) LOGI("vk device hook %d", tagtus_hook(cdev, (void*)hk_create_device, (void**)&o_create_device));
    if (cswap) LOGI("vk swap hook %d", tagtus_hook(cswap, (void*)hk_create_swap, (void**)&o_create_swap));
    (void)o_get_dev_proc;
}
