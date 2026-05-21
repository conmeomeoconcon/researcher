#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>

#pragma comment(lib, "Advapi32.lib")

std::wstring ReadRegistryString(HKEY hKeyRoot,
    const std::wstring& subKey,
    const std::wstring& valueName)
{
    HKEY hKey;

    if (RegOpenKeyExW(
        hKeyRoot,
        subKey.c_str(),
        0,
        KEY_READ,
        &hKey) != ERROR_SUCCESS)
    {
        return L"Unknown";
    }

    WCHAR buffer[512];
    DWORD bufferSize = sizeof(buffer);

    LONG result = RegQueryValueExW(
        hKey,
        valueName.c_str(),
        nullptr,
        nullptr,
        reinterpret_cast<LPBYTE>(buffer),
        &bufferSize);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS)
    {
        return L"Unknown";
    }

    return buffer;
}

void PrintSeparator()
{
    std::wcout << L"==================================================\n";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);

    PrintSeparator();
    std::wcout << L"               MY SYSTEM INFO\n";
    PrintSeparator();

    // =====================================================
    // COMPUTER NAME
    // =====================================================

    WCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = ARRAYSIZE(computerName);

    if (GetComputerNameExW(
        ComputerNamePhysicalDnsHostname,
        computerName,
        &size))
    {
        std::wcout
            << std::left
            << std::setw(25)
            << L"Computer Name:"
            << computerName
            << L'\n';
    }

    // =====================================================
    // OS INFO
    // =====================================================

    std::wstring osName =
        ReadRegistryString(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            L"ProductName");

    std::wstring buildNumber =
        ReadRegistryString(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            L"CurrentBuild");

    std::wstring displayVersion =
        ReadRegistryString(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            L"DisplayVersion");

    std::wcout
        << std::setw(25)
        << L"OS Name:"
        << osName
        << L'\n';

    std::wcout
        << std::setw(25)
        << L"OS Version:"
        << displayVersion
        << L" (Build "
        << buildNumber
        << L")\n";

    // =====================================================
    // CPU INFO
    // =====================================================

    std::wstring cpuName =
        ReadRegistryString(
            HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            L"ProcessorNameString");

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    std::wcout
        << std::setw(25)
        << L"Processor:"
        << cpuName
        << L'\n';

    std::wcout
        << std::setw(25)
        << L"CPU Cores:"
        << si.dwNumberOfProcessors
        << L'\n';

    // =====================================================
    // ARCHITECTURE
    // =====================================================

    std::wstring architecture;

    switch (si.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        architecture = L"x64";
        break;

    case PROCESSOR_ARCHITECTURE_INTEL:
        architecture = L"x86";
        break;

    case PROCESSOR_ARCHITECTURE_ARM64:
        architecture = L"ARM64";
        break;

    default:
        architecture = L"Unknown";
        break;
    }

    std::wcout
        << std::setw(25)
        << L"System Type:"
        << architecture
        << L'\n';

    // =====================================================
    // RAM INFO
    // =====================================================

    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(memInfo);

    if (GlobalMemoryStatusEx(&memInfo))
    {
        double totalRAM =
            static_cast<double>(memInfo.ullTotalPhys)
            / 1024 / 1024 / 1024;

        std::wcout
            << std::setw(25)
            << L"Installed RAM:"
            << std::fixed
            << std::setprecision(2)
            << totalRAM
            << L" GB\n";
    }

    // =====================================================
    // UPTIME
    // =====================================================

    ULONGLONG uptimeMs = GetTickCount64();

    ULONGLONG totalSeconds = uptimeMs / 1000;

    ULONGLONG days =
        totalSeconds / 86400;

    ULONGLONG hours =
        (totalSeconds % 86400) / 3600;

    ULONGLONG minutes =
        (totalSeconds % 3600) / 60;

    std::wcout
        << std::setw(25)
        << L"System Uptime:"
        << days
        << L" Days "
        << hours
        << L" Hours "
        << minutes
        << L" Minutes\n";

    PrintSeparator();

    system("pause");

    return 0;
}