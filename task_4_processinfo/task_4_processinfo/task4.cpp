#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Advapi32.lib")

using namespace std;

void PrintBasicInfo(DWORD pid);
void PrintPrivileges(DWORD pid);
void PrintMemoryInfo(DWORD pid);
void PrintModules(DWORD pid);
void PrintThreads(DWORD pid);
void PrintSystemContext(DWORD pid);

wstring GetPriorityClassString(DWORD priority);

DWORD GetPidByName(const wstring& processName);

int wmain(int argc, wchar_t* argv[])
{
    DWORD pid = 0;

    if (argc != 3)
    {
        wcout << L"Usage:\n";
        wcout << L"project.exe -p <PID>\n";
        wcout << L"project.exe -n <ProcessName>\n";
        return 0;
    }

    wstring option = argv[1];

    if (option == L"-p")
    {
        pid = _wtoi(argv[2]);
    }
    else if (option == L"-n")
    {
        pid = GetPidByName(argv[2]);

        if (pid == 0)
        {
            wcout << L"Process not found\n";
            return 0;
        }
    }
    else
    {
        wcout << L"Invalid option\n";
        return 0;
    }

    wcout << L"\n===========================================\n";
    wcout << L"PROCESS INFORMATION REPORT\n";
    wcout << L"===========================================\n\n";

    PrintBasicInfo(pid);

    wcout << endl;
    PrintPrivileges(pid);

    wcout << endl;
    PrintMemoryInfo(pid);

    wcout << endl;
    PrintModules(pid);

    wcout << endl;
    PrintThreads(pid);

    wcout << endl;
    PrintSystemContext(pid);

    wcout << L"\n===========================================\n";
    wcout << L"END OF REPORT\n";
    wcout << L"===========================================\n";

    return 0;
}

DWORD GetPidByName(const wstring& processName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0)
            {
                CloseHandle(hSnap);
                return pe.th32ProcessID;
            }

        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    return 0;
}

void PrintBasicInfo(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!hProcess)
    {
        wcout << L"Failed to open process\n";
        return;
    }

    wcout << L"[PROCESS BASIC INFO]\n";
    wcout << L"-------------------------------------------\n";

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    HANDLE hSnap = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (Process32First(hSnap, &pe))
    {
        do
        {
            if (pe.th32ProcessID == pid)
            {
                wcout << L"Process Name       : "
                    << pe.szExeFile << endl;

                wcout << L"Parent PID         : "
                    << pe.th32ParentProcessID << endl;

                wcout << L"Thread Count       : "
                    << pe.cntThreads << endl;

                break;
            }

        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);

    wcout << L"Process ID (PID)   : "
        << pid << endl;

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(
        hProcess,
        0,
        path,
        &size
    ))
    {
        wcout << L"Executable Path    : "
            << path << endl;

        wcout << L"Command Line       : "
            << L"\"" << L"N/A" << L"\"" << endl;
    }

    DWORD sessionId;

    if (ProcessIdToSessionId(
        pid,
        &sessionId
    ))
    {
        wcout << L"Session ID         : "
            << sessionId << endl;
    }

    DWORD priority = GetPriorityClass(hProcess);

    wcout << L"Priority Class     : "
        << GetPriorityClassString(priority)
        << endl;

    DWORD handleCount = 0;

    if (GetProcessHandleCount(
        hProcess,
        &handleCount
    ))
    {
        wcout << L"Handle Count       : "
            << handleCount << endl;
    }

    BOOL wow64 = FALSE;

    if (IsWow64Process(hProcess, &wow64))
    {
        wcout << L"Architecture       : ";

        if (wow64)
        {
            wcout << L"x86 (WOW64)\n";
        }
        else
        {
            wcout << L"x64\n";
        }
    }

    FILETIME create, exit, kernel, user;

    if (GetProcessTimes(
        hProcess,
        &create,
        &exit,
        &kernel,
        &user
    ))
    {
        SYSTEMTIME st;

        FileTimeToSystemTime(
            &create,
            &st
        );

        wcout << L"Created Time       : ";

        wcout << setfill(L'0');

        wcout
            << st.wYear << L"-"
            << setw(2) << st.wMonth << L"-"
            << setw(2) << st.wDay << L" "
            << setw(2) << st.wHour << L":"
            << setw(2) << st.wMinute << L":"
            << setw(2) << st.wSecond
            << endl;

        wcout << setfill(L' ');
    }

    HANDLE hToken;

    if (OpenProcessToken(
        hProcess,
        TOKEN_QUERY,
        &hToken
    ))
    {
        DWORD tokenSize = 0;

        GetTokenInformation(
            hToken,
            TokenUser,
            NULL,
            0,
            &tokenSize
        );

        vector<BYTE> buffer(tokenSize);

        if (GetTokenInformation(
            hToken,
            TokenUser,
            buffer.data(),
            tokenSize,
            &tokenSize
        ))
        {
            TOKEN_USER* tokenUser =
                (TOKEN_USER*)buffer.data();

            WCHAR name[256];
            WCHAR domain[256];

            DWORD nameSize = 256;
            DWORD domainSize = 256;

            SID_NAME_USE sidType;

            if (LookupAccountSidW(
                NULL,
                tokenUser->User.Sid,
                name,
                &nameSize,
                domain,
                &domainSize,
                &sidType
            ))
            {
                wcout << L"User               : "
                    << domain
                    << L"\\"
                    << name
                    << endl;
            }
        }

        CloseHandle(hToken);
    }

    CloseHandle(hProcess);
}

void PrintPrivileges(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION,
        FALSE,
        pid
    );

    if (!hProcess)
        return;

    HANDLE hToken;

    if (!OpenProcessToken(
        hProcess,
        TOKEN_QUERY,
        &hToken
    ))
    {
        CloseHandle(hProcess);
        return;
    }

    DWORD size = 0;

    GetTokenInformation(
        hToken,
        TokenPrivileges,
        NULL,
        0,
        &size
    );

    vector<BYTE> buffer(size);

    if (GetTokenInformation(
        hToken,
        TokenPrivileges,
        buffer.data(),
        size,
        &size
    ))
    {
        TOKEN_PRIVILEGES* privileges =
            (TOKEN_PRIVILEGES*)buffer.data();

        wcout << L"[PRIVILEGES]\n";
        wcout << L"-------------------------------------------\n";

        wcout << left << setfill(L' ');

        for (DWORD i = 0; i < privileges->PrivilegeCount; i++)
        {
            WCHAR privilegeName[256];
            DWORD nameSize = 256;

            LookupPrivilegeNameW(
                NULL,
                &privileges->Privileges[i].Luid,
                privilegeName,
                &nameSize
            );

            bool enabled =
                privileges->Privileges[i].Attributes &
                SE_PRIVILEGE_ENABLED;

            wcout
                << setw(35)
                << privilegeName
                << L": "
                << (enabled ? L"Enabled" : L"Disabled")
                << endl;
        }
    }

    CloseHandle(hToken);
    CloseHandle(hProcess);
}

void PrintMemoryInfo(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!hProcess)
        return;

    PROCESS_MEMORY_COUNTERS_EX pmc;

    if (GetProcessMemoryInfo(
        hProcess,
        (PROCESS_MEMORY_COUNTERS*)&pmc,
        sizeof(pmc)
    ))
    {
        wcout << L"[MEMORY USAGE]\n";
        wcout << L"-------------------------------------------\n";

        wcout << fixed << setprecision(1);

        wcout << L"Working Set (RAM)  : "
            << pmc.WorkingSetSize / 1024.0 / 1024.0
            << L" MB" << endl;

        wcout << L"Private Bytes      : "
            << pmc.PrivateUsage / 1024.0 / 1024.0
            << L" MB" << endl;

        wcout << L"Pagefile Usage     : "
            << pmc.PagefileUsage / 1024.0 / 1024.0
            << L" MB" << endl;

        wcout << L"Peak Working Set   : "
            << pmc.PeakWorkingSetSize / 1024.0 / 1024.0
            << L" MB" << endl;
    }

    CloseHandle(hProcess);
}

void PrintModules(DWORD pid)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE |
        TH32CS_SNAPMODULE32,
        pid
    );

    if (hSnapshot == INVALID_HANDLE_VALUE)
        return;

    MODULEENTRY32 me;
    me.dwSize = sizeof(me);

    int count = 0;

    wcout << L"[MODULES LOADED]\n";
    wcout << L"-------------------------------------------\n";

    wcout << left << setfill(L' ');

    wcout
        << setw(18) << L"Base Address"
        << setw(12) << L"Size"
        << L"Module Name (Full Path)"
        << endl;

    wcout << L"-------------------------------------------\n";

    if (Module32First(hSnapshot, &me))
    {
        do
        {
            count++;

            wcout
                << setw(18) << me.modBaseAddr
                << setw(12)
                << (to_wstring(me.modBaseSize / 1024) + L" KB")
                << me.szExePath
                << endl;

        } while (Module32Next(hSnapshot, &me));
    }

    wcout << endl;

    wcout << L"Total Modules Loaded : "
        << count << endl;

    CloseHandle(hSnapshot);
}

void PrintThreads(DWORD pid)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPTHREAD,
        0
    );

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        wcout << L"Failed to create thread snapshot\n";
        return;
    }

    THREADENTRY32 te;
    te.dwSize = sizeof(te);

    int count = 0;

    wcout << L"[THREADS]\n";
    wcout << L"-------------------------------------------\n";

    wcout << left << setfill(L' ');

    wcout
        << setw(15) << L"Thread ID"
        << setw(20) << L"Base Priority"
        << setw(15) << L"Status"
        << endl;

    wcout << L"-------------------------------------------\n";

    if (Thread32First(hSnapshot, &te))
    {
        do
        {
            if (te.th32OwnerProcessID == pid)
            {
                count++;

                wstring status = L"Running";

                HANDLE hThread = OpenThread(
                    THREAD_SUSPEND_RESUME |
                    THREAD_QUERY_INFORMATION,
                    FALSE,
                    te.th32ThreadID
                );

                if (hThread)
                {
                    DWORD suspendCount =
                        SuspendThread(hThread);

                    if (suspendCount != (DWORD)-1)
                    {
                        ResumeThread(hThread);

                        if (suspendCount > 0)
                        {
                            status = L"Suspended";
                        }
                    }
                    else
                    {
                        status = L"Unknown";
                    }

                    CloseHandle(hThread);
                }

                wcout
                    << setw(15) << te.th32ThreadID
                    << setw(20) << te.tpBasePri
                    << setw(15) << status
                    << endl;
            }

        } while (Thread32Next(hSnapshot, &te));
    }

    wcout << endl;

    wcout << L"Total Threads: "
        << count << endl;

    CloseHandle(hSnapshot);
}

void PrintSystemContext(DWORD pid)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION,
        FALSE,
        pid
    );

    if (!hProcess)
        return;

    wcout << L"[SYSTEM CONTEXT]\n";
    wcout << L"-------------------------------------------\n";

    SYSTEM_INFO si;

    GetNativeSystemInfo(&si);

    BOOL wow64 = FALSE;

    IsWow64Process(
        hProcess,
        &wow64
    );

    wcout << L"Process Bitness      : "
        << (wow64 ? L"32-bit" : L"64-bit")
        << endl;

    wcout << L"OS Architecture      : ";

    if (si.wProcessorArchitecture ==
        PROCESSOR_ARCHITECTURE_AMD64)
    {
        wcout << L"x64\n";
    }
    else
    {
        wcout << L"x86\n";
    }

    wcout << L"IsWow64Process       : "
        << (wow64 ? L"Yes" : L"No")
        << endl;

    CloseHandle(hProcess);
}

wstring GetPriorityClassString(DWORD priority)
{
    switch (priority)
    {
    case IDLE_PRIORITY_CLASS:
        return L"IDLE_PRIORITY_CLASS";

    case BELOW_NORMAL_PRIORITY_CLASS:
        return L"BELOW_NORMAL_PRIORITY_CLASS";

    case NORMAL_PRIORITY_CLASS:
        return L"NORMAL_PRIORITY_CLASS";

    case ABOVE_NORMAL_PRIORITY_CLASS:
        return L"ABOVE_NORMAL_PRIORITY_CLASS";

    case HIGH_PRIORITY_CLASS:
        return L"HIGH_PRIORITY_CLASS";

    case REALTIME_PRIORITY_CLASS:
        return L"REALTIME_PRIORITY_CLASS";

    default:
        return L"UNKNOWN";
    }
}
