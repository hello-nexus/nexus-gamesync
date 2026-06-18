/* nexus-chroma — Razer Chroma capture shim.
 *
 * Built as RzChromaSDK64.dll (and RzChromatic64.dll); a Chroma game's
 * CChromaEditorLibrary loads it in place of Razer's SDK. We implement the SDK
 * surface, capture the keyboard effect grids, and forward the active grid to the
 * local Nexus service (POST /lighting/game-sync/frame) over loopback via the
 * shared worker in forward.c.
 *
 * Capture model: games create a few keyboard effects once (each baked with
 * colours) then animate by SetEffect(id) cycling between them - so grids are
 * stored by effect id at create time and the active grid is forwarded on
 * SetEffect. Some games (Dead Cells) re-create the effect each frame and never
 * SetEffect, so create also forwards. Forwarding happens on the shared worker
 * thread (never the game thread). Wire JSON: device "keyboard", effect
 * "CHROMA_CUSTOM", COLORREF 0x00BBGGRR. */
#include <windows.h>
#include <string.h>
#include "forward.h"

typedef LONG RZRESULT; /* 0 == RZRESULT_SUCCESS */

#define KB_CAP 8192
#define KB_MAX_BYTES (8 * 24 * 4) /* CHROMA_CUSTOM2 grid is the largest (8x24) */

static CRITICAL_SECTION g_cs; /* guards the grid table below */
static volatile LONG g_eff = 0;

/* Stored keyboard grids, keyed by effect id (id % KB_CAP, with id verify). */
static unsigned char g_kbValid[KB_CAP];
static long g_kbId[KB_CAP];
static int g_kbRows[KB_CAP], g_kbCols[KB_CAP];
static unsigned char g_kbColors[KB_CAP][KB_MAX_BYTES];

/* ---------------- capture ---------------- */

static void store_kbd(long id, int rows, int cols, const void *colors)
{
    int slot = (int)(((unsigned long)id) % KB_CAP);
    int rgbBytes = rows * cols * 3, srcBytes = rows * cols * 4, got = 0, i;
    if (rgbBytes <= 0 || rgbBytes > KB_MAX_BYTES) return;
    EnterCriticalSection(&g_cs);
    g_kbValid[slot] = 0;
    __try {
        const unsigned char *s = (const unsigned char *)colors;
        for (i = 0; i < srcBytes; i += 4) {
            g_kbColors[slot][got] = s[i];         /* R */
            g_kbColors[slot][got + 1] = s[i + 1]; /* G */
            g_kbColors[slot][got + 2] = s[i + 2]; /* B */
            got += 3;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { got = 0; }
    if (got == rgbBytes) { g_kbRows[slot] = rows; g_kbCols[slot] = cols; g_kbId[slot] = id; g_kbValid[slot] = 1; }
    LeaveCriticalSection(&g_cs);
}

static long next_id(GUID *pEffectId)
{
    long id = InterlockedIncrement(&g_eff);
    if (pEffectId) {
        __try { ZeroMemory(pEffectId, sizeof(GUID)); pEffectId->Data1 = (unsigned long)id; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return id;
}

/* Forward the grid stored for effect id (if valid). Copies under the table lock,
 * publishes outside it. */
static int publish_effect(long id)
{
    int slot = (int)(((unsigned long)id) % KB_CAP);
    int rows = 0, cols = 0;
    unsigned char rgb[KB_MAX_BYTES];
    EnterCriticalSection(&g_cs);
    if (g_kbValid[slot] && g_kbId[slot] == id) {
        rows = g_kbRows[slot]; cols = g_kbCols[slot];
        memcpy(rgb, g_kbColors[slot], (size_t)(rows * cols * 3));
    }
    LeaveCriticalSection(&g_cs);
    if (rows > 0 && cols > 0) {
        forward_publish("keyboard", "CHROMA_CUSTOM", rows, cols, rgb);
        return 1;
    }
    return 0;
}

RZRESULT Init(void) { SLOG("Init()"); forward_ensure_worker(); return 0; }
RZRESULT InitSDK(void *pAppInfo)
{
    SLOG("InitSDK()");
    if (pAppInfo) forward_capture_app_w((const wchar_t *)pAppInfo);
    forward_ensure_worker();
    return 0;
}
RZRESULT UnInit(void) { SLOG("UnInit()"); return 0; }

RZRESULT CreateEffect(GUID DeviceId, int Effect, void *pParam, GUID *pEffectId)
{ (void)pParam; SLOG("CreateEffect(dev=%08lX, effect=%d)", DeviceId.Data1, Effect); next_id(pEffectId); return 0; }

RZRESULT CreateKeyboardEffect(int Effect, void *pParam, GUID *pEffectId)
{
    long id = next_id(pEffectId);
    int rows = 0, cols = 0;
    SLOG("CreateKeyboardEffect(effect=%d) id=%ld", Effect, id);
    if (Effect == 2 || Effect == 7) { rows = 6; cols = 22; } /* CUSTOM / CUSTOM_KEY */
    else if (Effect == 8) { rows = 8; cols = 24; }           /* CUSTOM2 */
    if (pParam && rows) {
        store_kbd(id, rows, cols, pParam);
        /* Some games apply on create and never call SetEffect (Dead Cells
         * re-creates the keyboard effect each frame). Publish on create so those
         * forward; SetEffect still drives create-once/set-cycle games (Cyberpunk). */
        publish_effect(id);
    }
    return 0;
}

RZRESULT CreateMouseEffect(int Effect, void *pParam, GUID *pEffectId) { (void)pParam; SLOG("CreateMouseEffect(effect=%d)", Effect); next_id(pEffectId); return 0; }
RZRESULT CreateHeadsetEffect(int Effect, void *pParam, GUID *pEffectId) { (void)pParam; SLOG("CreateHeadsetEffect(effect=%d)", Effect); next_id(pEffectId); return 0; }
RZRESULT CreateMousepadEffect(int Effect, void *pParam, GUID *pEffectId) { (void)pParam; SLOG("CreateMousepadEffect(effect=%d)", Effect); next_id(pEffectId); return 0; }
RZRESULT CreateKeypadEffect(int Effect, void *pParam, GUID *pEffectId) { (void)pParam; SLOG("CreateKeypadEffect(effect=%d)", Effect); next_id(pEffectId); return 0; }
RZRESULT CreateChromaLinkEffect(int Effect, void *pParam, GUID *pEffectId) { (void)pParam; SLOG("CreateChromaLinkEffect(effect=%d)", Effect); next_id(pEffectId); return 0; }

RZRESULT SetEffect(GUID EffectId)
{
    long id = (long)EffectId.Data1;
    int fwd;
    forward_ensure_worker();
    fwd = publish_effect(id);
    SLOG("SetEffect(id=%ld) fwd=%d", id, fwd);
    return 0;
}

RZRESULT DeleteEffect(GUID EffectId) { (void)EffectId; return 0; }

RZRESULT QueryDevice(GUID DeviceId, void *pInfo)
{
    SLOG("QueryDevice(dev=%08lX)", DeviceId.Data1);
    if (pInfo) { __try { ((DWORD *)pInfo)[0] = 1; ((DWORD *)pInfo)[1] = 1; } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    return 0;
}

RZRESULT RegisterEventNotification(void *hWnd) { (void)hWnd; return 0; }
RZRESULT UnregisterEventNotification(void) { return 0; }
RZRESULT IsActive(int *pActive) { SLOG("IsActive()"); if (pActive) { __try { *pActive = 1; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return 0; }
RZRESULT IsConnected(void *pDeviceInfo) { SLOG("IsConnected()"); if (pDeviceInfo) { __try { ((DWORD *)pDeviceInfo)[0] = 1; ((DWORD *)pDeviceInfo)[1] = 1; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return 0; }
RZRESULT SetEventName(const wchar_t *Name) { (void)Name; return 0; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_cs);
        forward_init();
        SLOG("==== loaded pid=%lu ====", GetCurrentProcessId());
    } else if (reason == DLL_PROCESS_DETACH) {
        forward_shutdown();
    }
    return TRUE;
}
