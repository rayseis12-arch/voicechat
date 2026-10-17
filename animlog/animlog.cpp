// animlog.dll - registra llamadas a funciones de animacion/hechizos del cliente Turtle WoW (1.12.1.5875)
// NO modifica el juego: solo escribe anim_log.txt (en la carpeta del juego).
// Teclas: F8 = iniciar/parar registro, F9 = marca, F7 = activar/desactivar AnimIdToIndex (ruidosa, apagada por defecto)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "MinHook.h"

extern "C" {
    void* g_orig[16];
    void detour0(); void detour1(); void detour2(); void detour3(); void detour4();
    void detour5(); void detour6(); void detour7(); void detour8(); void detour9();
}

static FILE* g_f = nullptr;
static volatile bool g_on = false;
static volatile bool g_anim = false;   // F7: registrar tambien AnimIdToIndex (muy ruidoso)
static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Se llama desde el detour: no depende del numero real de argumentos
extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2) {
    if (!g_on || !g_f) return;
    if (id == 0 && !g_anim) return;
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
DETOUR(0) DETOUR(1) DETOUR(2) DETOUR(3) DETOUR(4) DETOUR(5) DETOUR(6) DETOUR(7) DETOUR(8) DETOUR(9)

static DWORD WINAPI KeyThread(LPVOID) {
    bool f8 = false, f9 = false, f7 = false;
    for (;;) {
        bool a = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool b = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        bool c = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (c && !f7) { g_anim = !g_anim; if (g_f) { fprintf(g_f, "---- AnimIdToIndex %s ----\n", g_anim ? "ON" : "OFF"); fflush(g_f); } }
        if (a && !f8) {
            g_on = !g_on;
            if (g_f) { fprintf(g_f, "---- registro %s ----\n", g_on ? "ACTIVADO" : "DETENIDO"); fflush(g_f); }
        }
        if (b && !f9 && g_f) { fprintf(g_f, "==== MARCA %lu ====\n", (unsigned long)GetTickCount()); fflush(g_f); }
        f8 = a; f9 = b; f7 = c;
        if (g_f && g_on) fflush(g_f);
        Sleep(30);
    }
}

static HINSTANCE g_self = nullptr;

static DWORD WINAPI Boot(LPVOID) {
    // Crear el log YA, junto a la DLL, para saber si se cargo
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(g_self, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "anim_log.txt");
    g_f = fopen(path, "a");
    if (!g_f) return 0;
    fprintf(g_f, "animlog.dll cargada. Esperando al juego...\n");
    fflush(g_f);
    // Espera la ventana (maximo 40 s) y luego continua igualmente
    for (int i = 0; i < 160 && !FindWindowA("GxWindowClass", NULL); i++) Sleep(250);
    Sleep(5000);
    fprintf(g_f, "Instalando hooks...\n");
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
    fprintf(g_f, "Listo. F8 = iniciar/parar, F9 = marca, F7 = AnimIdToIndex.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_self = h; DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
