static DWORD WINAPI Boot(LPVOID) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(g_self, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "anim_log.txt");
    g_f = fopen(path, "a");
    if (!g_f) return 0;
    setvbuf(g_f, NULL, _IOFBF, 1 << 16);
    fprintf(g_f, "animlog.dll cargada (Analizador Caller 00712475). Esperando al juego...\n");
    fflush(g_f);
    for (int i = 0; i < 160 && !FindWindowA("GxWindowClass", NULL); i++) Sleep(250);
    Sleep(5000);

    // NUEVO: Volcar 32 bytes exactamente alrededor de la direccion 00712475
    fprintf(g_f, "--- VOLCADO DE MEMORIA EN CALLER 00712475 ---\n");
    uintptr_t dumpAddr = 0x00712450; // Un poco antes para ver el contexto completo
    unsigned char* p = (unsigned char*)dumpAddr;
    fprintf(g_f, "Memoria en %08X: ", (unsigned int)dumpAddr);
    for (int i = 0; i < 32; i++) {
        fprintf(g_f, "%02X ", p[i]);
    }
    fprintf(g_f, "\n");
    fflush(g_f);

    if (MH_Initialize() != MH_OK) return 0;
    struct { uintptr_t addr; void* det; } T[10] = {
        { 0x00710310, (void*)detour0 }, { 0x005FF0C0, (void*)detour1 },
        { 0x005FF290, (void*)detour2 }, { 0x00612320, (void*)detour3 },
        { 0x0060ABE0, (void*)detour4 }, { 0x0060EDF0, (void*)detour5 },
        { 0x0060D150, (void*)detour6 }, { 0x005FF320, (void*)detour7 },
        { 0x00614150, (void*)detour8 }, { 0x0060CE70, (void*)detour9 }
    };
    for (int i = 0; i < 10; i++) {
        MH_STATUS s = MH_CreateHook((LPVOID)T[i].addr, T[i].det, &g_orig[i]);
        fprintf(g_f, "hook %s: %s\n", NAMES[i], MH_StatusToString(s));
    }
    MH_EnableHook(MH_ALL_HOOKS);
    fprintf(g_f, "Listo. F8 = iniciar/parar, F7 = Rastreo activo.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}
