/* Model-A validation without launching the game: load Cyberpunk's REAL
 * CChromaEditorLibrary64.dll, init it, and report whether/where it resolved the
 * SDK DLL. Then drive a red keyboard frame through the editor lib's Core API so
 * the chain editor-lib -> System32 shim -> service -> hardware is exercised end
 * to end. The harness has no RzChromaSDK64.dll in its own dir, so resolution
 * falls to System32 exactly as a game with no per-game drop would. */
#include <windows.h>
#include <stdio.h>

typedef LONG RZRESULT;
typedef RZRESULT (*pCoreInit)(void);
typedef RZRESULT (*pCKE)(int, void *, GUID *);
typedef RZRESULT (*pSet)(GUID);

static void report_sdk_module(void)
{
    char path[MAX_PATH];
    HMODULE rz = GetModuleHandleA("RzChromaSDK64.dll");
    if (rz && GetModuleFileNameA(rz, path, MAX_PATH)) { printf("RzChromaSDK64.dll loaded from: %s\n", path); return; }
    rz = GetModuleHandleA("RzChromatic64.dll");
    if (rz && GetModuleFileNameA(rz, path, MAX_PATH)) { printf("RzChromatic64.dll loaded from: %s\n", path); return; }
    printf("SDK DLL NOT loaded by the editor lib\n");
}

int main(void)
{
    const char *binx64 = "C:\\program files (x86)\\steam\\steamapps\\common\\Cyberpunk 2077\\bin\\x64";
    const char *ed = "C:\\program files (x86)\\steam\\steamapps\\common\\Cyberpunk 2077\\bin\\x64\\CChromaEditorLibrary64.dll";
    SetDllDirectoryA(binx64); /* let the editor lib's own deps resolve from bin\x64 */
    HMODULE h = LoadLibraryA(ed);
    if (!h) { printf("editor lib load failed: %lu\n", GetLastError()); return 1; }
    pCoreInit CoreInit = (pCoreInit)GetProcAddress(h, "PluginCoreInit");
    pCKE CKE = (pCKE)GetProcAddress(h, "PluginCoreCreateKeyboardEffect");
    pSet Set = (pSet)GetProcAddress(h, "PluginCoreSetEffect");
    printf("exports: CoreInit=%p CreateKbd=%p SetEffect=%p\n", (void *)CoreInit, (void *)CKE, (void *)Set);
    if (!CoreInit) return 2;

    RZRESULT r = CoreInit();
    printf("PluginCoreInit -> %ld\n", r);
    report_sdk_module();

    unsigned int grid[6 * 22];
    int i;
    for (i = 0; i < 6 * 22; i++) grid[i] = 0x000000FFu; /* red */
    GUID eff; ZeroMemory(&eff, sizeof(eff));
    if (CKE) { RZRESULT r2 = CKE(2 /* CHROMA_CUSTOM */, grid, &eff); printf("CreateKeyboardEffect -> %ld eff=%lu\n", r2, eff.Data1); }
    if (Set) { RZRESULT r3 = Set(eff); printf("SetEffect -> %ld\n", r3); }
    Sleep(2500); /* let the shim worker forward */
    printf("done\n");
    return 0;
}
