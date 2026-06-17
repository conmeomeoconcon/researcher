#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "Shell32.lib")

DWORD WINAPI OpenCalculatorThread(LPVOID) {
    Sleep(300);
    ShellExecuteA(NULL, "open", "calc.exe", NULL, NULL, SW_SHOWNORMAL);
    return 0;
}

extern "C" __declspec(dllexport)
void CALLBACK RunCalc(HWND, HINSTANCE, LPSTR, int) {
    ShellExecuteA(NULL, "open", "calc.exe", NULL, NULL, SW_SHOWNORMAL);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(NULL, 0, OpenCalculatorThread, NULL, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        }
    }

    return TRUE;
}
