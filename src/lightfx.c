/* nexus-lightfx - Alienware/Dell LightFX (LightFX.dll) capture shim.
 *
 * Drop-in for Dell's LightFX 2.0 SDK; a game LoadLibrary("LightFX.dll") +
 * GetProcAddress binds these exports. We stage colours and forward a single
 * representative colour to the Nexus service on the COMMIT call (LFX_Update).
 *
 * ABI: __stdcall, extern "C", undecorated names via the .def. LFX_RESULT is
 * unsigned int; LFX_SUCCESS == 0. Reimplemented from Dell's LFX2.h/LFXDecl.h
 * (see THIRD-PARTY.md); links no Dell code.
 *
 * Capture model: LightFX accumulates staged state since the last LFX_Reset and
 * commits it to hardware on LFX_Update. So per-light LFX_SetLightColor and
 * zone-mask LFX_Light(mask,packedRGB) only STAGE; LFX_Update forwards the
 * representative colour (prefer the most recent LFX_Light(LFX_ALL); else the
 * average of staged per-light colours) as CHROMA_STATIC 1x1. */
#include <windows.h>
#include "forward.h"

typedef unsigned int LFX_RESULT;
#define LFX_SUCCESS 0u

#define LFX_ALL 0x07FFFFFFu /* all-zones location mask (LFXDecl.h) */
#define MAX_LIGHTS 64

/* LFXDecl.h structs: field order is red,green,blue,brightness then x,y,z. */
typedef struct { unsigned char red, green, blue, brightness; } LFX_COLOR;
typedef LFX_COLOR *PLFX_COLOR;
typedef struct { unsigned char x, y, z; } LFX_POSITION;
typedef LFX_POSITION *PLFX_POSITION;

static CRITICAL_SECTION g_cs; /* guards the staged state below */

/* Most recent LFX_Light(LFX_ALL) colour (preferred representative). */
static int g_haveAll;
static unsigned char g_allR, g_allG, g_allB, g_allBri;

/* Staged per-light colours from LFX_SetLightColor (fallback representative). */
static int g_lightSet[MAX_LIGHTS];
static unsigned char g_lightR[MAX_LIGHTS], g_lightG[MAX_LIGHTS], g_lightB[MAX_LIGHTS], g_lightBri[MAX_LIGHTS];

static void reset_state(void)
{
    int i;
    g_haveAll = 0; g_allR = g_allG = g_allB = 0; g_allBri = 0;
    for (i = 0; i < MAX_LIGHTS; i++) g_lightSet[i] = 0;
}

/* brightness 0 means unset; otherwise scales the colour. */
static unsigned char scale_bri(unsigned int c, unsigned char bri)
{
    if (bri == 0) return (unsigned char)c;
    return (unsigned char)((c * bri) / 255u);
}

/* Pick the representative colour and forward it as a 1x1 static frame. */
static void commit(void)
{
    unsigned char r = 0, g = 0, b = 0, bri = 0;
    int have = 0, i, n = 0;
    unsigned int sr = 0, sg = 0, sb = 0, sbri = 0;

    EnterCriticalSection(&g_cs);
    if (g_haveAll) {
        r = g_allR; g = g_allG; b = g_allB; bri = g_allBri; have = 1;
    } else {
        for (i = 0; i < MAX_LIGHTS; i++) {
            if (!g_lightSet[i]) continue;
            sr += g_lightR[i]; sg += g_lightG[i]; sb += g_lightB[i]; sbri += g_lightBri[i]; n++;
        }
        if (n > 0) {
            r = (unsigned char)(sr / n); g = (unsigned char)(sg / n);
            b = (unsigned char)(sb / n); bri = (unsigned char)(sbri / n); have = 1;
        }
    }
    LeaveCriticalSection(&g_cs);

    if (have) {
        unsigned char rgb[3];
        rgb[0] = scale_bri(r, bri);
        rgb[1] = scale_bri(g, bri);
        rgb[2] = scale_bri(b, bri);
        forward_publish("keyboard", "CHROMA_STATIC", 1, 1, rgb);
    }
}

LFX_RESULT __stdcall LFX_Initialize(void) { SLOG("LFX_Initialize()"); forward_ensure_worker(); return LFX_SUCCESS; }
LFX_RESULT __stdcall LFX_Release(void) { SLOG("LFX_Release()"); return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_Reset(void)
{
    SLOG("LFX_Reset()");
    EnterCriticalSection(&g_cs);
    reset_state();
    LeaveCriticalSection(&g_cs);
    return LFX_SUCCESS;
}

LFX_RESULT __stdcall LFX_Update(void) { SLOG("LFX_Update()"); commit(); return LFX_SUCCESS; }
LFX_RESULT __stdcall LFX_UpdateDefault(void) { SLOG("LFX_UpdateDefault()"); commit(); return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetNumDevices(unsigned int *const n)
{ SLOG("LFX_GetNumDevices()"); if (n) { __try { *n = 1; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetDeviceDescription(const unsigned int dev, char *const desc, const unsigned int cap, unsigned char *const type)
{
    (void)dev;
    if (desc && cap > 0) { __try { lstrcpynA(desc, "Nexus", (int)cap); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    if (type) { __try { *type = 0x06; } __except (EXCEPTION_EXECUTE_HANDLER) {} } /* LFX_DEVTYPE_KEYBOARD */
    return LFX_SUCCESS;
}

LFX_RESULT __stdcall LFX_GetNumLights(const unsigned int dev, unsigned int *const n)
{ (void)dev; if (n) { __try { *n = 1; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetLightDescription(const unsigned int dev, const unsigned int light, char *const desc, const unsigned int cap)
{ (void)dev; (void)light; if (desc && cap > 0) { __try { lstrcpynA(desc, "Nexus", (int)cap); } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetLightLocation(const unsigned int dev, const unsigned int light, PLFX_POSITION const pos)
{ (void)dev; (void)light; if (pos) { __try { pos->x = pos->y = pos->z = 0; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetLightColor(const unsigned int dev, const unsigned int light, PLFX_COLOR const col)
{ (void)dev; (void)light; if (col) { __try { col->red = col->green = col->blue = 0; col->brightness = 0; } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

/* Stage a per-light colour (commit deferred to LFX_Update). */
LFX_RESULT __stdcall LFX_SetLightColor(const unsigned int dev, const unsigned int light, const PLFX_COLOR col)
{
    (void)dev;
    if (!col || light >= MAX_LIGHTS) return LFX_SUCCESS;
    __try {
        unsigned char r = col->red, g = col->green, b = col->blue, bri = col->brightness;
        EnterCriticalSection(&g_cs);
        g_lightR[light] = r; g_lightG[light] = g; g_lightB[light] = b; g_lightBri[light] = bri;
        g_lightSet[light] = 1;
        LeaveCriticalSection(&g_cs);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    SLOG("LFX_SetLightColor(light=%u)", light);
    return LFX_SUCCESS;
}

/* Stage a zone-mask colour. Packed: bits0-7 Blue,8-15 Green,16-23 Red,24-31 Bri.
 * An LFX_ALL mask becomes the preferred representative colour at commit. */
LFX_RESULT __stdcall LFX_Light(const unsigned int locationMask, const unsigned int rgbColor)
{
    SLOG("LFX_Light(mask=%08X, rgb=%08X)", locationMask, rgbColor);
    if ((locationMask & LFX_ALL) == LFX_ALL) {
        EnterCriticalSection(&g_cs);
        g_allR = (unsigned char)((rgbColor >> 16) & 0xFF);
        g_allG = (unsigned char)((rgbColor >> 8) & 0xFF);
        g_allB = (unsigned char)(rgbColor & 0xFF);
        g_allBri = (unsigned char)((rgbColor >> 24) & 0xFF);
        g_haveAll = 1;
        LeaveCriticalSection(&g_cs);
    }
    return LFX_SUCCESS;
}

LFX_RESULT __stdcall LFX_SetLightActionColor(const unsigned int dev, const unsigned int light, const unsigned int action, const PLFX_COLOR col)
{ return LFX_SetLightColor(dev, light, col); }

LFX_RESULT __stdcall LFX_SetLightActionColorEx(const unsigned int dev, const unsigned int light, const unsigned int action, const PLFX_COLOR primary, const PLFX_COLOR secondary)
{ (void)secondary; return LFX_SetLightColor(dev, light, primary); }

LFX_RESULT __stdcall LFX_ActionColor(const unsigned int locationMask, const unsigned int action, const unsigned int rgbColor)
{ (void)action; return LFX_Light(locationMask, rgbColor); }

LFX_RESULT __stdcall LFX_ActionColorEx(const unsigned int locationMask, const unsigned int action, const unsigned int primaryColor, const unsigned int secondaryColor)
{ (void)action; (void)secondaryColor; return LFX_Light(locationMask, primaryColor); }

LFX_RESULT __stdcall LFX_SetTiming(const int timing) { (void)timing; return LFX_SUCCESS; }

LFX_RESULT __stdcall LFX_GetVersion(char *const ver, const unsigned int cap)
{ if (ver && cap > 0) { __try { lstrcpynA(ver, "2.2.0.0", (int)cap); } __except (EXCEPTION_EXECUTE_HANDLER) {} } return LFX_SUCCESS; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_cs);
        forward_init();
        SLOG("==== lightfx loaded pid=%lu ====", GetCurrentProcessId());
    } else if (reason == DLL_PROCESS_DETACH) {
        forward_shutdown();
    }
    return TRUE;
}
