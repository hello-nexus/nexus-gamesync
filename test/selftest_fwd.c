/* Dev harness: load the built shim, drive Init + CreateKeyboardEffect + SetEffect
 * with a solid-colour 6x22 grid, and let the shim's worker forward it to the
 * Nexus service. Proves the production forwarding path with no game involved.
 * Usage: selftest_fwd.exe [BBGGRR-hex]  (default FF0000 = red) */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef LONG RZRESULT;
typedef RZRESULT (*pInit)(void);
typedef RZRESULT (*pCKE)(int, void *, GUID *);
typedef RZRESULT (*pSet)(GUID);

int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA("C:\\Users\\nicol\\nexus-gamesync\\dist\\RzChromaSDK64.dll");
    if (!m) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    pInit Init = (pInit)GetProcAddress(m, "Init");
    pCKE CKE = (pCKE)GetProcAddress(m, "CreateKeyboardEffect");
    pSet Set = (pSet)GetProcAddress(m, "SetEffect");
    if (!Init || !CKE || !Set) { printf("missing exports\n"); return 2; }

    /* COLORREF is 0x00BBGGRR; arg is that hex, default pure red (R=FF). */
    unsigned int col = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 16) : 0x000000FFu;
    unsigned int grid[6 * 22];
    int i;
    for (i = 0; i < 6 * 22; i++) grid[i] = col;

    Init();
    GUID eff; ZeroMemory(&eff, sizeof(eff));
    CKE(2 /* CHROMA_CUSTOM */, grid, &eff);
    Set(eff);
    printf("posted effect Data1=%lu color=%06X; waiting 2s for worker to forward...\n", eff.Data1, col);
    Sleep(2000);
    printf("done\n");
    return 0;
}
