#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <filesystem>
using namespace std;
using namespace std::filesystem;

// ============================================================================
// CẤU TRÚC KẾT QUẢ
// ============================================================================

struct InjectionResult {
    bool success;
    HANDLE hProcess;
    LPVOID remoteMemory;
    HANDLE hThread;
    DWORD threadId;
    DWORD lastError;
    const char* errorMsg;
};

// ============================================================================
// HÀM KHỞI TẠO
// ============================================================================

void InitResult(InjectionResult* result) {
    result->success = false;
    result->hProcess = NULL;
    result->remoteMemory = NULL;
    result->hThread = NULL;
    result->threadId = 0;
    result->lastError = 0;
    result->errorMsg = "Unknown";
}

// ============================================================================
// HÀM KIỂM TRA HANDLE
// ============================================================================

bool IsValidHandle(HANDLE h) {
    return (h != NULL && h != INVALID_HANDLE_VALUE);
}

// ============================================================================
// HÀM DỌN DẸP
// ============================================================================

void Cleanup(InjectionResult* result) {
    if (IsValidHandle(result->hThread)) {
        CloseHandle(result->hThread);
        result->hThread = NULL;
    }

    if (result->remoteMemory != NULL && IsValidHandle(result->hProcess)) {
        VirtualFreeEx(result->hProcess, result->remoteMemory, 0, MEM_RELEASE);
        result->remoteMemory = NULL;
    }

    if (IsValidHandle(result->hProcess)) {
        CloseHandle(result->hProcess);
        result->hProcess = NULL;
    }
}

// ============================================================================
// HÀM LẤY ĐƯỜNG DẪN DLL (CÙNG THƯ MỤC VỚI EXE)
// ============================================================================

string GetDllPath() {
    // Lấy đường dẫn của file exe hiện tại
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);

    string exePath = buffer;
    size_t pos = exePath.find_last_of("\\/");

    if (pos != string::npos) {
        string dllPath = exePath.substr(0, pos + 1) + "panda.dll";
        return dllPath;
    }

    return "panda.dll";
}

// ============================================================================
// HÀM KIỂM TRA FILE TỒN TẠI
// ============================================================================

bool CheckDLLExists(const string& dllPath) {
    if (exists(dllPath)) {
        cout << "[+] DLL file found: " << dllPath << endl;
        cout << "[+] File size: " << file_size(dllPath) << " bytes" << endl;
        return true;
    }

    cerr << "[!] DLL file not found: " << dllPath << endl;
    cerr << "[!] Please place panda.dll in the same folder as the injector." << endl;
    return false;
}

// ============================================================================
// HÀM TÌM PROCESS THEO TÊN
// ============================================================================

DWORD FindProcessIdByName(const wstring& processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        cerr << "CreateToolhelp32Snapshot failed. Error: " << GetLastError() << endl;
        return 0;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    wstring targetLower = processName;
    for (auto& c : targetLower) c = towlower(c);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            wstring currentLower = pe32.szExeFile;
            for (auto& c : currentLower) c = towlower(c);

            if (currentLower == targetLower) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processId;
}

// ============================================================================
// HÀM LẤY PROCESS HANDLE
// ============================================================================

HANDLE GetProcessHandle(DWORD pid) {
    HANDLE hProcess = OpenProcess(
        PROCESS_ALL_ACCESS,
        FALSE,
        pid
    );

    if (hProcess == NULL) {
        cerr << "OpenProcess failed. Error: " << GetLastError() << endl;
        return NULL;
    }

    cout << "[+] Process handle obtained: 0x" << hex << hProcess << dec << endl;
    return hProcess;
}

// ============================================================================
// HÀM LẤY - ALLOCATE MEMORY
// ============================================================================

bool AllocateRemoteMemory(InjectionResult* result, size_t dllPathLen) {
    cout << "[+] Allocating " << dllPathLen << " bytes for DLL path..." << endl;

    result->remoteMemory = VirtualAllocEx(
        result->hProcess,
        NULL,
        dllPathLen,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (result->remoteMemory == NULL) {
        result->lastError = GetLastError();
        result->errorMsg = "VirtualAllocEx failed";
        cerr << "VirtualAllocEx failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Memory allocated at: 0x" << hex << result->remoteMemory << dec << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - WRITE DLL PATH
// ============================================================================

bool WriteDllPathToRemote(InjectionResult* result, const string& dllPath) {
    cout << "[+] Writing DLL path to remote process..." << endl;

    SIZE_T bytesWritten = 0;
    BOOL success = WriteProcessMemory(
        result->hProcess,
        result->remoteMemory,
        dllPath.c_str(),
        dllPath.length() + 1,
        &bytesWritten
    );

    if (!success || bytesWritten != dllPath.length() + 1) {
        result->lastError = GetLastError();
        result->errorMsg = "WriteProcessMemory failed";
        cerr << "WriteProcessMemory failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] DLL path written successfully (" << bytesWritten << " bytes)" << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - GET LoadLibraryA ADDRESS
// ============================================================================

LPVOID GetLoadLibraryAAddress() {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

    if (hKernel32 == NULL) {
        cerr << "Failed to get kernel32.dll handle" << endl;
        return NULL;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

    if (pLoadLibrary == NULL) {
        cerr << "Failed to get address of LoadLibraryA" << endl;
        return NULL;
    }

    cout << "[+] LoadLibraryA address: 0x" << hex << pLoadLibrary << dec << endl;
    return pLoadLibrary;
}

// ============================================================================
// HÀM LẤY - CREATE REMOTE THREAD
// ============================================================================

bool CreateRemoteThread(InjectionResult* result, LPVOID pLoadLibrary) {
    cout << "[+] Creating remote thread to load DLL..." << endl;

    result->hThread = CreateRemoteThread(
        result->hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary,
        result->remoteMemory,
        0,
        &result->threadId
    );

    if (!IsValidHandle(result->hThread)) {
        result->lastError = GetLastError();
        result->errorMsg = "CreateRemoteThread failed";
        cerr << "CreateRemoteThread failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Remote thread created. Thread ID: " << result->threadId << endl;
    return true;
}

// ============================================================================
// HÀM THỰC THI DLL INJECTION
// ============================================================================

bool ExecuteDLLInjection(InjectionResult* result, const wstring& targetProcess, const string& dllPath) {
    cout << "\n[+] === STARTING DLL INJECTION ===" << endl;
    cout << "[+] Target process: ";
    wcout << targetProcess << endl;
    cout << "[+] DLL path: " << dllPath << endl;

    // Kiểm tra file DLL tồn tại
    if (!CheckDLLExists(dllPath)) {
        result->errorMsg = "DLL file not found";
        return false;
    }

    cout << "\n[+] Step 1: Finding target process..." << endl;
    DWORD pid = FindProcessIdByName(targetProcess);

    if (pid == 0) {
        result->errorMsg = "Target process not found";
        cerr << "Target process not found: ";
        wcerr << targetProcess << endl;
        return false;
    }

    cout << "[+] Process found. PID: " << pid << endl;

    cout << "\n[+] Step 2: Getting process handle..." << endl;
    result->hProcess = GetProcessHandle(pid);

    if (!IsValidHandle(result->hProcess)) {
        result->errorMsg = "Failed to get process handle";
        return false;
    }

    cout << "\n[+] Step 3: Allocating memory..." << endl;
    if (!AllocateRemoteMemory(result, dllPath.length() + 1)) {
        Cleanup(result);
        return false;
    }

    cout << "\n[+] Step 4: Writing DLL path..." << endl;
    if (!WriteDllPathToRemote(result, dllPath)) {
        Cleanup(result);
        return false;
    }

    cout << "\n[+] Step 5: Getting LoadLibraryA address..." << endl;
    LPVOID pLoadLibrary = GetLoadLibraryAAddress();

    if (pLoadLibrary == NULL) {
        result->errorMsg = "Failed to get LoadLibraryA address";
        Cleanup(result);
        return false;
    }

    cout << "\n[+] Step 6: Creating remote thread..." << endl;
    if (!CreateRemoteThread(result, pLoadLibrary)) {
        Cleanup(result);
        return false;
    }

    cout << "\n[+] Step 7: Waiting for DLL to load (3s)..." << endl;
    Sleep(3000);

    cout << "\n[+] Step 8: Cleanup..." << endl;

    if (IsValidHandle(result->hThread)) {
        CloseHandle(result->hThread);
        result->hThread = NULL;
        cout << "[+] Remote thread handle closed." << endl;
    }

    if (result->remoteMemory != NULL) {
        VirtualFreeEx(result->hProcess, result->remoteMemory, 0, MEM_RELEASE);
        result->remoteMemory = NULL;
        cout << "[+] Remote memory freed." << endl;
    }

    if (IsValidHandle(result->hProcess)) {
        CloseHandle(result->hProcess);
        result->hProcess = NULL;
        cout << "[+] Process handle closed." << endl;
    }

    result->success = true;
    result->errorMsg = "Success";
    cout << "\n[+] === DLL INJECTION SUCCESSFUL ===" << endl;

    return true;
}

// ============================================================================
// HÀM IN BANNER
// ============================================================================

void PrintBanner() {
    cout << "============================================================" << endl;
    cout << "  CLASSIC DLL INJECTION" << endl;
    cout << "============================================================" << endl;
    cout << "[+] Technique: Remote Thread Injection (LoadLibraryA)" << endl;
    cout << "[+] DLL: panda.dll (cùng thư mục với exe)" << endl;
    cout << "============================================================" << endl;
}

// ============================================================================
// HÀM IN KẾT QUẢ
// ============================================================================

void PrintResult(const InjectionResult* result) {
    cout << "\n============================================================" << endl;
    if (result->success) {
        cout << "  [SUCCESS] DLL injected and loaded!" << endl;
        cout << "  [+] Check if Calculator opened." << endl;
        cout << "  [+] Target process should have loaded panda.dll" << endl;
    }
    else {
        cout << "  [FAILED] DLL injection failed!" << endl;
        cout << "  [+] Error: " << result->errorMsg << endl;
        if (result->lastError != 0) {
            cout << "  [+] Code: 0x" << hex << result->lastError << dec << endl;
        }
    }
    cout << "============================================================" << endl;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    PrintBanner();

    wstring targetProcess = L"notepad.exe";

    // Lấy đường dẫn DLL cùng thư mục với exe
    string dllPath = GetDllPath();

    InjectionResult result;
    InitResult(&result);

    if (ExecuteDLLInjection(&result, targetProcess, dllPath)) {
        PrintResult(&result);
    }
    else {
        PrintResult(&result);
    }

    cout << "\nPress Enter to exit..." << endl;
    cin.get();

    return result.success ? 0 : 1;
}