/* Dev harness for the LightFX + Logitech shims: LoadLibrary each, GetProcAddress
 * the key exports, drive them with a known GREEN colour, and confirm no crash.
 * Each shim's worker forwards a frame to the Nexus service (start Game Sync
 * first; verify via /lighting/game-sync/state LastFrameAt). Build x64 and x86 to
 * prove both arches load. Loads by bare name, so the DLLs must sit next to this
 * exe (or in the system search path).
 * Usage: loadtest.exe  (loads LightFX.dll + LogitechLedEnginesWrapper.dll) */
#include <windows.h>
#include <stdio.h>

/* LightFX: __stdcall, LFX_COLOR{red,green,blue,brightness}, LFX_SUCCESS==0. */
typedef unsigned int LFX_RESULT;
typedef struct { unsigned char red, green, blue, brightness; } LFX_COLOR;
typedef LFX_RESULT(__stdcall *pLFX_Init)(void);
typedef LFX_RESULT(__stdcall *pLFX_SetLightColor)(unsigned int, unsigned int, const LFX_COLOR *);
typedef LFX_RESULT(__stdcall *pLFX_Light)(unsigned int, unsigned int);
typedef LFX_RESULT(__stdcall *pLFX_Update)(void);

/* Logitech: __cdecl, bool returns, int percentages 0-100. */
typedef int(__cdecl *pLogiInit)(void);
typedef int(__cdecl *pLogiSetLighting)(int, int, int);
typedef int(__cdecl *pLogiSetBitmap)(unsigned char *);

#define LFX_ALL 0x07FFFFFFu
#define LOGI_BITMAP_SIZE (21 * 6 * 4)

static int test_lightfx(void)
{
    HMODULE m = LoadLibraryA("LightFX.dll");
    if (!m) { printf("LightFX: LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    char path[MAX_PATH];
    if (GetModuleFileNameA(m, path, MAX_PATH)) printf("LightFX loaded from: %s\n", path);

    pLFX_Init Init = (pLFX_Init)GetProcAddress(m, "LFX_Initialize");
    pLFX_SetLightColor SetCol = (pLFX_SetLightColor)GetProcAddress(m, "LFX_SetLightColor");
    pLFX_Light Light = (pLFX_Light)GetProcAddress(m, "LFX_Light");
    pLFX_Update Update = (pLFX_Update)GetProcAddress(m, "LFX_Update");
    printf("LightFX exports: Init=%p SetLightColor=%p Light=%p Update=%p\n",
           (void *)Init, (void *)SetCol, (void *)Light, (void *)Update);
    if (!Init || !SetCol || !Light || !Update) { printf("LightFX: missing exports\n"); return 2; }

    Init();
    LFX_COLOR green = { 0, 255, 0, 255 }; /* red,green,blue,brightness */
    SetCol(0, 0, &green);
    Light(LFX_ALL, 0x0000FF00u); /* packed 0x00RRGGBB -> green */
    Update();                    /* COMMIT -> forwards */
    printf("LightFX: posted green, no crash\n");
    return 0;
}

static int test_logiled(void)
{
    HMODULE m = LoadLibraryA("LogitechLedEnginesWrapper.dll");
    if (!m) { printf("Logitech: LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    char path[MAX_PATH];
    if (GetModuleFileNameA(m, path, MAX_PATH)) printf("Logitech loaded from: %s\n", path);

    pLogiInit Init = (pLogiInit)GetProcAddress(m, "LogiLedInit");
    pLogiSetLighting SetLighting = (pLogiSetLighting)GetProcAddress(m, "LogiLedSetLighting");
    pLogiSetBitmap SetBitmap = (pLogiSetBitmap)GetProcAddress(m, "LogiLedSetLightingFromBitmap");
    printf("Logitech exports: Init=%p SetLighting=%p SetBitmap=%p\n",
           (void *)Init, (void *)SetLighting, (void *)SetBitmap);
    if (!Init || !SetLighting || !SetBitmap) { printf("Logitech: missing exports\n"); return 2; }

    Init();
    SetLighting(0, 100, 0); /* green at 100% */
    unsigned char bmp[LOGI_BITMAP_SIZE];
    int i;
    for (i = 0; i < LOGI_BITMAP_SIZE; i += 4) { bmp[i] = 0; bmp[i + 1] = 255; bmp[i + 2] = 0; bmp[i + 3] = 255; } /* BGRA green */
    SetBitmap(bmp);
    printf("Logitech: posted green, no crash\n");
    return 0;
}

int main(void)
{
    int rl = test_lightfx();
    int rg = test_logiled();
    printf("waiting 2.5s for workers to forward...\n");
    Sleep(2500);
    printf("done lightfx=%d logitech=%d\n", rl, rg);
    return (rl || rg) ? 1 : 0;
}
