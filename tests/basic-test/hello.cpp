// hello.dll - DllMain.cpp
#include <windows.h>
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        Sleep(1000);
        FILE* log = fopen("C:\\temp\\dll_log.txt", "w");
        if (log) {
            fprintf(log, "Hello from stealth DLL! Time: %d\n", GetTickCount());
            fclose(log);
        }
         MessageBoxA(NULL, "Hello from stealth DLL!", "Stealth Injection", MB_OK | MB_TOPMOST);
        break;
    }
    return TRUE;
}
