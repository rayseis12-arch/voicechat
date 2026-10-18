// RADIOGRAFÍA EXACTA DE 00712475
    uintptr_t patchAddr = 0x00712475;
    DWORD oldProtect = 0;
    if (VirtualProtect((void*)patchAddr, 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        unsigned char* p = (unsigned char*)patchAddr;
        fprintf(g_f, "RADIOGRAFIA 00712475: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        VirtualProtect((void*)patchAddr, 8, oldProtect, &oldProtect);
    }
