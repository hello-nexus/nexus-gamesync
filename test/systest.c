/* Proves system-wide (Model-A) resolution + forwarding without a game: load
 * RzChromaSDK.dll by BARE NAME (so the OS resolves it from System32 for x64 or
 * SysWOW64 for x86 - exactly as a game's binding does), report where it loaded
 * from, then push a GREEN frame. Build x86 to test the SysWOW64/32-bit path
 * (Dead Cells), x64 for the System32 path (Cyberpunk). */
#include <windows.h>
#include <stdio.h>

typedef LONG RZRESULT;
typedef RZRESULT (*pInit)(void);
typedef RZRESULT (*pCKE)(int, void *, GUID *);
typedef RZRESULT (*pSet)(GUID);

int main(void)
{
    HMODULE m = LoadLibraryA("RzChromaSDK.dll"); /* bare name -> system search */
    if (!m) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    char path[MAX_PATH];
    if (GetModuleFileNameA(m, path, MAX_PATH)) printf("RzChromaSDK.dll loaded from: %s\n", path);
    pInit Init = (pInit)GetProcAddress(m, "Init");
    pCKE CKE = (pCKE)GetProcAddress(m, "CreateKeyboardEffect");
    pSet Set = (pSet)GetProcAddress(m, "SetEffect");
    if (Init) Init();
    unsigned int grid[6 * 22];
    int i;
    for (i = 0; i < 6 * 22; i++) grid[i] = 0x0000FF00u; /* COLORREF 0x00BBGGRR -> green */
    GUID eff; ZeroMemory(&eff, sizeof(eff));
    if (CKE) CKE(2, grid, &eff);
    if (Set) Set(eff);
    printf("posted green; waiting 2.5s for worker...\n");
    Sleep(2500);
    printf("done\n");
    return 0;
}
