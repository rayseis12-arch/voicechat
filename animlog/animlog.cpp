// animlog.dll - registra llamadas a funciones de animacion/hechizos del cliente Turtle WoW (1.12.1.5875)
// NO modifica el juego: solo escribe anim_log.txt (en la carpeta del juego).
// Teclas: F8 = iniciar/parar registro, F9 = escribir una marca (para separar fases)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include "MinHook.h"

extern "C" {
    void* g_orig[8];
    void detour0(); void detour1(); void detour2(); void detour3(); void detour4();
}

static FILE* g_f = nullptr;
static volatile bool g_on = false;
static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)"
};

// Se llama desde el detour: no depende del numero real de argumentos
extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2) {
    if (!g_on || !g_f) return;
    fprintf(g_f, "%lu %s ecx=%08X edx=%08X a1=%08X a2=%08X\n",
            (unsigned long)GetTickCount(), NAMES[id], ecx, edx, a1, a2);
}

#define DETOUR(N) \
asm(".globl _detour" #N "\n" \
    "_detour" #N ":\n" \
    "pushfl\n pushal\n" \
    "pushl 44(%esp)\n" \
    "pushl 44(%esp)\n" \
    "pushl 28(%esp)\n" \
    "pushl 36(%esp)\n" \
    "pushl $" #N "\n" \
    "call _LogCall\n" \
    "addl $20, %esp\n" \
    "popal\n popfl\n" \
    "jmp *_g_orig+" #N "*4\n");
DETOUR(0) DETOUR(1) DETOUR(2) DETOUR(3) DETOUR(4)

static DWORD WINAPI KeyThread(LPVOID) {
    bool f8 = false, f9 = false;
    for (;;) {
        bool a = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool b = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (a && !f8) {
            g_on = !g_on;
            if (g_f) { fprintf(g_f, "---- registro %s ----\n", g_on ? "ACTIVADO" : "DETENIDO"); fflush(g_f); }
        }
        if (b && !f9 && g_f) { fprintf(g_f, "==== MARCA %lu ====\n", (unsigned long)GetTickCount()); fflush(g_f); }
        f8 = a; f9 = b;
        if (g_f && g_on) fflush(g_f);
        Sleep(30);
    }
}

static DWORD WINAPI Boot(LPVOID) {
    while (!FindWindowA("GxWindowClass", NULL)) Sleep(250);
    Sleep(5000);
    g_f = fopen("anim_log.txt", "a");
    if (!g_f) return 0;
    if (MH_Initialize() != MH_OK) return 0;
    struct { uintptr_t addr; void* det; } T[5] = {
        { 0x00710310, (void*)detour0 }, { 0x005FF0C0, (void*)detour1 },
        { 0x005FF290, (void*)detour2 }, { 0x00612320, (void*)detour3 },
        { 0x0060ABE0, (void*)detour4 }
    };
    for (int i = 0; i < 5; i++) {
        MH_STATUS s = MH_CreateHook((LPVOID)T[i].addr, T[i].det, &g_orig[i]);
        fprintf(g_f, "hook %s: %s\n", NAMES[i], MH_StatusToString(s));
    }
    MH_EnableHook(MH_ALL_HOOKS);
    fprintf(g_f, "Listo. F8 = iniciar/parar, F9 = marca.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
