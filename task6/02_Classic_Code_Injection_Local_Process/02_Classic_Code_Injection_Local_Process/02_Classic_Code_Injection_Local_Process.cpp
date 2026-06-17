#include <windows.h>
#include <iostream>


DWORD WINAPI Payload(LPVOID)
{
    MessageBoxA(
        NULL,
        "Hello from Local Process Lab",
        "Classic Code Injection Demo",
        MB_OK
    );

    return 0;
}


int main()
{
    SIZE_T size = 4096;


    // ==========================
    // 1. Memory Allocation
    // ==========================

    LPVOID buffer = VirtualAlloc(
        NULL,
        size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );


    if (!buffer)
    {
        std::cout << "VirtualAlloc failed\n";
        return 1;
    }


    std::cout
        << "[+] Allocated: "
        << buffer
        << std::endl;



    // ==========================
    // 2. Writing Memory
    // ==========================

    const char data[] =
        "Local Process Memory Test";


    RtlCopyMemory(
        buffer,
        data,
        sizeof(data)
    );


    std::cout
        << "[+] Memory written"
        << std::endl;



    // ==========================
    // 3. Executing Thread
    // ==========================

    HANDLE hThread = CreateThread(
        NULL,
        0,
        Payload,
        NULL,
        0,
        NULL
    );


    if (!hThread)
    {
        std::cout
            << "CreateThread failed\n";

        VirtualFree(
            buffer,
            0,
            MEM_RELEASE
        );

        return 1;
    }


    std::cout
        << "[+] Thread running"
        << std::endl;


    WaitForSingleObject(
        hThread,
        INFINITE
    );


    CloseHandle(hThread);


    VirtualFree(
        buffer,
        0,
        MEM_RELEASE
    );


    return 0;
}