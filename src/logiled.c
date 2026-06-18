/* nexus-logiled - Logitech LED Illumination (LogitechLedEnginesWrapper.dll)
 * capture shim. Also shipped as LogitechLed.dll (older name some games load).
 *
 * Drop-in for Logitech's Gaming LED SDK; a game LoadLibrary()s the wrapper +
 * GetProcAddress binds these exports. Logitech has NO commit call: each set-call
 * IS the frame, so we forward immediately.
 *
 * ABI: __cdecl, extern "C" (plain C here), undecorated names via the .def.
 * Returns bool true for all except LogiLedShutdown (void). Colours are int
 * percentages 0-100. Reimplemented from Logitech's LogitechLEDLib.h (see
 * THIRD-PARTY.md); links no Logitech code.
 *
 * Capture model:
 *   LogiLedSetLighting(r%,g%,b%) -> CHROMA_STATIC 1x1 (whole-device solid).
 *   LogiLedSetLightingFromBitmap(bmp[504]) -> 21x6 BGRA -> 6 rows x 21 cols RGB
 *     grid (row-major) -> CHROMA_CUSTOM 6x21.
 *   Per-key set-calls no-op (return true); forwarding the solid + bitmap is enough. */
#include <windows.h>
#include <stdbool.h>
#include "forward.h"

/* LogitechLEDLib.h bitmap geometry. Byte order is BGRA (0=Blue,1=Green,2=Red,3=Alpha). */
#define LOGI_LED_BITMAP_WIDTH 21
#define LOGI_LED_BITMAP_HEIGHT 6
#define LOGI_LED_BITMAP_BYTES_PER_KEY 4
#define LOGI_LED_BITMAP_SIZE (LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT * LOGI_LED_BITMAP_BYTES_PER_KEY)

static unsigned char pct2byte(int pct)
{
    if (pct <= 0) return 0;
    if (pct >= 100) return 255;
    return (unsigned char)((pct * 255) / 100);
}

bool LogiLedInit(void) { SLOG("LogiLedInit()"); forward_ensure_worker(); return true; }

bool LogiLedInitWithName(const char *name)
{
    SLOG("LogiLedInitWithName()");
    if (name) forward_capture_app_a(name);
    forward_ensure_worker();
    return true;
}

bool LogiLedGetSdkVersion(int *major, int *minor, int *build)
{
    __try {
        if (major) *major = 9;
        if (minor) *minor = 0;
        if (build) *build = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return true;
}

bool LogiLedSetTargetDevice(int targetDevice) { (void)targetDevice; return true; }
bool LogiLedSaveCurrentLighting(void) { return true; }
bool LogiLedRestoreLighting(void) { return true; }
bool LogiLedStopEffects(void) { return true; }

/* Whole-device solid colour; forwards immediately (no commit call in this SDK). */
bool LogiLedSetLighting(int r, int g, int b)
{
    unsigned char rgb[3];
    rgb[0] = pct2byte(r); rgb[1] = pct2byte(g); rgb[2] = pct2byte(b);
    SLOG("LogiLedSetLighting(%d,%d,%d)", r, g, b);
    forward_publish("keyboard", "CHROMA_STATIC", 1, 1, rgb);
    return true;
}

bool LogiLedFlashLighting(int r, int g, int b, int msDuration, int msInterval)
{ (void)msDuration; (void)msInterval; return LogiLedSetLighting(r, g, b); }

bool LogiLedPulseLighting(int r, int g, int b, int msDuration, int msInterval)
{ (void)msDuration; (void)msInterval; return LogiLedSetLighting(r, g, b); }

/* 21x6 BGRA bitmap -> 6 rows x 21 cols RGB grid (row-major). The SDK bitmap is
 * row-major: byte index = (row*WIDTH + col)*4. */
bool LogiLedSetLightingFromBitmap(unsigned char *bitmap)
{
    unsigned char rgb[LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT * 3];
    int got = 0;
    SLOG("LogiLedSetLightingFromBitmap()");
    if (!bitmap) return true;
    __try {
        int i;
        for (i = 0; i < LOGI_LED_BITMAP_SIZE; i += LOGI_LED_BITMAP_BYTES_PER_KEY) {
            rgb[got] = bitmap[i + 2];     /* R (BGRA index 2) */
            rgb[got + 1] = bitmap[i + 1]; /* G */
            rgb[got + 2] = bitmap[i];     /* B (BGRA index 0) */
            got += 3;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { got = 0; }
    if (got == LOGI_LED_BITMAP_WIDTH * LOGI_LED_BITMAP_HEIGHT * 3)
        forward_publish("keyboard", "CHROMA_CUSTOM", LOGI_LED_BITMAP_HEIGHT, LOGI_LED_BITMAP_WIDTH, rgb);
    return true;
}

/* Per-key set-calls: no-op (the solid + bitmap paths carry the lighting). */
bool LogiLedSetLightingForKeyWithScanCode(int keyCode, int r, int g, int b) { (void)keyCode; (void)r; (void)g; (void)b; return true; }
bool LogiLedSetLightingForKeyWithHidCode(int keyCode, int r, int g, int b) { (void)keyCode; (void)r; (void)g; (void)b; return true; }
bool LogiLedSetLightingForKeyWithQuartzCode(int keyCode, int r, int g, int b) { (void)keyCode; (void)r; (void)g; (void)b; return true; }
bool LogiLedSetLightingForKeyWithKeyName(int keyName, int r, int g, int b) { (void)keyName; (void)r; (void)g; (void)b; return true; }
bool LogiLedSaveLightingForKey(int keyName) { (void)keyName; return true; }
bool LogiLedRestoreLightingForKey(int keyName) { (void)keyName; return true; }
bool LogiLedExcludeKeysFromBitmap(int *keyList, int listCount) { (void)keyList; (void)listCount; return true; }
bool LogiLedFlashSingleKey(int keyName, int r, int g, int b, int msDuration, int msInterval) { (void)keyName; (void)r; (void)g; (void)b; (void)msDuration; (void)msInterval; return true; }
bool LogiLedPulseSingleKey(int keyName, int startR, int startG, int startB, int finishR, int finishG, int finishB, int msDuration, int isInfinite) { (void)keyName; (void)startR; (void)startG; (void)startB; (void)finishR; (void)finishG; (void)finishB; (void)msDuration; (void)isInfinite; return true; }
bool LogiLedStopEffectsOnKey(int keyName) { (void)keyName; return true; }
bool LogiLedSetLightingForTargetZone(int deviceType, int zone, int r, int g, int b) { (void)deviceType; (void)zone; (void)r; (void)g; (void)b; return true; }

/* Config-option setters: editor-side, no-op here. Generic signatures; cdecl
 * leaves stack cleanup to the caller, so the exact tail args are irrelevant. */
bool LogiLedSetConfigOptionNumber(const wchar_t *configPath, double *defaultValue) { (void)configPath; (void)defaultValue; return true; }
bool LogiLedSetConfigOptionBool(const wchar_t *configPath, int *defaultValue) { (void)configPath; (void)defaultValue; return true; }
bool LogiLedSetConfigOptionColor(const wchar_t *configPath, int *r, int *g, int *b) { (void)configPath; (void)r; (void)g; (void)b; return true; }
bool LogiLedSetConfigOptionRange(const wchar_t *configPath, int *defaultValue, int min, int max) { (void)configPath; (void)defaultValue; (void)min; (void)max; return true; }
bool LogiLedSetConfigOptionSelect(const wchar_t *configPath, wchar_t *defaultValue, int *valueSize, const wchar_t *values, int bufferSize) { (void)configPath; (void)defaultValue; (void)valueSize; (void)values; (void)bufferSize; return true; }
bool LogiLedSetConfigOptionKeyInput(const wchar_t *configPath, wchar_t *defaultValue, int bufferSize) { (void)configPath; (void)defaultValue; (void)bufferSize; return true; }
bool LogiLedSetConfigOptionLabel(const wchar_t *configPath, wchar_t *label) { (void)configPath; (void)label; return true; }

void LogiLedShutdown(void) { SLOG("LogiLedShutdown()"); }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        forward_init();
        SLOG("==== logiled loaded pid=%lu ====", GetCurrentProcessId());
    } else if (reason == DLL_PROCESS_DETACH) {
        forward_shutdown();
    }
    return TRUE;
}
