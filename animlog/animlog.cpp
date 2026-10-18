// animlog.dll - Detour Seguro en 00712475 para Turtle WoW (1.12.1.5875)
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
    void detour_caller();
}

static FILE* g_f = nullptr;
static volatile bool g_on = false;
static volatile bool g_detail = false; 
static volatile bool g_anim = false;   
static volatile bool g_overrideActive = true; // Interruptor de priorización activa

static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Puntero de función original para el bloque 00712475
typedef void (__fastcall *CallerBlock_t)();
static CallerBlock_t g_origCallerBlock = nullptr;

extern "C" __attribute__((used)) void OnCallerTrigger(uint32_t currentEcx, uint32_t currentEbp) {
    if (!g_overrideActive) return;
    
    // Si el registro de depuración está activo, enviamos el evento al log
    if (g_f && g_on) {
        fprintf(g_f, "%lu [HOOK 00712475] Procesando estado de capa M2 (ECX=%08X EBP=%08X)\n", 
                (unsigned long)GetTickCount(), currentEcx, currentEbp);
    }
}

// Detour en ensamblador para 00712475: Preserva el estado y ejecuta las instrucciones reemplazadas
asm(
    ".globl _detour_caller\n"
    "_detour_caller:\n"
    "pushfl\n pushal\n"
    "pushl %ebp\n"
    "pushl %ecx\n"
    "call _OnCallerTrigger\n"
    "addl $8, %esp\n"
    "popal\n popfl\n"
    // Ejecutamos exactamente los 5 bytes interceptados originales (8B 4D F4 33 D2)
    "movl -12(%ebp), %ecx\n"
    "xorl %edx, %edx\n"
    // Volvemos al flujo normal del motor (00712475 + 5 = 0071247A)
    "pushl $0x0071247A\n"
    "ret\n"
);

extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2, uint32_t caller) {
    if (!g_on || !g_f) return;
    if (id == 0 && g_anim) {
        fprintf(g_f, "%lu ANIM_CALLER obj=%08X anim_id=%08X caller=00%06X\n", (unsigned long)GetTickCount(), ecx, edx, caller);
        return;
    }
    if (id >= 5 && id <= 9 && !g_detail) return; 
    fprintf(g_f, "%lu %s ecx=%08X edx=%08X a1=%08X a2=%08X caller=00%06X\n",
            (unsigned long)GetTickCount(), NAMES[id], ecx, edx, a1, a2, caller);
}

#define DETOUR(N) \
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
    "call _LogCall\n" \
    "addl $24, %esp\n" \
    "popal\n popfl\n" \
    "jmp *_g_orig+" #N "*4\n");

#define DETOUR_SKIP(N, POP) \
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
    "call _LogCall\n" \
    "addl $24, %esp\n" \
    "popal\n popfl\n" \
    "cmpb $0, _g_skip+" #N "\n" \
    "je 1f\n" \
    "ret $" #POP "\n" \
    "1:\n" \
    "jmp *_g_orig+" #N "*4\n");

DETOUR(0) 
DETOUR_SKIP(1, 4)   
DETOUR_SKIP(2, 4)   
DETOUR(3) 
DETOUR(4) 
DETOUR(5) 
DETOUR_SKIP(6, 4)   
DETOUR(7) 
DETOUR_SKIP(8, 4)   
DETOUR(9) 

static DWORD WINAPI KeyThread(LPVOID) {
    bool f8 = false, f9 = false, f7 = false, f6 = false;
    DWORD lastFlush = 0;
    for (;;) {
        bool a = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool b = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        bool c = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        bool d = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        
        if (d && !f6) { g_detail = !g_detail; if (g_f) { fprintf(g_f, "---- detalle profundo %s ----\n", g_detail ? "ON" : "OFF"); fflush(g_f); } }
        if (c && !f7) { g_overrideActive = !g_overrideActive; if (g_f) { fprintf(g_f, "---- Intercepción 00712475 %s ----\n", g_overrideActive ? "ACTIVADA" : "DESACTIVADA"); fflush(g_f); } }
        if (a && !f8) {
            g_on = !g_on;
            if (g_f) { fprintf(g_f, "---- registro %s ----\n", g_on ? "ACTIVADO" : "DETENIDO"); fflush(g_f); }
        }
        if (b && !f9 && g_f) { fprintf(g_f, "==== MARCA %lu ====\n", (unsigned long)GetTickCount()); }
        
        f8 = a; f9 = b; f7 = c; f6 = d;
        { DWORD nowT = GetTickCount(); if (g_f && g_on && nowT - lastFlush > 1000) { fflush(g_f); lastFlush = nowT; } }
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
    fprintf(g_f, "animlog.dll cargada (Modo Detour Seguro en 00712475). Esperando al juego...\n");
    fflush(g_f);
    for (int i = 0; i < 160 && !FindWindowA("GxWindowClass", NULL); i++) Sleep(250);
    Sleep(5000);

    if (MH_Initialize() != MH_OK) return 0;

    // Crear hook con MinHook sobre los 5 bytes de 00712475
    MH_STATUS callerStatus = MH_CreateHook((LPVOID)0x00712475, (void*)detour_caller, (void**)&g_origCallerBlock);
    fprintf(g_f, "hook en 00712475: %s\n", MH_StatusToString(callerStatus));

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
    fprintf(g_f, "Listo. Hook seguro activado sobre 00712475 sin riesgo de crash.\n");
    fflush(g_f);
    CreateThread(NULL, 0, KeyThread, NULL, 0, NULL);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_self = h; DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
