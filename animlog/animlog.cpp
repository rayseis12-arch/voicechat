// animlog.dll - Filtro Seguro en AnimIdToIndex (Turtle WoW 1.12.1.5875)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "MinHook.h"

extern "C" {
    void* g_orig[16];
    volatile unsigned char g_skip[16];
    void detour0(); void detour1(); void detour2(); void detour3(); void detour4();
    void detour5(); void detour6(); void detour7(); void detour8(); void detour9();
}

static FILE* g_f = nullptr;
static volatile bool g_on = false;
static volatile bool g_detail = false; 
static volatile bool g_anim = false;   
static volatile bool g_filterActive = true; 

static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Wrapper en C++ con convención estándar (__cdecl) para evitar problemas de decorado con __fastcall en MinGW
extern "C" __attribute__((used)) int HandleAnimIdToIndex(uint32_t pThis, uint32_t animId) {
    // Llamada a la función original usando el puntero original de MinHook
    typedef int (*Orig_t)(uint32_t, uint32_t);
    Orig_t origFunc = (Orig_t)g_orig[0];
    
    int realIndex = origFunc(pThis, animId);

    if (g_filterActive && g_f && g_on && g_anim) {
        fprintf(g_f, "%lu [ANIM INDEX] obj=%08X animId=%08X -> index=%d\n", (unsigned long)GetTickCount(), pThis, animId, realIndex);
    }

    return realIndex;
}

// Detour en ensamblador puro con el prefijo correcto _detour0 para el enlazador
asm(
    ".globl _detour0\n"
    "_detour0:\n"
    "pushl %edx\n"          // Preservamos EDX (animId en el fastcall original)
    "pushl %ecx\n"          // Preservamos ECX (this pointer)
    "call _HandleAnimIdToIndex\n"
    "addl $8, %esp\n"
    "ret\n"
);

extern "C" __attribute__((used)) uint32_t FilterCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2, uint32_t caller) {
    if (g_on && g_f) {
        if (id >= 5 && id <= 9 && g_detail) {
            fprintf(g_f, "%lu %s ecx=%08X edx=%08X a1=%08X a2=%08X caller=00%06X\n",
                    (unsigned long)GetTickCount(), NAMES[id], ecx, edx, a1, a2, caller);
        }
    }
    return 0;
}

#define DETOUR_GENERIC(N) \
asm(".globl _detour" #N "\n" \
    "_detour" #N ":\n" \
    "movl (%esp), %eax\n" \
    "pushfl\n pushal\n" \
    "pushl %eax\n" \
    "pushl 48(%esp)\n" \
    "pushl 48(%esp)\n" \
    "pushl 32(%esp)\n" \
    "pushl 40(%esp)\n" \
    "pushl $" #N "\n" \
    "call _FilterCall\n" \
    "addl $24, %esp\n" \
    "popal\n popfl\n" \
    "jmp *_g_orig+" #N "*4\n");

DETOUR_GENERIC(1)   
DETOUR_GENERIC(2)   
DETOUR_GENERIC(3)   
DETOUR_GENERIC(4)   
DETOUR_GENERIC(5)   
DETOUR_GENERIC(6)   
DETOUR_GENERIC(7)   
DETOUR_GENERIC(8)   
DETOUR_GENERIC(9)   

static DWORD WINAPI KeyThread(LPVOID) {
    bool f8 = false, f9 = false, f7 = false, f6 = false;
    DWORD lastFlush = 0;
    for (;;) {
        bool a = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool b = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        bool c = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        bool d = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        
        if (d && !f6) { g_detail = !g_detail; if (g_f) { fprintf(g_f, "---- detalle profundo %s ----\n", g_detail ? "ON" : "OFF"); fflush(g_f); } }
        if (c && !f7) { 
            g_filterActive = !g_filterActive; 
            if (g_f) { 
                fprintf(g_f, "---- Filtro AnimIdToIndex %s ----\n", g_filterActive ? "ACTIVADO" : "DESACTIVADO"); 
                fflush(g_f); 
            } 
        }
        if (a && !f8) {
            g_on = !g_on;
            if (g_f) { fprintf(g_f, "---- registro %s ----\n", g_on ? "ACTIVADO" : "DETENIDO"); fflush(g_f); }
        }
        if (b && !f9 && g_f) { fprintf(g_f, "==== MARCA %lu ====\n", (unsigned long)GetTickCount()); }
        
        f8 = a; f9 = b; f7 = c; f6 = d;
        { 
            DWORD nowT = GetTickCount(); 
            if (g_f && g_on && nowT - lastFlush > 1000) { 
                fflush(g_f); 
                lastFlush = nowT; 
            } 
        }
        Sleep(30);
    }
}

static HINSTANCE g_self = nullptr;

static DWORD WINAPI Boot(LPVOID) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(g_self, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "anim_log.txt");
    g_f = fopen(path, "a");
    if (!g_f) return 0;
    setvbuf(g_f, NULL, _IOFBF, 1 << 16);
    fprintf(g_f, "animlog.dll cargada (Filtro AnimIdToIndex corregido). Esperando al juego...\n");
    fflush(g_f);
    for (int i = 0; i < 160 && !FindWindowA("GxWindowClass", NULL); i++) Sleep(250);
    Sleep(5000);

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
    fprintf(g_f, "Listo. Hook configurado correctamente sin errores de enlazador.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_self = h; DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
