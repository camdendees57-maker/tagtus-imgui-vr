static PFN_vkVoidFunction VKAPI_CALL hk_get_device_proc(VkDevice device, const char* name) {
    PFN_vkVoidFunction fn = o_get_dev_proc ? o_get_dev_proc(device, name) : nullptr;
    if (!name) return fn;
    if (strcmp(name, "vkQueuePresentKHR") == 0 && fn) {
        g_dev = device;
        o_present = (PFN_vkQueuePresentKHR)fn;
        LOGI("handing hooked present");
        return (PFN_vkVoidFunction)hk_present;
    }
    if (strcmp(name, "vkCreateSwapchainKHR") == 0 && fn) {
        g_dev = device;
        o_create_swap = (PFN_vkCreateSwapchainKHR)fn;
        return (PFN_vkVoidFunction)hk_create_swap;
    }
    return fn;
}

static void hook_loaded_present() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[512];
    char seen[8][256];
    int nseen = 0;
    while (fgets(line, sizeof(line), f)) {
        char* path = strchr(line, '/');
        if (!path) continue;
        path[strcspn(path, "\n")] = 0;
        if (!strstr(path, "vulkan") && !strstr(path, "libunity") && !strstr(path, "openxr")) continue;
        bool dup = false;
        for (int i = 0; i < nseen; ++i) if (strcmp(seen[i], path) == 0) dup = true;
        if (dup || nseen >= 8) continue;
        snprintf(seen[nseen++], 256, "%s", path);
        void* h = dlopen(path, RTLD_NOW);
        void* sym = h ? dlsym(h, "vkQueuePresentKHR") : nullptr;
        if (sym) LOGI("present in %s hook %d", path, tagtus_hook(sym, (void*)hk_present, (void**)&o_present));
    }
    fclose(f);
}

extern "C" void tagtus_vk_install() {
    void* vk = dlopen("libvulkan.so", RTLD_NOW);
    if (!vk) { LOGE("no libvulkan"); return; }
    void* present = dlsym(vk, "vkQueuePresentKHR");
    void* cdev = dlsym(vk, "vkCreateDevice");
    void* cswap = dlsym(vk, "vkCreateSwapchainKHR");
    void* gdpa = dlsym(vk, "vkGetDeviceProcAddr");
    if (present) LOGI("vk present hook %d", tagtus_hook(present, (void*)hk_present, (void**)&o_present));
    if (cdev) LOGI("vk device hook %d", tagtus_hook(cdev, (void*)hk_create_device, (void**)&o_create_device));
    if (cswap) LOGI("vk swap hook %d", tagtus_hook(cswap, (void*)hk_create_swap, (void**)&o_create_swap));
    if (gdpa) LOGI("vk getproc hook %d", tagtus_hook(gdpa, (void*)hk_get_device_proc, (void**)&o_get_dev_proc));
    hook_loaded_present();
    LOGI("if no vulkan frame 0, move the so from Mods to Libs so it loads before the device");
}
