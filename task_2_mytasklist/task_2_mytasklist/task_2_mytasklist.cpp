#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wtsapi32.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "wtsapi32.lib")

using namespace std;

// thông số tự cho thôi
struct TableLayout {
    const int nameWidth = 40;
    const int pidWidth = 8;
    const int sessionNameWidth = 16;
    const int sessionIdWidth = 11;
    const int memUsageWidth = 12;
};

struct MemoryResult {
    SIZE_T bytes = 0;
    bool isAccessible = false;
};

struct ProcessModel {
    wstring imageName;
    DWORD pid;
    wstring sessionName;
    DWORD sessionId;
    MemoryResult memory;
};

// ==========================================
// 1. TẦNG TRUY VẤN DỮ LIỆU HỆ THỐNG
// ==========================================

MemoryResult FetchWorkingSetMemory(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess == NULL) return MemoryResult{ 0, false };

    PROCESS_MEMORY_COUNTERS pmc = { 0 };
    pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

    MemoryResult result{ 0, false };
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        result.bytes = pmc.WorkingSetSize;
        result.isAccessible = true;
    }

    CloseHandle(hProcess);
    return result;
}

wstring FetchSessionNameFromId(DWORD sessionId) {
    LPWSTR pSessionName = NULL;
    DWORD bytesReturned = 0;
    wstring name = L"";

    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSWinStationName, &pSessionName, &bytesReturned)) {
        if (pSessionName != NULL && wcslen(pSessionName) > 0) {
            name = pSessionName;
        }
        WTSFreeMemory(pSessionName);
    }
    return name;
}

wstring DetermineFallbackSessionName(DWORD sessionId) {
    wstring sessionName = FetchSessionNameFromId(sessionId);
    if (!sessionName.empty()) return sessionName;

    return (sessionId == 0) ? L"Services" : L"Console";
}

ProcessModel BuildProcessModel(const PROCESSENTRY32W& entry) {
    DWORD pid = entry.th32ProcessID;
    DWORD sessionId = 0;
    ProcessIdToSessionId(pid, &sessionId);

    return ProcessModel{
        entry.szExeFile,
        pid,
        DetermineFallbackSessionName(sessionId),
        sessionId,
        FetchWorkingSetMemory(pid)
    };
}

// ==========================================
// 2. TẦNG HIỂN THỊ GIAO DIỆN (ĐÃ TINH GỌN HÀM THỪA)
// ==========================================

void PrintHeader(const TableLayout& layout) {
    wcout << left << setw(layout.nameWidth) << L"Image Name"
        << right << setw(layout.pidWidth) << L"PID" << L" "
        << left << setw(layout.sessionNameWidth) << L"Session Name"
        << right << setw(layout.sessionIdWidth) << L"Session#"
        << right << setw(layout.memUsageWidth) << L"Mem Usage\n";

    // Tự động vẽ đường kẻ khớp 100% với cấu hình layout, không lo bị lệch lề
    wcout << wstring(layout.nameWidth, L'=') << L" "
        << wstring(layout.pidWidth, L'=') << L" "
        << wstring(layout.sessionNameWidth, L'=') << L" "
        << wstring(layout.sessionIdWidth, L'=') << L" "
        << wstring(layout.memUsageWidth, L'=') << L"\n";
}

void PrintRow(const ProcessModel& process, const TableLayout& layout) {
    wstring displayName = process.imageName;
    wstring memDisplay = L"N/A";
    if (process.memory.isAccessible) {
        wstringstream ss;
        ss.imbue(locale("en_US.UTF-8"));
        ss << (process.memory.bytes / 1024) << L" K";
        memDisplay = ss.str();
    }

    wcout << left << setw(layout.nameWidth) << displayName
        << right << setw(layout.pidWidth) << process.pid << L" "
        << left << setw(layout.sessionNameWidth) << process.sessionName
        << right << setw(layout.sessionIdWidth) << process.sessionId
        << right << setw(layout.memUsageWidth) << memDisplay << L"\n";
}

// ==========================================
// 3. TẦNG ĐIỀU PHỐI LOGIC CHÍNH
// ==========================================

void RunTasklist() {
    TableLayout layout;
    PrintHeader(layout);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &entry)) {
        do {
            ProcessModel process = BuildProcessModel(entry);
            PrintRow(process, layout);
        } while (Process32NextW(hSnapshot, &entry));
    }

    CloseHandle(hSnapshot);
}

int main() {
    _setmode(_fileno(stdout), _O_U16TEXT); // Ép console xuất UTF-16 sạch
    RunTasklist();
    return 0;
}