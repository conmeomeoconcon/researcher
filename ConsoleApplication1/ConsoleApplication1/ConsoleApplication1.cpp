#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <string>

#pragma comment(lib, "Psapi.lib")

int main()
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        std::cout << "Failed to create snapshot\n";
        return 1;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe))
    {
        std::cout << "Process32First failed\n";
        CloseHandle(hSnapshot);
        return 1;
    }

    std::wcout
        << L"===============================================================================================\n";

    std::wcout
        << std::left
        << std::setw(40) << L"Image Name"
        << std::setw(10) << L"PID"
        << std::setw(15) << L"Session"
        << std::setw(12) << L"Session#"
        << std::setw(12) << L"Mem(MB)"
        << L"\n";

    std::wcout
        << L"===============================================================================================\n";

    for (
        BOOL ok = Process32First(hSnapshot, &pe);
        ok;
        ok = Process32Next(hSnapshot, &pe)
        )
    {
        DWORD sessionId = 0;
        std::wstring sessionName = L"N/A";
        SIZE_T memMB = 0;

        if (ProcessIdToSessionId(pe.th32ProcessID, &sessionId))
        {
            if (sessionId == 0)
                sessionName = L"Service";
            else
                sessionName = L"Console";
        }

        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
            FALSE,
            pe.th32ProcessID
        );

        if (hProcess)
        {
            PROCESS_MEMORY_COUNTERS pmc;

            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
            {
                memMB = pmc.WorkingSetSize / 1024 / 1024;
            }

            CloseHandle(hProcess);
        }

        std::wstring name = pe.szExeFile;

        if (name.length() > 38)
            name = name.substr(0, 38) + L"...";

        std::wcout
            << std::left
            << std::setw(40) << name
            << std::setw(10) << pe.th32ProcessID
            << std::setw(15) << sessionName
            << std::setw(12) << sessionId
            << std::setw(10) << memMB << L" MB"
            << L"\n";
    }
    CloseHandle(hSnapshot);

    return 0;
}