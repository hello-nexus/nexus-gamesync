/* nexus-chroma — Razer Chroma capture shim.
 *
 * Built as RzChromaSDK64.dll (and RzChromatic64.dll); a Chroma game's
 * CChromaEditorLibrary loads it in place of Razer's SDK. We implement the SDK
 * surface, capture the keyboard effect grids, and forward the active grid to the
 * local Nexus service (POST /lighting/game-sync/frame) over loopback.
 *
 * Capture model: games create a few keyboard effects once (each baked with
 * colours) then animate by SetEffect(id) cycling between them — so grids are
 * stored by effect id at create time and the active grid is forwarded on
 * SetEffect. Forwarding happens on a worker thread (never the game thread):
 * SetEffect publishes the active grid and signals an event; the worker coalesces
 * and POSTs. COLORREF wire format is 0x00BBGGRR. */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#pragma comment(lib, "winhttp")

typedef LONG RZRESULT; /* 0 == RZRESULT_SUCCESS */

#define NEXUS_HOST L"127.0.0.1"
#define NEXUS_PORT 9400
#define KB_CAP 8192
#define KB_MAX_BYTES (8 * 24 * 4) /* CHROMA_CUSTOM2 grid is the largest (8x24) */
#define POST_MIN_INTERVAL_MS 25   /* coalescing floor: cap forwarding near the service render rate */

static CRITICAL_SECTION g_cs;
static volatile LONG g_eff = 0;

/* Stored keyboard grids, keyed by effect id (id % KB_CAP, with id verify). */
static unsigned char g_kbValid[KB_CAP];
static long g_kbId[KB_CAP];
static int g_kbRows[KB_CAP], g_kbCols[KB_CAP];
static unsigned char g_kbColors[KB_CAP][KB_MAX_BYTES];

/* Active grid published by SetEffect, drained by the worker. */
static int g_curRows, g_curCols;
static unsigned char g_curColors[KB_MAX_BYTES];
static volatile LONG g_dirty;
static HANDLE g_frameEvent;
static volatile LONG g_workerStarted;
static volatile LONG g_stop;

static char g_token[128];
static volatile LONG g_haveToken;

/* ---------------- forwarding worker ---------------- */

/* Extract the token value from a /pair JSON body: {"token":"..."} (camel or Pascal). */
static int parse_token(const char *json, char *out, int cap)
{
    /* Match the "token" KEY (quoted) so fields like refresh_token/tokenExpiry don't fool us. */
    const char *p = strstr(json, "\"token\"");
    int i = 0;
    if (!p) return 0;
    p += 7;
    while (*p && *p != ':') p++; /* to the colon after the key */
    if (*p == ':') p++;
    while (*p == ' ' || *p == '"') p++; /* to the value */
    while (p[i] && p[i] != '"' && i < cap - 1) { out[i] = p[i]; i++; }
    out[i] = 0;
    return i > 0;
}

/* One WinHTTP request. Returns HTTP status (or 0 on transport failure). Optional
 * response body copied to respOut. */
static DWORD http_req(HINTERNET hConnect, const wchar_t *verb, const wchar_t *path,
                      const char *body, int bodyLen, char *respOut, int respCap)
{
    DWORD status = 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, verb, path, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) return 0;
    const wchar_t *hdr = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(hReq, body ? hdr : WINHTTP_NO_ADDITIONAL_HEADERS,
                                 body ? (DWORD)-1L : 0,
                                 (LPVOID)(body ? body : NULL), (DWORD)bodyLen, (DWORD)bodyLen, 0);
    if (ok) ok = WinHttpReceiveResponse(hReq, NULL);
    if (ok) {
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (respOut && respCap > 0) {
            DWORD total = 0, avail = 0;
            while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                DWORD want = (avail < (DWORD)(respCap - 1 - total)) ? avail : (DWORD)(respCap - 1 - total);
                if (want == 0) break;
                DWORD got = 0;
                if (!WinHttpReadData(hReq, respOut + total, want, &got) || got == 0) break;
                total += got;
            }
            respOut[total] = 0;
        }
    }
    WinHttpCloseHandle(hReq);
    return status;
}

static int build_frame_json(int rows, int cols, const unsigned char *colors, char *out, int cap)
{
    int n = rows * cols, i, len;
    len = _snprintf_s(out, cap, _TRUNCATE,
                      "{\"device\":\"keyboard\",\"effect\":\"CHROMA_CUSTOM\",\"rows\":%d,\"cols\":%d,\"colors\":[",
                      rows, cols);
    if (len < 0) return -1;
    for (i = 0; i < n; i++) {
        unsigned int cr = (unsigned int)colors[i * 3] | ((unsigned int)colors[i * 3 + 1] << 8) | ((unsigned int)colors[i * 3 + 2] << 16);
        int w = _snprintf_s(out + len, cap - len, _TRUNCATE, (i + 1 < n) ? "%u," : "%u", cr);
        if (w < 0) return -1;
        len += w;
    }
    int w = _snprintf_s(out + len, cap - len, _TRUNCATE, "]}");
    if (w < 0) return -1;
    return len + w;
}

static DWORD WINAPI worker(LPVOID arg)
{
    (void)arg;
    HINTERNET hSession = WinHttpOpen(L"nexus-chroma/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = hSession ? WinHttpConnect(hSession, NEXUS_HOST, NEXUS_PORT, 0) : NULL;
    static char body[KB_MAX_BYTES * 12 + 256];
    static char resp[512];
    ULONGLONG lastPost = 0;
    int rows, cols;
    unsigned char snap[KB_MAX_BYTES];

    while (!g_stop) {
        WaitForSingleObject(g_frameEvent, 1000);
        if (g_stop) break;
        if (!hConnect) continue;

        if (!g_haveToken) {
            DWORD st = http_req(hConnect, L"GET", L"/pair", NULL, 0, resp, sizeof(resp));
            if (st == 200 && parse_token(resp, g_token, sizeof(g_token))) g_haveToken = 1;
            else continue;
        }

        if (!InterlockedCompareExchange(&g_dirty, 0, 1)) continue; /* nothing new */
        ULONGLONG now = GetTickCount64();
        if (now - lastPost < POST_MIN_INTERVAL_MS) { Sleep((DWORD)(POST_MIN_INTERVAL_MS - (now - lastPost))); }

        EnterCriticalSection(&g_cs);
        rows = g_curRows; cols = g_curCols;
        memcpy(snap, g_curColors, (size_t)(rows * cols * 3));
        LeaveCriticalSection(&g_cs);

        if (rows > 0 && cols > 0) {
            int blen = build_frame_json(rows, cols, snap, body, (int)sizeof(body));
            if (blen > 0) {
                wchar_t path[256];
                _snwprintf_s(path, 256, _TRUNCATE, L"/lighting/game-sync/frame?token=%hs", g_token);
                DWORD st = http_req(hConnect, L"POST", path, body, blen, NULL, 0);
                if (st == 401) g_haveToken = 0; /* token rotated/invalid; refetch next loop */
                lastPost = GetTickCount64();
            }
        }
    }
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return 0;
}

static void ensure_worker(void)
{
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
}

/* ---------------- debug logging (build with /DSHIM_LOG; off in production) ---------------- */
#ifdef SHIM_LOG
static void slog_impl(const char *fmt, ...)
{
    EnterCriticalSection(&g_cs);
    FILE *f = fopen("C:\\Users\\nicol\\chroma-stub\\gamesync-shim.log", "a");
    if (f) {
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
        fputc('\n', f); fclose(f);
    }
    LeaveCriticalSection(&g_cs);
}
#define SLOG(...) slog_impl(__VA_ARGS__)
#else
#define SLOG(...) ((void)0)
#endif

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

RZRESULT Init(void) { SLOG("Init()"); ensure_worker(); return 0; }
RZRESULT InitSDK(void *pAppInfo)
{
    SLOG("InitSDK()");
    if (pAppInfo) { __try { SLOG("  app=\"%ls\"", (wchar_t *)pAppInfo); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    ensure_worker();
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
        int slot = (int)(((unsigned long)id) % KB_CAP);
        EnterCriticalSection(&g_cs);
        if (g_kbValid[slot] && g_kbId[slot] == id) {
            g_curRows = rows; g_curCols = cols;
            memcpy(g_curColors, g_kbColors[slot], (size_t)(rows * cols * 3));
            InterlockedExchange(&g_dirty, 1);
            SetEvent(g_frameEvent);
        }
        LeaveCriticalSection(&g_cs);
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
    int slot = (int)(((unsigned long)id) % KB_CAP);
    int fwd = 0;
    ensure_worker();
    EnterCriticalSection(&g_cs);
    if (g_kbValid[slot] && g_kbId[slot] == id) {
        g_curRows = g_kbRows[slot];
        g_curCols = g_kbCols[slot];
        memcpy(g_curColors, g_kbColors[slot], (size_t)(g_curRows * g_curCols * 3));
        InterlockedExchange(&g_dirty, 1);
        SetEvent(g_frameEvent);
        fwd = 1;
    }
    LeaveCriticalSection(&g_cs);
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
        g_frameEvent = CreateEventW(NULL, FALSE, FALSE, NULL); /* auto-reset */
        SLOG("==== loaded pid=%lu ====", GetCurrentProcessId());
    } else if (reason == DLL_PROCESS_DETACH) {
        g_stop = 1;
        if (g_frameEvent) SetEvent(g_frameEvent);
    }
    return TRUE;
}
