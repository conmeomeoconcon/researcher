#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

DWORD GetTargetProcessPID(const std::wstring& processName)
{
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE hSnapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD pid = 0;

    if (Process32FirstW(hSnapshot, &pe32))
    {
        do
        {
            if (_wcsicmp(
                processName.c_str(),
                pe32.szExeFile) == 0)
            {
                pid = pe32.th32ProcessID;
                break;
            }

        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    return pid;
}

bool FileExists(const char* path)
{
    DWORD attr =
        GetFileAttributesA(path);

    return (
        attr != INVALID_FILE_ATTRIBUTES &&
        !(attr & FILE_ATTRIBUTE_DIRECTORY)
        );
}

int main()
{
    std::wcout
        << L"===== Classic DLL Injection ====="
        << std::endl;

    //--------------------------------------------------
    // DLL PATH
    //--------------------------------------------------

    char dllPath[MAX_PATH];

    if (!GetFullPathNameA(
        "PandaDLL.dll",
        MAX_PATH,
        dllPath,
        NULL))
    {
        std::cout
            << "[-] Failed to resolve DLL path"
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] DLL Path: "
        << dllPath
        << std::endl;

    if (!FileExists(dllPath))
    {
        std::cout
            << "[-] DLL NOT FOUND"
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] DLL FOUND"
        << std::endl;

    //--------------------------------------------------
    // FIND PID
    //--------------------------------------------------

    DWORD pid =
        GetTargetProcessPID(
            L"notepad.exe");

    if (!pid)
    {
        std::cout
            << "[-] notepad.exe not found"
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] PID: "
        << pid
        << std::endl;

    //--------------------------------------------------
    // OPEN PROCESS
    //--------------------------------------------------

    HANDLE hProcess =
        OpenProcess(
            PROCESS_CREATE_THREAD |
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE |
            PROCESS_VM_READ,
            FALSE,
            pid);

    if (!hProcess)
    {
        std::cout
            << "[-] OpenProcess failed: "
            << GetLastError()
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] OpenProcess OK"
        << std::endl;

    //--------------------------------------------------
    // ALLOCATE MEMORY
    //--------------------------------------------------

    SIZE_T dllPathLen =
        strlen(dllPath) + 1;

    LPVOID remoteMemory =
        VirtualAllocEx(
            hProcess,
            NULL,
            dllPathLen,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE);

    if (!remoteMemory)
    {
        std::cout
            << "[-] VirtualAllocEx failed: "
            << GetLastError()
            << std::endl;

        CloseHandle(hProcess);
        return 1;
    }

    std::cout
        << "[+] Remote Memory: 0x"
        << std::hex
        << remoteMemory
        << std::endl;

    //--------------------------------------------------
    // WRITE DLL PATH
    //--------------------------------------------------

    if (!WriteProcessMemory(
        hProcess,
        remoteMemory,
        dllPath,
        dllPathLen,
        NULL))
    {
        std::cout
            << "[-] WriteProcessMemory failed: "
            << GetLastError()
            << std::endl;

        VirtualFreeEx(
            hProcess,
            remoteMemory,
            0,
            MEM_RELEASE);

        CloseHandle(hProcess);

        return 1;
    }

    std::cout
        << "[+] WriteProcessMemory OK"
        << std::endl;

    //--------------------------------------------------
    // LOADLIBRARY
    //--------------------------------------------------

    LPVOID loadLibraryAddr =
        (LPVOID)GetProcAddress(
            GetModuleHandleA(
                "kernel32.dll"),
            "LoadLibraryA");

    if (!loadLibraryAddr)
    {
        std::cout
            << "[-] GetProcAddress failed"
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] LoadLibraryA: 0x"
        << std::hex
        << loadLibraryAddr
        << std::endl;

    //--------------------------------------------------
    // CREATE REMOTE THREAD
    //--------------------------------------------------

    std::cout << "[+] DLL Path Written: "
        << dllPath
        << std::endl;

    DWORD attr = GetFileAttributesA(dllPath);

    std::cout
        << "[+] Attributes: "
        << attr
        << std::endl;

    HANDLE hThread =
        CreateRemoteThread(
            hProcess,
            NULL,
            0,
            (LPTHREAD_START_ROUTINE)loadLibraryAddr,
            remoteMemory,
            0,
            NULL);

    if (!hThread)
    {
        std::cout
            << "[-] CreateRemoteThread failed: "
            << GetLastError()
            << std::endl;

        return 1;
    }

    std::cout
        << "[+] CreateRemoteThread OK"
        << std::endl;


    //--------------------------------------------------
    // WAIT
    //--------------------------------------------------

    WaitForSingleObject(
        hThread,
        INFINITE);

    //--------------------------------------------------
    // RESULT
    //--------------------------------------------------

    DWORD exitCode = 0;

    GetExitCodeThread(
        hThread,
        &exitCode);

    std::cout
        << "[+] Thread ExitCode: 0x"
        << std::hex
        << exitCode
        << std::endl;

    if (exitCode == 0)
    {
        std::cout
            << "[-] LoadLibraryA FAILED"
            << std::endl;
    }
    else
    {
        std::cout
            << "[+] DLL LOADED SUCCESSFULLY"
            << std::endl;
    }


    //--------------------------------------------------
    // CLEANUP
    //--------------------------------------------------

    CloseHandle(hThread);

    VirtualFreeEx(
        hProcess,
        remoteMemory,
        0,
        MEM_RELEASE);

    CloseHandle(hProcess);

    system("pause");

    return 0;
}