#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <io.h>
#include <fcntl.h>

using namespace std;

// ============================================================================
// CONSTANTS
// ============================================================================
#define LAYOUT_WIDTH 43
#define MAX_PATH_BUFFER 1024
#define MAX_MODULES 1024
#define MAX_THREADS 1024

// ============================================================================
// DATA STRUCTURES
// ============================================================================
struct ProcessCoreInfo {
    DWORD pid;
    DWORD parentPid;
    DWORD threadCount;
    BOOL found;
};

struct PrivilegeRecord {
    wstring name;
    wstring state;
};

struct ModuleRecord {
    wstring name;
    wstring fullPath;
    PVOID baseAddress;
    DWORD size;
};

struct ThreadRecord {
    DWORD id;
    LONG priority;
    wstring status;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Lấy tên file từ đường dẫn
void GetFileNameFromPath(const wchar_t* fullPath, wchar_t* fileName, DWORD size) {
    const wchar_t* lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(fullPath, L'/');
    if (lastSlash) {
        wcsncpy_s(fileName, size, lastSlash + 1, _TRUNCATE);
    }
    else {
        wcsncpy_s(fileName, size, fullPath, _TRUNCATE);
    }
}

// Chuyển đổi FILETIME sang chuỗi
void FileTimeToString(const FILETIME* ft, wchar_t* buffer, DWORD bufferSize) {
    FILETIME localTime;
    SYSTEMTIME st;

    if (!FileTimeToLocalFileTime(ft, &localTime)) {
        wcscpy_s(buffer, bufferSize, L"N/A");
        return;
    }

    if (!FileTimeToSystemTime(&localTime, &st)) {
        wcscpy_s(buffer, bufferSize, L"N/A");
        return;
    }

    swprintf_s(buffer, bufferSize, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
}

// Format memory size
void FormatMemorySize(SIZE_T bytes, wchar_t* buffer, DWORD bufferSize) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        swprintf_s(buffer, bufferSize, L"%.2f GB", gb);
    }
    else if (bytes >= 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        swprintf_s(buffer, bufferSize, L"%.1f MB", mb);
    }
    else if (bytes >= 1024) {
        swprintf_s(buffer, bufferSize, L"%llu KB", bytes / 1024);
    }
    else {
        swprintf_s(buffer, bufferSize, L"%llu B", bytes);
    }
}

// Format module size
void FormatModuleSize(DWORD bytes, wchar_t* buffer, DWORD bufferSize) {
    if (bytes >= 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        swprintf_s(buffer, bufferSize, L"%.1f MB", mb);
    }
    else {
        swprintf_s(buffer, bufferSize, L"%d KB", bytes / 1024);
    }
}

// Translate priority class
const wchar_t* GetPriorityClassName(DWORD priorityClass) {
    switch (priorityClass) {
    case IDLE_PRIORITY_CLASS: return L"IDLE_PRIORITY_CLASS";
    case BELOW_NORMAL_PRIORITY_CLASS: return L"BELOW_NORMAL_PRIORITY_CLASS";
    case NORMAL_PRIORITY_CLASS: return L"NORMAL_PRIORITY_CLASS";
    case ABOVE_NORMAL_PRIORITY_CLASS: return L"ABOVE_NORMAL_PRIORITY_CLASS";
    case HIGH_PRIORITY_CLASS: return L"HIGH_PRIORITY_CLASS";
    case REALTIME_PRIORITY_CLASS: return L"REALTIME_PRIORITY_CLASS";
    default: return L"NORMAL_PRIORITY_CLASS";
    }
}

// ============================================================================
// PROCESS FINDER FUNCTIONS
// ============================================================================

// Tìm process bằng PID
void FindProcessByPid(DWORD pid, ProcessCoreInfo* result) {
    ZeroMemory(result, sizeof(ProcessCoreInfo));

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                result->pid = pid;
                result->parentPid = pe.th32ParentProcessID;
                result->threadCount = pe.cntThreads;
                result->found = TRUE;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

// Tìm process bằng tên
void FindProcessByName(const wchar_t* name, ProcessCoreInfo* result) {
    ZeroMemory(result, sizeof(ProcessCoreInfo));

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                result->pid = pe.th32ProcessID;
                result->parentPid = pe.th32ParentProcessID;
                result->threadCount = pe.cntThreads;
                result->found = TRUE;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

// ============================================================================
// PROCESS INFORMATION EXTRACTOR FUNCTIONS
// ============================================================================

// Lấy đường dẫn executable
void GetProcessExecutablePath(HANDLE hProcess, wchar_t* path, DWORD pathSize) {
    DWORD size = pathSize;
    if (!QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        wcscpy_s(path, pathSize, L"N/A");
    }
}

// Lấy command line từ PEB
void GetProcessCommandLine(HANDLE hProcess, wchar_t* cmdLine, DWORD cmdSize) {
    wcscpy_s(cmdLine, cmdSize, L"N/A");

    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtDll) return;

    // NT API structures
    typedef LONG NTSTATUS;
    typedef struct _PROCESS_BASIC_INFORMATION {
        NTSTATUS ExitStatus;
        PVOID PebBaseAddress;
        ULONG_PTR AffinityMask;
        LONG BasePriority;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
    } PROCESS_BASIC_INFORMATION;

    typedef struct _UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    } UNICODE_STRING;

    typedef NTSTATUS(WINAPI* NtQueryInformationProcessPtr)(
        HANDLE, DWORD, PVOID, ULONG, PULONG);

    NtQueryInformationProcessPtr NtQueryInformationProcess =
        (NtQueryInformationProcessPtr)GetProcAddress(hNtDll, "NtQueryInformationProcess");

    if (!NtQueryInformationProcess) return;

    PROCESS_BASIC_INFORMATION pbi;
    ULONG returnLength = 0;

    if (NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &returnLength) != 0) {
        return;
    }

    // Đọc PEB
    ULONG_PTR pebOffset = (sizeof(PVOID) == 8) ? 0x20 : 0x10;
    PVOID processParametersAddr = NULL;

    if (!ReadProcessMemory(hProcess, (PBYTE)pbi.PebBaseAddress + pebOffset,
        &processParametersAddr, sizeof(PVOID), NULL)) {
        return;
    }

    // Đọc RTL_USER_PROCESS_PARAMETERS
    ULONG_PTR cmdLineOffset = (sizeof(PVOID) == 8) ? 0x70 : 0x40;
    UNICODE_STRING cmdLineUnicode;

    if (!ReadProcessMemory(hProcess, (PBYTE)processParametersAddr + cmdLineOffset,
        &cmdLineUnicode, sizeof(UNICODE_STRING), NULL)) {
        return;
    }

    size_t charCount = cmdLineUnicode.Length / sizeof(wchar_t);
    if (charCount > 0 && charCount < cmdSize) {
        vector<wchar_t> buffer(charCount + 1, L'\0');
        if (ReadProcessMemory(hProcess, cmdLineUnicode.Buffer, buffer.data(),
            cmdLineUnicode.Length, NULL)) {
            wcsncpy_s(cmdLine, cmdSize, buffer.data(), charCount);
        }
    }
}

// Lấy chủ sở hữu process
void GetProcessOwner(HANDLE hProcess, wchar_t* owner, DWORD ownerSize) {
    wcscpy_s(owner, ownerSize, L"N/A");

    HANDLE hToken = NULL;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) return;

    DWORD tokenSize = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &tokenSize);
    if (tokenSize == 0) {
        CloseHandle(hToken);
        return;
    }

    vector<BYTE> buffer(tokenSize);
    PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());

    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, tokenSize, &tokenSize)) {
        CloseHandle(hToken);
        return;
    }

    DWORD nameLen = 0, domainLen = 0;
    SID_NAME_USE sidUse;
    LookupAccountSidW(NULL, pTokenUser->User.Sid, NULL, &nameLen, NULL, &domainLen, &sidUse);

    if (nameLen > 0 && domainLen > 0) {
        vector<wchar_t> nameBuf(nameLen);
        vector<wchar_t> domainBuf(domainLen);
        if (LookupAccountSidW(NULL, pTokenUser->User.Sid, nameBuf.data(), &nameLen,
            domainBuf.data(), &domainLen, &sidUse)) {
            swprintf_s(owner, ownerSize, L"%s\\%s", domainBuf.data(), nameBuf.data());
        }
    }

    CloseHandle(hToken);
}

// Lấy thời gian tạo process
void GetProcessCreatedTime(HANDLE hProcess, wchar_t* timeStr, DWORD timeSize) {
    FILETIME createTime, exitTime, kernelTime, userTime;

    if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        FileTimeToString(&createTime, timeStr, timeSize);
    }
    else {
        wcscpy_s(timeStr, timeSize, L"N/A");
    }
}

// Lấy kiến trúc process
void GetProcessArchitecture(HANDLE hProcess, wchar_t* arch, DWORD archSize) {
    BOOL isWow64 = FALSE;
    if (!IsWow64Process(hProcess, &isWow64)) {
        wcscpy_s(arch, archSize, L"Unknown");
        return;
    }

    if (isWow64) {
        wcscpy_s(arch, archSize, L"x86 (32-bit on 64-bit)");
        return;
    }

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        wcscpy_s(arch, archSize, L"x64");
    }
    else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        wcscpy_s(arch, archSize, L"x86");
    }
    else {
        wcscpy_s(arch, archSize, L"Unknown");
    }
}

// ============================================================================
// EXTRACT PRIVILEGES
// ============================================================================

void ExtractPrivileges(HANDLE hProcess, vector<PrivilegeRecord>& privileges) {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) return;

    DWORD size = 0;
    GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &size);
    if (size == 0) {
        CloseHandle(hToken);
        return;
    }

    vector<BYTE> buffer(size);
    PTOKEN_PRIVILEGES pPrivs = reinterpret_cast<PTOKEN_PRIVILEGES>(buffer.data());

    if (!GetTokenInformation(hToken, TokenPrivileges, pPrivs, size, &size)) {
        CloseHandle(hToken);
        return;
    }

    for (DWORD i = 0; i < pPrivs->PrivilegeCount; i++) {
        DWORD nameLen = 0;
        LookupPrivilegeNameW(NULL, &pPrivs->Privileges[i].Luid, NULL, &nameLen);

        if (nameLen > 0) {
            vector<wchar_t> privName(nameLen);
            if (LookupPrivilegeNameW(NULL, &pPrivs->Privileges[i].Luid,
                privName.data(), &nameLen)) {
                PrivilegeRecord rec;
                rec.name = privName.data();
                rec.state = (pPrivs->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ?
                    L"Enabled" : L"Disabled";
                privileges.push_back(rec);
            }
        }
    }

    CloseHandle(hToken);
}

// ============================================================================
// EXTRACT MODULES
// ============================================================================

void ExtractModules(DWORD pid, vector<ModuleRecord>& modules) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
    if (Module32FirstW(hSnapshot, &me)) {
        do {
            ModuleRecord rec;
            rec.name = me.szModule;
            rec.fullPath = me.szExePath;
            rec.baseAddress = me.modBaseAddr;
            rec.size = me.modBaseSize;
            modules.push_back(rec);
        } while (Module32NextW(hSnapshot, &me));
    }

    CloseHandle(hSnapshot);
}

// ============================================================================
// EXTRACT THREADS
// ============================================================================

// Lấy trạng thái thực của thread
void GetThreadRealStatus(DWORD threadId, wchar_t* status, DWORD statusSize) {
    wcscpy_s(status, statusSize, L"Waiting");

    HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
    if (!hThread) return;

    // Kiểm tra thread time để xác định Running
    FILETIME cTime, eTime, kTime, uTime;
    if (GetThreadTimes(hThread, &cTime, &eTime, &kTime, &uTime)) {
        ULARGE_INTEGER kernelTime, userTime;
        kernelTime.LowPart = kTime.dwLowDateTime;
        kernelTime.HighPart = kTime.dwHighDateTime;
        userTime.LowPart = uTime.dwLowDateTime;
        userTime.HighPart = uTime.dwHighDateTime;

        if (kernelTime.QuadPart > 0 || userTime.QuadPart > 0) {
            wcscpy_s(status, statusSize, L"Running");
        }
    }

    // Kiểm tra suspended
    DWORD suspendCount = SuspendThread(hThread);
    if (suspendCount != (DWORD)-1) {
        ResumeThread(hThread);
        if (suspendCount > 0) {
            wcscpy_s(status, statusSize, L"Suspended");
        }
    }

    CloseHandle(hThread);
}

void ExtractThreads(DWORD pid, vector<ThreadRecord>& threads) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te = { sizeof(THREADENTRY32) };
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                ThreadRecord rec;
                rec.id = te.th32ThreadID;
                rec.priority = te.tpBasePri;

                wchar_t status[64];
                GetThreadRealStatus(te.th32ThreadID, status, 64);
                rec.status = status;

                threads.push_back(rec);
            }
        } while (Thread32Next(hSnapshot, &te));
    }

    CloseHandle(hSnapshot);
}

// ============================================================================
// EXTRACT SYSTEM CONTEXT
// ============================================================================

void GetSystemContext(HANDLE hProcess, BOOL* isWow64, PVOID* imageBase) {
    *isWow64 = FALSE;
    *imageBase = NULL;

    IsWow64Process(hProcess, isWow64);

    // Lấy PEB ImageBase
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtDll) return;

    typedef LONG NTSTATUS;
    typedef struct _PROCESS_BASIC_INFORMATION {
        NTSTATUS ExitStatus;
        PVOID PebBaseAddress;
        ULONG_PTR AffinityMask;
        LONG BasePriority;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
    } PROCESS_BASIC_INFORMATION;

    typedef NTSTATUS(WINAPI* NtQueryInformationProcessPtr)(
        HANDLE, DWORD, PVOID, ULONG, PULONG);

    NtQueryInformationProcessPtr NtQueryInformationProcess =
        (NtQueryInformationProcessPtr)GetProcAddress(hNtDll, "NtQueryInformationProcess");

    if (!NtQueryInformationProcess) return;

    PROCESS_BASIC_INFORMATION pbi;
    ULONG retLen = 0;

    if (NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &retLen) != 0) {
        return;
    }

    ULONG_PTR baseOffset = (sizeof(PVOID) == 8) ? 0x10 : 0x08;
    ReadProcessMemory(hProcess, (PBYTE)pbi.PebBaseAddress + baseOffset,
        imageBase, sizeof(PVOID), NULL);
}

// ============================================================================
// COLLECT ALL PROCESS INFORMATION
// ============================================================================

void CollectProcessInformation(HANDLE hProcess, DWORD pid,
    const ProcessCoreInfo* coreInfo,
    vector<PrivilegeRecord>& privileges,
    vector<ModuleRecord>& modules,
    vector<ThreadRecord>& threads,
    wchar_t* processName, DWORD nameSize,
    wchar_t* exePath, DWORD pathSize,
    wchar_t* cmdLine, DWORD cmdSize,
    wchar_t* owner, DWORD ownerSize,
    wchar_t* createdTime, DWORD timeSize,
    wchar_t* architecture, DWORD archSize,
    PROCESS_MEMORY_COUNTERS_EX* memory,
    DWORD* handleCount,
    DWORD* sessionId,
    DWORD* priorityClass,
    BOOL* isWow64,
    PVOID* imageBase) {

    // Basic info
    GetProcessExecutablePath(hProcess, exePath, pathSize);
    GetFileNameFromPath(exePath, processName, nameSize);
    GetProcessCommandLine(hProcess, cmdLine, cmdSize);
    GetProcessOwner(hProcess, owner, ownerSize);
    GetProcessCreatedTime(hProcess, createdTime, timeSize);
    GetProcessArchitecture(hProcess, architecture, archSize);

    // Session ID
    ProcessIdToSessionId(pid, sessionId);

    // Priority Class
    *priorityClass = GetPriorityClass(hProcess);

    // Handle Count
    GetProcessHandleCount(hProcess, handleCount);

    // Memory
    memory->cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);
    GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)memory, sizeof(*memory));

    // Privileges
    ExtractPrivileges(hProcess, privileges);

    // Modules
    ExtractModules(pid, modules);

    // Threads
    ExtractThreads(pid, threads);

    // System Context
    GetSystemContext(hProcess, isWow64, imageBase);
}

// ============================================================================
// PRINT FUNCTIONS (Presentation Layer)
// ============================================================================

void PrintFormatted(const wchar_t* label, const wchar_t* value) {
    wcout << left << setw(LAYOUT_WIDTH) << label << L": " << value << endl;
}

void PrintFormattedW(const wchar_t* label, const wstring& value) {
    wcout << left << setw(LAYOUT_WIDTH) << label << L": " << value << endl;
}

void PrintBasicInfo(const wchar_t* name, DWORD pid, DWORD parentPid,
    const wchar_t* exePath, const wchar_t* cmdLine,
    DWORD sessionId, DWORD priorityClass, DWORD handleCount,
    DWORD threadCount, const wchar_t* architecture,
    const wchar_t* createdTime, const wchar_t* owner) {

    wcout << L"[PROCESS BASIC INFO]" << endl;
    wcout << L"-------------------------------------------" << endl;
    PrintFormatted(L"Process Name", name);
    PrintFormatted(L"Process ID (PID)", to_wstring(pid).c_str());
    PrintFormatted(L"Parent PID", to_wstring(parentPid).c_str());
    PrintFormatted(L"Executable Path", exePath);
    PrintFormatted(L"Command Line", cmdLine);
    PrintFormatted(L"Session ID", to_wstring(sessionId).c_str());
    PrintFormatted(L"Priority Class", GetPriorityClassName(priorityClass));
    PrintFormatted(L"Handle Count", to_wstring(handleCount).c_str());
    PrintFormatted(L"Thread Count", to_wstring(threadCount).c_str());
    PrintFormatted(L"Architecture", architecture);
    PrintFormatted(L"Created Time", createdTime);
    PrintFormatted(L"User", owner);
    wcout << endl;
}

void PrintPrivileges(const vector<PrivilegeRecord>& privileges) {
    wcout << L"-------------------------------------------" << endl;
    wcout << L"[PRIVILEGES]" << endl;
    wcout << L"-------------------------------------------" << endl;

    if (privileges.empty()) {
        wcout << L"No privileges available or access denied." << endl;
    }
    else {
        for (const auto& priv : privileges) {
            wcout << left << setw(30) << priv.name << L": " << priv.state << endl;
        }
    }
    wcout << endl;
}

void PrintMemory(const PROCESS_MEMORY_COUNTERS_EX* memory) {
    wchar_t buffer[64];

    wcout << L"-------------------------------------------" << endl;
    wcout << L"[MEMORY USAGE]" << endl;
    wcout << L"-------------------------------------------" << endl;

    FormatMemorySize(memory->WorkingSetSize, buffer, 64);
    PrintFormatted(L"Working Set (RAM)", buffer);

    FormatMemorySize(memory->PrivateUsage, buffer, 64);
    PrintFormatted(L"Private Bytes", buffer);

    FormatMemorySize(memory->PagefileUsage, buffer, 64);
    PrintFormatted(L"Pagefile Usage", buffer);

    FormatMemorySize(memory->PeakWorkingSetSize, buffer, 64);
    PrintFormatted(L"Peak Working Set", buffer);
    wcout << endl;
}

void PrintModules(const vector<ModuleRecord>& modules, BOOL isWow64) {
    wcout << L"-------------------------------------------" << endl;
    wcout << L"[MODULES LOADED]" << endl;
    wcout << L"-------------------------------------------" << endl;
    wcout << left << setw(18) << L"Base Address"
        << L" " << left << setw(10) << L"Size"
        << L" Module Name (Full Path)" << endl;
    wcout << L"-------------------------------------------" << endl;

    int addressWidth = isWow64 ? 8 : 16;

    for (const auto& mod : modules) {
        wchar_t sizeStr[32];
        FormatModuleSize(mod.size, sizeStr, 32);

        wcout << L"0x" << uppercase << hex
            << setw(addressWidth) << setfill(L'0')
            << reinterpret_cast<ULONG_PTR>(mod.baseAddress)
            << nouppercase << dec << setfill(L' ')
            << L" " << right << setw(7) << sizeStr
            << L"    " << mod.fullPath << endl;
    }

    wcout << L"Total Modules Loaded : " << modules.size() << endl;
    wcout << endl;
}

void PrintThreads(const vector<ThreadRecord>& threads) {
    wcout << L"-------------------------------------------" << endl;
    wcout << L"[THREADS]" << endl;
    wcout << L"-------------------------------------------" << endl;
    wcout << left << setw(12) << L"Thread ID"
        << left << setw(16) << L"Base Priority"
        << L"Status" << endl;
    wcout << L"-------------------------------------------" << endl;

    // Sort by Thread ID
    vector<ThreadRecord> sortedThreads = threads;
    sort(sortedThreads.begin(), sortedThreads.end(),
        [](const ThreadRecord& a, const ThreadRecord& b) {
            return a.id < b.id;
        });

    for (const auto& th : sortedThreads) {
        wcout << left << setw(12) << th.id
            << left << setw(16) << th.priority
            << th.status << endl;
    }

    wcout << L"Total Threads: " << threads.size() << endl;
    wcout << endl;
}

void PrintSystemContext(BOOL isWow64, PVOID imageBase, const wchar_t* exePath) {
    wcout << L"-------------------------------------------" << endl;
    wcout << L"[SYSTEM CONTEXT]" << endl;
    wcout << L"-------------------------------------------" << endl;

    PrintFormatted(L"Process Bitness", (sizeof(PVOID) == 8) ? L"64-bit" : L"32-bit");
    PrintFormatted(L"OS Architecture", L"x64");
    PrintFormatted(L"IsWow64Process", isWow64 ? L"Yes" : L"No");

    wchar_t baseAddr[32];
    swprintf_s(baseAddr, 32, L"0x%p", imageBase);
    PrintFormatted(L"PEB ImageBaseAddress", baseAddr);
    PrintFormatted(L"Image Path from PEB", exePath);
    wcout << endl;
}

// ============================================================================
// MAIN REPORT FUNCTION
// ============================================================================

void PrintProcessReport(DWORD pid, const ProcessCoreInfo* coreInfo) {
    // Open process
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, pid
    );

    if (!hProcess) {
        wcerr << L"Error: Cannot open process " << pid << L". Error: " << GetLastError() << endl;
        return;
    }

    // Data buffers
    wchar_t processName[MAX_PATH_BUFFER] = { 0 };
    wchar_t exePath[MAX_PATH_BUFFER] = { 0 };
    wchar_t cmdLine[MAX_PATH_BUFFER * 2] = { 0 };
    wchar_t owner[MAX_PATH_BUFFER] = { 0 };
    wchar_t createdTime[64] = { 0 };
    wchar_t architecture[64] = { 0 };

    // Data structures
    vector<PrivilegeRecord> privileges;
    vector<ModuleRecord> modules;
    vector<ThreadRecord> threads;

    PROCESS_MEMORY_COUNTERS_EX memory = { 0 };
    DWORD handleCount = 0;
    DWORD sessionId = 0;
    DWORD priorityClass = 0;
    BOOL isWow64 = FALSE;
    PVOID imageBase = NULL;

    // Collect all information
    CollectProcessInformation(
        hProcess, pid, coreInfo,
        privileges, modules, threads,
        processName, MAX_PATH_BUFFER,
        exePath, MAX_PATH_BUFFER,
        cmdLine, MAX_PATH_BUFFER * 2,
        owner, MAX_PATH_BUFFER,
        createdTime, 64,
        architecture, 64,
        &memory,
        &handleCount,
        &sessionId,
        &priorityClass,
        &isWow64,
        &imageBase
    );

    // Print report
    wcout << L"===========================================" << endl;
    wcout << L"PROCESS INFORMATION REPORT" << endl;
    wcout << L"===========================================" << endl << endl;

    PrintBasicInfo(
        processName, pid, coreInfo->parentPid,
        exePath, cmdLine,
        sessionId, priorityClass, handleCount,
        coreInfo->threadCount,
        architecture, createdTime, owner
    );

    PrintPrivileges(privileges);
    PrintMemory(&memory);
    PrintModules(modules, isWow64);
    PrintThreads(threads);
    PrintSystemContext(isWow64, imageBase, exePath);

    wcout << L"===========================================" << endl;
    wcout << L"END OF REPORT" << endl;
    wcout << L"===========================================" << endl;

    CloseHandle(hProcess);
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int wmain(int argc, wchar_t* argv[]) {
    // Set console to Unicode
    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
        return 1;
    }

    // Parse arguments
    if (argc < 3) {
        wcout << L"Usage: processinfo.exe -n <process_name> or -p <PID>" << endl;
        wcout << L"Example: processinfo.exe -n notepad.exe" << endl;
        wcout << L"Example: processinfo.exe -p 1234" << endl;
        return 1;
    }

    wstring option = argv[1];
    wstring value = argv[2];

    ProcessCoreInfo coreInfo = { 0 };

    if (option == L"-p" || option == L"--pid") {
        DWORD pid = _wtoi(value.c_str());
        FindProcessByPid(pid, &coreInfo);
    }
    else if (option == L"-n" || option == L"--name") {
        FindProcessByName(value.c_str(), &coreInfo);
    }
    else {
        wcout << L"Error: Invalid option. Use -n or -p" << endl;
        return 1;
    }

    if (!coreInfo.found) {
        wcout << L"Error: Target process not found." << endl;
        return 1;
    }

    // Print report
    PrintProcessReport(coreInfo.pid, &coreInfo);

    return 0;
}