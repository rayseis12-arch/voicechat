// APLICAR PARCHE SEGURO EN 00712475 (Inversión de Salto Condicional)
    uintptr_t patchAddr = 0x00712475;
    DWORD oldProtect = 0;
    if (VirtualProtect((void*)patchAddr, 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        unsigned char* p = (unsigned char*)patchAddr;
        fprintf(g_f, "Bytes originales en 00712475: %02X %02X %02X %02X\n", p[0], p[1], p[2], p[3]);
        
        // En lugar de NOPs que rompen la instrucción, cambiamos el flag de salto de 0x74/0x75 
        // o ajustamos el offset del salto para priorizar la ejecución de la habilidad.
        // Si p[0] es un salto condicional común (ej. 0x74 o 0x75), invertimos su condición sumando/restando 1 al opcode (ej: 0x74 <-> 0x75)
        if (p[0] == 0x74 || p[0] == 0x75) {
            p[0] ^= 1; // Invierte el salto condicional de forma segura sin romper la longitud
            fprintf(g_f, "Salto condicional invertido exitosamente en 00712475 (Nuevo byte: %02X).\n", p[0]);
        } else {
            // Si es otro tipo de instrucción, aplicamos un salto corto incondicional seguro (0xEB) con un byte de salto
            // O dejamos un stub limpio. Por ahora probamos invirtiendo el primer byte si es condicional.
            fprintf(g_f, "El byte en 00712475 no es un salto estándar conocido, se mantiene intacto para diagnóstico.\n");
        }
        
        VirtualProtect((void*)patchAddr, 8, oldProtect, &oldProtect);
    }
