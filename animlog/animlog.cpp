// animlog.dll - Parche activo de bloqueo de animación en memoria (WoW 1.12.1)
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
static volatile bool g_on = true; // Activado por defecto
static volatile bool g_anim = true; // Activo para capturar AnimIdToIndex
static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Estado para rastrear y proteger la animación del personaje
static struct {
    uint32_t unit;
    uint32_t currentAnim;
    DWORD protectUntil;
} g_playerAnimState = {0, 0, 0};

extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2, uint32_t* stk) {
    if (!g_on || !g_f) return;

    // Interceptamos AnimIdToIndex (id == 0)
    if (id == 0) {
        if ((edx & 0xFFF) != 0x034) return;
        if (a2 < 0x01000000 || (a2 & 7) != 0) return;

        uint32_t unit = a2;
        DWORD now = GetTickCount();

        // Identificamos al jugador (primeras ejecuciones o unidad coincidente)
        if (g_playerAnimState.unit == 0 || g_playerAnimState.unit == unit) {
            g_playerAnimState.unit = unit;

            // 1. Cuando lanzas la habilidad (IDs de animación 212 o 26)
            if (ecx == 212 || ecx == 26) {
                g_playerAnimState.currentAnim = ecx;
                g_playerAnimState.protectUntil = now + 700; // 700ms de ventana blindada
                fprintf(g_f, "%lu [PROTECCIÓN] Habilidad %u iniciada (Protegida hasta %lu)\n", now, ecx, g_playerAnimState.protectUntil);
                fflush(g_f);
                return;
            }

            // 2. Si el motor intenta pisarla con auto-ataque (85 u 88) durante la ventana protegida
            if ((ecx == 85 || ecx == 88) && now < g_playerAnimState.protectUntil) {
                fprintf(g_f, "%lu [BLOQUEO MEMORIA] Intento de corte por %u evitado. Forzando animación %u\n", 
                        now, ecx, g_playerAnimState.currentAnim);
                fflush(g_f);

                // SOBREESCRIBIMOS EL REGISTRO ECX DIRECTAMENTE EN LA PILA (pushal slot)
                // stk - 3 apunta exactamente al slot de ECX guardado por pushal.
                // Al hacer popal, el juego cargará nuestra animación en lugar del ataque automático.
                if (stk) {
                    uint32_t* saved_ecx_slot = stk - 3;
                    *saved_ecx_slot = g_playerAnimState.currentAnim;
                }
                return;
            }

            // Si la protección expiró, actualizamos normalmente
            if (now >= g_playerAnimState.protectUntil) {
                g_playerAnimState.currentAnim = ecx;
            }
        }
    }
}

#define DETOUR(N) \
asm(".globl _detour" #N "\n" \
    "_detour" #N ":\n" \
    "pushfl\n pushal\n" \
    "leal 36(%esp), %eax\n" \
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

DETOUR(0) DETOUR(1) DETOUR(2) DETOUR(3) DETOUR(4) DETOUR(5) DETOUR(6) DETOUR(7) DETOUR(8) DETOUR(9)

static DWORD WINAPI Boot(LPVOID) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "anim_log.txt");
    g_f = fopen(path, "a");
    if (g_f) setvbuf(g_f, NULL, _IOFBF, 1 << 16);

    for (int i = 0; i < 160 && !FindWindowA("GxWindowClass", NULL); i++) Sleep(250);
    Sleep(3000);

    if (MH_Initialize() != MH_OK) return 0;
    struct { uintptr_t addr; void* det; } T[10] = {
        { 0x00710310, (void*)detour0 }, { 0x005FF0C0, (void*)detour1 },
        { 0x005FF290, (void*)detour2 }, { 0x00612320, (void*)detour3 },
        { 0x0060ABE0, (void*)detour4 }, { 0x0060EDF0, (void*)detour5 },
        { 0x0060D150, (void*)detour6 }, { 0x005FF320, (void*)detour7 },
        { 0x00614150, (void*)detour8 }, { 0x0060CE70, (void*)detour9 }
    };
    for (int i = 0; i < 10; i++) {
        MH_CreateHook((LPVOID)T[i].addr, T[i].det, &g_orig[i]);
    }
    MH_EnableHook(MH_ALL_HOOKS);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); CreateThread(NULL, 0, Boot, NULL, 0, NULL); }
    return TRUE;
}
