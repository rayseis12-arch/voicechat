// voicechat.dll - chat de voz para el cliente WoW 1.12.1 (build 5875), 32 bits.
// Captura microfono (miniaudio) -> Opus -> UDP -> relay -> Opus -> altavoces.
// Expone a Lua: VC_Init(nombre, ip, puerto), VC_SetRoom(sala), VC_PTT(0/1), VC_Speaking()
//
// Compilar (MinGW 32 bits):
//   i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -o voicechat.dll voicechat.cpp \
//       -I. -lopus -lws2_32 -lole32
// Necesitas miniaudio.h (https://miniaud.io) junto a este archivo y libopus de 32 bits.

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <opus/opus.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <utility>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// ---------- Direcciones del cliente 1.12.1 (5875). VERIFICA con tu build ----------
typedef void  (__cdecl *FnRegister)(const char*, int (__cdecl*)(void*));
typedef const char* (__cdecl *FnToString)(void*, int);
typedef double (__cdecl *FnToNumber)(void*, int);
typedef void  (__cdecl *FnPushString)(void*, const char*);
typedef void  (__cdecl *FnPushNumber)(void*, double);

static FnRegister   FrameScript_RegisterFunction = (FnRegister)0x00704120;
static FnToString   L_tostring   = (FnToString)0x006F3690;
static FnToNumber   L_tonumber   = (FnToNumber)0x006F3620;
static FnPushString L_pushstring = (FnPushString)0x006F3890;
static FnPushNumber L_pushnumber = (FnPushNumber)0x006F3810;

// ---------- Estado ----------
static const int RATE = 16000;
static const int FRAME = 320;           // 20 ms
static const size_t MAX_BUFFER = RATE * 3 / 10;  // 300 ms por peer

struct Peer { OpusDecoder* dec; std::deque<int16_t> pcm; DWORD last; };

static std::string g_name;
static sockaddr_in g_srv;
static SOCKET g_sock = INVALID_SOCKET;
static std::atomic<bool> g_ptt{false}, g_run{false};
static std::atomic<uint32_t> g_room{0};
static OpusEncoder* g_enc = nullptr;
static std::map<std::string, Peer> g_peers;
static std::mutex g_mx;
static ma_device g_cap, g_play;
static std::vector<int16_t> g_accum;
static bool g_inited = false;

static uint32_t Fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h ? h : 1;
}

static void SendPacket(uint8_t type, const uint8_t* payload, int len) {
    uint32_t room = g_room.load();
    if (room == 0 || g_sock == INVALID_SOCKET) return;
    uint8_t buf[1500];
    int n = 0;
    buf[n++] = type;
    memcpy(buf + n, &room, 4); n += 4;
    buf[n++] = (uint8_t)g_name.size();
    memcpy(buf + n, g_name.data(), g_name.size()); n += (int)g_name.size();
    if (len > 0 && n + len < (int)sizeof(buf)) { memcpy(buf + n, payload, len); n += len; }
    sendto(g_sock, (const char*)buf, n, 0, (sockaddr*)&g_srv, sizeof(g_srv));
}

// ---------- Audio ----------
static void CaptureCb(ma_device*, void*, const void* in, ma_uint32 frames) {
    if (!g_ptt.load() || !g_enc) { g_accum.clear(); return; }
    const int16_t* s = (const int16_t*)in;
    g_accum.insert(g_accum.end(), s, s + frames);
    while ((int)g_accum.size() >= FRAME) {
        uint8_t out[400];
        int n = opus_encode(g_enc, g_accum.data(), FRAME, out, sizeof(out));
        if (n > 0) SendPacket(1, out, n);
        g_accum.erase(g_accum.begin(), g_accum.begin() + FRAME);
    }
}

static void PlayCb(ma_device*, void* out, const void*, ma_uint32 frames) {
    int16_t* o = (int16_t*)out;
    memset(o, 0, frames * sizeof(int16_t));
    std::lock_guard<std::mutex> lk(g_mx);
    for (auto& kv : g_peers) {
        auto& q = kv.second.pcm;
        ma_uint32 n = (ma_uint32)std::min<size_t>(frames, q.size());
        for (ma_uint32 i = 0; i < n; i++) {
            int v = o[i] + q.front(); q.pop_front();
            o[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }
    }
}

// ---------- Red ----------
static DWORD WINAPI RecvThread(LPVOID) {
    uint8_t buf[1500];
    while (g_run) {
        int n = recv(g_sock, (char*)buf, sizeof(buf), 0);
        if (n < 7) { if (n < 0) Sleep(5); continue; }
        if (buf[0] != 1) continue;
        int nl = buf[5];
        if (6 + nl >= n) continue;
        std::string who((char*)buf + 6, nl);
        if (who == g_name) continue;
        int16_t pcm[FRAME * 6];
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = g_peers.find(who);
        if (it == g_peers.end()) {
            int err; Peer p; p.dec = opus_decoder_create(RATE, 1, &err); p.last = 0;
            if (err != OPUS_OK) continue;
            it = g_peers.emplace(who, std::move(p)).first;
        }
        int got = opus_decode(it->second.dec, buf + 6 + nl, n - 6 - nl, pcm, FRAME * 6, 0);
        if (got > 0) {
            auto& q = it->second.pcm;
            q.insert(q.end(), pcm, pcm + got);
            while (q.size() > MAX_BUFFER) q.pop_front();
            it->second.last = GetTickCount();
        }
    }
    return 0;
}

static DWORD WINAPI KeepAliveThread(LPVOID) {
    while (g_run) { SendPacket(2, nullptr, 0); Sleep(2000); }
    return 0;
}

static bool InitAll(const std::string& ip, int port) {
    WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return false;
    g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(&g_srv, 0, sizeof(g_srv));
    g_srv.sin_family = AF_INET;
    g_srv.sin_port = htons((u_short)port);
    g_srv.sin_addr.s_addr = inet_addr(ip.c_str());
    if (g_srv.sin_addr.s_addr == INADDR_NONE) {
        hostent* h = gethostbyname(ip.c_str());
        if (!h) return false;
        memcpy(&g_srv.sin_addr, h->h_addr_list[0], 4);
    }
    int err;
    g_enc = opus_encoder_create(RATE, 1, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) return false;
    opus_encoder_ctl(g_enc, OPUS_SET_BITRATE(20000));
    opus_encoder_ctl(g_enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    ma_device_config c = ma_device_config_init(ma_device_type_capture);
    c.capture.format = ma_format_s16; c.capture.channels = 1; c.sampleRate = RATE;
    c.dataCallback = (ma_device_data_proc)CaptureCb;
    if (ma_device_init(NULL, &c, &g_cap) != MA_SUCCESS) return false;
    ma_device_config p = ma_device_config_init(ma_device_type_playback);
    p.playback.format = ma_format_s16; p.playback.channels = 1; p.sampleRate = RATE;
    p.dataCallback = (ma_device_data_proc)PlayCb;
    if (ma_device_init(NULL, &p, &g_play) != MA_SUCCESS) return false;
    ma_device_start(&g_cap);
    ma_device_start(&g_play);

    g_run = true;
    CreateThread(NULL, 0, RecvThread, NULL, 0, NULL);
    CreateThread(NULL, 0, KeepAliveThread, NULL, 0, NULL);
    return true;
}

// ---------- Funciones expuestas a Lua ----------
static int __cdecl Lua_Init(void* L) {
    if (g_inited) return 0;
    const char* n = L_tostring(L, 1);
    const char* ip = L_tostring(L, 2);
    int port = (int)L_tonumber(L, 3);
    if (!n || !ip) return 0;
    g_name = n;
    g_inited = InitAll(ip, port);
    return 0;
}
static int __cdecl Lua_SetRoom(void* L) {
    const char* r = L_tostring(L, 1);
    g_room = (r && *r) ? Fnv1a(r) : 0;
    return 0;
}
static int __cdecl Lua_PTT(void* L) {
    g_ptt = L_tonumber(L, 1) != 0;
    return 0;
}
static int __cdecl Lua_Speaking(void* L) {
    std::string out;
    DWORD now = GetTickCount();
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& kv : g_peers)
            if (now - kv.second.last < 400) { if (!out.empty()) out += ","; out += kv.first; }
    }
    L_pushstring(L, out.c_str());
    return 1;
}

static DWORD WINAPI BootThread(LPVOID) {
    // Espera a que el cliente cree su ventana y su estado Lua
    while (!FindWindowA("GxWindowClass", NULL)) Sleep(250);
    Sleep(3000);
    FrameScript_RegisterFunction("VC_Init", Lua_Init);
    FrameScript_RegisterFunction("VC_SetRoom", Lua_SetRoom);
    FrameScript_RegisterFunction("VC_PTT", Lua_PTT);
    FrameScript_RegisterFunction("VC_Speaking", Lua_Speaking);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, BootThread, NULL, 0, NULL);
    }
    return TRUE;
}
