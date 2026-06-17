/* 32-bit module enumerator: lists a (32-bit) process's loaded modules via a
 * toolhelp snapshot. Needed because 64-bit PowerShell/tasklist can't see a
 * WOW64 process's 32-bit modules. Prints chroma/Razer matches + total count.
 * Usage: modlist32.exe <pid> */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: modlist32 <pid>\n"); return 1; }
    DWORD pid = (DWORD)atoi(argv[1]);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) { printf("snapshot failed: %lu\n", GetLastError()); return 2; }
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    int n = 0;
    if (Module32First(snap, &me)) {
        do {
            n++;
            if (strstr(me.szModule, "chroma") || strstr(me.szModule, "Chroma") ||
                strstr(me.szModule, "Rz") || strstr(me.szModule, "razer") || strstr(me.szModule, "Razer"))
                printf("MATCH: %s  <=  %s\n", me.szModule, me.szExePath);
        } while (Module32Next(snap, &me));
    } else {
        printf("Module32First failed: %lu\n", GetLastError());
    }
    printf("total_modules=%d\n", n);
    CloseHandle(snap);
    return 0;
}
