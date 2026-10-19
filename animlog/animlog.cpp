// animlog.dll - Parche de bloqueo de animación en tiempo de ejecución (WoW 1.12.1)
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
static volatile bool g_on = true; // Activado por defecto para aplicar la protección
static const char* NAMES[] = {
    "AnimIdToIndex(00710310)", "SetActiveSpell(005ff0c0)", "ClearSlot(005ff290)",
    "SpellStart(00612320)", "UnitReset(0060abe0)",
    "CancelVisual?(0060edf0)", "ClearSlotA(0060d150)", "ClearSlotB(005ff320)", "ClearSlotC(00614150)", "ForcedAnim?(0060ce70)"
};

// Estructura de estado global para controlar la animación protegida
static struct {
    uint32_t unit;
    uint32_t currentAnim;
    DWORD protectUntil;
} g_playerAnimState = {0, 0, 0};

extern "C" __attribute__((used)) void LogCall(int id, uint32_t ecx, uint32_t edx, uint32_t a1, uint32_t a2, uint32_t* stk) {
    if (!g_on) return;

    // Interceptamos AnimIdToIndex (id == 0)
    if (id == 0) {
        if ((edx & 0xFFF) != 0x034) return;
        if (a2 < 0x01000000 || (a2 & 7) != 0) return;

        uint32_t unit = a2;
        uint32_t requestedAnim = ecx;
        DWORD now = GetTickCount();

        // Si la unidad es nuestro personaje (actualizamos la referencia activa)
        if (g_playerAnimState.unit == 0 || g_playerAnimState.unit == unit) {
            g_playerAnimState.unit = unit;

            // 1. Si se solicita la animación de la habilidad (212 o 26)
            if (requestedAnim == 212 || requestedAnim == 26) {
                g_playerAnimState.currentAnim = requestedAnim;
                // Asignamos una ventana de protección de 650 ms para la animación
                g_playerAnimState.protectUntil = now + 650;

                if (g_f) {
                    fprintf(g_f, "%lu [BLOQUEO ACTIVADO] Habilidad %u iniciada (Protección hasta %lu)\n", 
                            now, requestedAnim, g_playerAnimState.protectUntil);
                }
                return;
            }

            // 2. Si intenta sobreescribir con auto-ataque (85 u 88) mientras la protección está activa
            if ((requestedAnim == 85 || requestedAnim == 88) && now < g_playerAnimState.protectUntil) {
                if (g_f) {
                    fprintf(g_f, "%lu [INTERCEPTADO] Se bloqueó el corte de animación %u por auto-ataque %u\n", 
                            now, g_playerAnimState.currentAnim, requestedAnim);
                }
                // DENEGAMOS la actualización a la ranura: sobreescribimos ECX con la animación protegida
                // Esto fuerza al motor a mantener la animación de la habilidad en pantalla
                ecx = g_playerAnimState.currentAnim;
                return;
            }

            // Si expiró el tiempo de protección, actualizamos el estado
            if (now >= g_playerAnimState.protectUntil) {
                g_playerAnimState.currentAnim = requestedAnim;
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
