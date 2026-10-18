// animlog.dll - registra llamadas a funciones de animacion/hechizos del cliente Turtle WoW (1.12.1.5875)
// NO modifica el juego: solo escribe anim_log.txt (en la carpeta del juego).
// EXPERIMENTAL: F1 omite ClearSlot, F2 omite SetActiveSpell, F3 omite ForcedAnim, F4 omite ClearSlotA, F5 omite CancelVisual? (pitido agudo = ON, grave = OFF).
// Teclas: F8 = iniciar/parar registro, F9 = marca, F7 = AnimIdToIndex (sin repetidos), F6 = detalle de auras (ruidoso)
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
static volatile bool g_detail = false; // F6: detalle de auras (ClearSlotA/B/C y CancelVisual), muy ruidoso
static volatile bool g_anim = false;   // F7: registrar tambien AnimIdToIndex (muy ruidoso)
static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Se llama desde el detour: no depende del numero real de argumentos
extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2) {
    if (!g_on || !g_f) return;
    if (id >= 5 && id <= 9 && !g_detail) return;   // ruido de otras unidades (auras, ForcedAnim)
    if (id == 0) {                                  // AnimIdToIndex: CAMBIOS de animacion por unidad (F7)
        if (!g_anim) return;
        if ((edx & 0xFFF) != 0x034) return;         // solo la consulta de "animacion pedida"
        if (a2 < 0x01000000 || (a2 & 7) != 0) return;
        uint32_t unit = a2;
        uint32_t h = unit * 2654435761u; h ^= h >> 15;
        static struct { uint32_t unit; uint32_t cur; bool used; } st[2048];
        auto& e = st[h & 2047];
        if (e.used && e.unit != unit) e.used = false;
        if (!e.used) { e.used = true; e.unit = unit; e.cur = 0; }
        if (e.cur == ecx) return;                   // sin cambio: no se registra
        fprintf(g_f, "%lu ANIM obj=%08X modelo=%04X %u -> %u\n", (unsigned long)GetTickCount(), unit, edx >> 16, e.cur, ecx);
        e.cur = ecx;
        return;
    }
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
#define DETOUR_SKIP(N, POP) \
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
    "cmpb $0, _g_skip+" #N "\n" \
    "je 1f\n" \
    "ret $" #POP "\n" \
    "1:\n" \
    "jmp *_g_orig+" #N "*4\n");

DETOUR(0) 
DETOUR_SKIP(1, 4)   // SetActiveSpell (F2)
DETOUR_SKIP(2, 4)   // ClearSlot (F1)
DETOUR(3) 
DETOUR(4) 
DETOUR_SKIP(5, 4)   // CancelVisual? (F5) - NUEVO INTERRUPTOR
DETOUR_SKIP(6, 4)   // ClearSlotA (F4)[cite: 1]
DETOUR(7) 
DETOUR(8) 
DETOUR_SKIP(9, 4)   // ForcedAnim? (F3)[cite: 1]

static DWORD WINAPI KeyThread(LPVOID) {
    bool f8 = false, f9 = false, f7 = false, f6 = false, f1 = false, f2 = false, f3 = false, f4 = false, f5 = false;
    DWORD lastFlush = 0;
    for (;;) {
        bool a = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool b = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        bool c = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        bool d = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        bool k1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        bool k2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        bool k3 = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
        bool k4 = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        bool k5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        
        if (k1 && !f1) { g_skip[2] = !g_skip[2]; if (g_f) fprintf(g_f, "---- EXPERIMENTO ClearSlot omitido: %s ----\n", g_skip[2] ? "ON" : "OFF"); Beep(g_skip[2] ? 1200 : 500, 120); }[cite: 1]
        if (k2 && !f2) { g_skip[1] = !g_skip[1]; if (g_f) fprintf(g_f, "---- EXPERIMENTO SetActiveSpell omitido: %s ----\n", g_skip[1] ? "ON" : "OFF"); Beep(g_skip[1] ? 1200 : 500, 120); }[cite: 1]
        if (k3 && !f3) { g_skip[9] = !g_skip[9]; if (g_f) fprintf(g_f, "---- EXPERIMENTO ForcedAnim omitido: %s ----\n", g_skip[9] ? "ON" : "OFF"); Beep(g_skip[9] ? 1200 : 500, 120); }[cite: 1]
        if (k4 && !f4) { g_skip[6] = !g_skip[6]; if (g_f) fprintf(g_f, "---- EXPERIMENTO ClearSlotA omitido: %s ----\n", g_skip[6] ? "ON" : "OFF"); Beep(g_skip[6] ? 1200 : 500, 120); }[cite: 1]
        if (k5 && !f5) { g_skip[5] = !g_skip[5]; if (g_f) fprintf(g_f, "---- EXPERIMENTO CancelVisual omitido: %s ----\n", g_skip[5] ? "ON" : "OFF"); Beep(g_skip[5] ? 1200 : 500, 120); }
        
        if (d && !f6) { g_detail = !g_detail; if (g_f) { fprintf(g_f, "---- detalle auras %s ----\n", g_detail ? "ON" : "OFF"); fflush(g_f); } }[cite: 1]
        if (c && !f7) { g_anim = !g_anim; if (g_f) { fprintf(g_f, "---- AnimIdToIndex %s ----\n", g_anim ? "ON" : "OFF"); fflush(g_f); } }[cite: 1]
        if (a && !f8) {
            g_on = !g_on;
            if (g_f) { fprintf(g_f, "---- registro %s ----\n", g_on ? "ACTIVADO" : "DETENIDO"); fflush(g_f); }[cite: 1]
        }
        if (b && !f9 && g_f) { fprintf(g_f, "==== MARCA %lu ====\n", (unsigned long)GetTickCount()); }[cite: 1]
        
        f8 = a; f9 = b; f7 = c; f6 = d; f1 = k1; f2 = k2; f3 = k3; f4 = k4; f5 = k5;
        { DWORD nowT = GetTickCount(); if (g_f && g_on && nowT - lastFlush > 1000) { fflush(g_f); lastFlush = nowT; } }[cite: 1]
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
    fprintf(g_f, "animlog.dll cargada. Esperando al juego...\n");
    fflush(g_f);
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
    fprintf(g_f, "Listo. F8 = iniciar/parar, F9 = marca, F7 = AnimIdToIndex, F6 = detalle auras, F1 = ClearSlot, F2 = SetActiveSpell, F3 = ForcedAnim, F4 = ClearSlotA, F5 = CancelVisual.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_self = h; DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
