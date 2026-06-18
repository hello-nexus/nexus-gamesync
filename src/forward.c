/* Shared forwarding worker for the Game Sync capture shims. See forward.h. */
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "forward.h"
#pragma comment(lib, "winhttp")

#define NEXUS_HOST L"127.0.0.1"
#define NEXUS_PORT 9400
#define POST_MIN_INTERVAL_MS 25 /* coalescing floor: cap forwarding near the service render rate */
#define DEVICE_CAP 32
#define EFFECT_CAP 32

static CRITICAL_SECTION g_cs;

/* Active frame published by a shim, drained by the worker. */
static char g_curDevice[DEVICE_CAP];
static char g_curEffect[EFFECT_CAP];
static char g_curApp[96]; /* g_appTitle snapshot taken under g_cs at publish */
static int g_curRows, g_curCols;
static unsigned char g_curColors[FWD_MAX_BYTES];
static volatile LONG g_dirty;
static HANDLE g_frameEvent;
static volatile LONG g_workerStarted;
static volatile LONG g_stop;

static char g_token[128];
static volatile LONG g_haveToken;

/* Source app title, sanitized to JSON-safe ASCII; empty when none was given.
 * Written under g_cs and snapshotted into g_curApp under g_cs at publish, so the
 * worker never reads it concurrently with a late LogiLedInitWithName write. */
static char g_appTitle[96];

/* ---------------- debug logging (build with /DSHIM_LOG; off in production) ---------------- */
#ifdef SHIM_LOG
void fwd_slog(const char *fmt, ...)
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
#endif

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

void forward_capture_app_w(const wchar_t *w)
{
    if (!w) return;
    EnterCriticalSection(&g_cs);
    __try {
        int o = 0, i;
        /* JSON-safe ASCII: drop non-printable, non-ASCII, and " and \ so it embeds unescaped. */
        for (i = 0; w[i] && o < (int)sizeof(g_appTitle) - 1; i++) {
            wchar_t c = w[i];
            if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') g_appTitle[o++] = (char)c;
        }
        g_appTitle[o] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_appTitle[0] = 0; }
    LeaveCriticalSection(&g_cs);
}

void forward_capture_app_a(const char *a)
{
    if (!a) return;
    EnterCriticalSection(&g_cs);
    __try {
        int o = 0, i;
        for (i = 0; a[i] && o < (int)sizeof(g_appTitle) - 1; i++) {
            unsigned char c = (unsigned char)a[i];
            if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') g_appTitle[o++] = (char)c;
        }
        g_appTitle[o] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_appTitle[0] = 0; }
    LeaveCriticalSection(&g_cs);
}

static int build_frame_json(const char *device, const char *effect, const char *app,
                            int rows, int cols, const unsigned char *colors, char *out, int cap)
{
    int n = rows * cols, i, len;
    len = _snprintf_s(out, cap, _TRUNCATE,
                      "{\"device\":\"%s\",\"effect\":\"%s\",\"app\":\"%s\",\"rows\":%d,\"cols\":%d,\"colors\":[",
                      device, effect, app, rows, cols);
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
    HINTERNET hSession = WinHttpOpen(L"nexus-gamesync/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = hSession ? WinHttpConnect(hSession, NEXUS_HOST, NEXUS_PORT, 0) : NULL;
    /* worker-thread-only (single worker via the g_workerStarted CAS). */
    static char body[FWD_MAX_BYTES * 12 + 256];
    static char resp[512];
    ULONGLONG lastPost = 0;
    int rows, cols;
    char device[DEVICE_CAP], effect[EFFECT_CAP], app[sizeof(g_curApp)];
    unsigned char snap[FWD_MAX_BYTES];

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
        memcpy(device, g_curDevice, sizeof(device));
        memcpy(effect, g_curEffect, sizeof(effect));
        memcpy(app, g_curApp, sizeof(app));
        memcpy(snap, g_curColors, (size_t)(rows * cols * 3));
        LeaveCriticalSection(&g_cs);

        if (rows > 0 && cols > 0) {
            int blen = build_frame_json(device, effect, app, rows, cols, snap, body, (int)sizeof(body));
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

void forward_ensure_worker(void)
{
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
}

void forward_publish(const char *device, const char *effect, int rows, int cols, const unsigned char *rgb)
{
    int bytes = rows * cols * 3;
    if (!device || !effect || !rgb || bytes <= 0 || bytes > FWD_MAX_BYTES) return;
    forward_ensure_worker();
    EnterCriticalSection(&g_cs);
    __try {
        strncpy_s(g_curDevice, sizeof(g_curDevice), device, _TRUNCATE);
        strncpy_s(g_curEffect, sizeof(g_curEffect), effect, _TRUNCATE);
        memcpy(g_curApp, g_appTitle, sizeof(g_curApp)); /* stable copy for the worker */
        g_curRows = rows; g_curCols = cols;
        memcpy(g_curColors, rgb, (size_t)bytes);
        InterlockedExchange(&g_dirty, 1);
        SetEvent(g_frameEvent);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    LeaveCriticalSection(&g_cs);
}

void forward_init(void)
{
    InitializeCriticalSection(&g_cs);
    g_frameEvent = CreateEventW(NULL, FALSE, FALSE, NULL); /* auto-reset */
}

void forward_shutdown(void)
{
    g_stop = 1;
    if (g_frameEvent) SetEvent(g_frameEvent);
}
