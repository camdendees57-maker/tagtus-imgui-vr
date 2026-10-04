static void* boot(void*) {
    extern void tagtus_vk_install();
    tagtus_vk_install();
    for (int i = 0; i < 80 && !g_booted.load(); ++i) {
        install();
        if (g_booted.load()) break;
        usleep(250 * 1000);
    }
    LOGI("boot done swap=%p dmg=%p", o_swap, o_swap_dmg);
    return nullptr;
}
