#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#include <sysinfoapi.h>
#include <lm.h>
#include <wbemidl.h>
#include <comdef.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "wbemuuid.lib")

using namespace std;

// ========================================================================
// HẰNG SỐ
// ========================================================================
const int LABEL_WIDTH = 32;
const wstring NA = L"N/A";

// ========================================================================
// CẤU TRÚC DỮ LIỆU
// ========================================================================

struct NetworkAdapter {
    int index;
    wstring friendlyName;
    wstring description;
    bool dhcpEnabled;
    vector<wstring> ipv4Addresses;
    vector<wstring> ipv6Addresses;
    wstring gateway;
    wstring dnsServer;
    wstring macAddress;
};

struct HotfixInfo {
    wstring id;
    wstring installedOn;
};

// ========================================================================
// HÀM TRÍCH XUẤT REGISTRY
// ========================================================================

wstring ReadRegistryString(HKEY hKeyRoot, const wstring& subKey, const wstring& valueName) {
    HKEY hKey;
    wstring result = NA;
    if (RegOpenKeyExW(hKeyRoot, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR buffer[1024] = { 0 };
        DWORD size = sizeof(buffer);
        if (RegQueryValueExW(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            result = buffer;
        }
        RegCloseKey(hKey);
    }
    return result;
}

DWORD ReadRegistryDword(HKEY hKeyRoot, const wstring& subKey, const wstring& valueName) {
    HKEY hKey;
    DWORD result = 0;
    if (RegOpenKeyExW(hKeyRoot, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueExW(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)&result, &size);
        RegCloseKey(hKey);
    }
    return result;
}

// ========================================================================
// HÀM TRÍCH XUẤT THÔNG TIN
// ========================================================================

// ----- HOST NAME -----
wstring GetHostName() {
    WCHAR buffer[256] = { 0 };
    DWORD size = sizeof(buffer) / sizeof(buffer[0]);
    if (GetComputerNameExW(ComputerNameDnsHostname, buffer, &size)) {
        return wstring(buffer);
    }
    return NA;
}

// ----- OS NAME -----
wstring GetOSName() {
    wstring result = ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");

    // Kiểm tra Windows 11
    if (result != NA) {
        wstring build = ReadRegistryString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild");
        if (build != NA && stoi(build) >= 22000 && result.find(L"Windows 10") != wstring::npos) {
            result.replace(result.find(L"Windows 10"), 10, L"Windows 11");
        }
    }
    return result;
}

// ----- OS VERSION -----
wstring GetOSVersion() {
    // Ưu tiên dùng RtlGetVersion
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto RtlGetVersion = (LONG(WINAPI*)(PRTL_OSVERSIONINFOW))GetProcAddress(hNtdll, "RtlGetVersion");
        if (RtlGetVersion) {
            RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
            if (RtlGetVersion(&osvi) == 0) {
                return to_wstring(osvi.dwMajorVersion) + L"." +
                    to_wstring(osvi.dwMinorVersion) + L"." +
                    to_wstring(osvi.dwBuildNumber);
            }
        }
    }
    return NA;
}

wstring GetOSBuildNumber() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild");
}

// ----- OS MANUFACTURER -----
wstring GetOSManufacturer() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"Manufacturer");
}

// ----- OS CONFIGURATION -----
wstring GetOSConfiguration() {
    // Kiểm tra Domain Controller
    WCHAR domain[256] = { 0 };
    DWORD size = sizeof(domain) / sizeof(domain[0]);
    if (GetComputerNameExW(ComputerNameDnsDomain, domain, &size) && wcslen(domain) > 0) {
        wstring domainName(domain);
        if (domainName != L"WORKGROUP" && !domainName.empty()) {
            // Kiểm tra có phải Domain Controller không
            LPWSTR serverName = NULL;
            DWORD level = 101;
            SERVER_INFO_101* pServerInfo = NULL;
            if (NetServerGetInfo(domain, level, (LPBYTE*)&pServerInfo) == NERR_Success) {
                if (pServerInfo->sv101_type & SV_TYPE_DOMAIN_CTRL) {
                    NetApiBufferFree(pServerInfo);
                    return L"Domain Controller";
                }
                NetApiBufferFree(pServerInfo);
            }
            return L"Member Server";
        }
    }
    return L"Standalone Workstation";
}

// ----- OS BUILD TYPE -----
wstring GetOSBuildType() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"ProductType");
}

// ----- REGISTERED OWNER -----
wstring GetRegisteredOwner() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"RegisteredOwner");
}

// ----- REGISTERED ORGANIZATION -----
wstring GetRegisteredOrganization() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"RegisteredOrganization");
}

// ----- PRODUCT ID -----
wstring GetProductId() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductId");
}

// ----- ORIGINAL INSTALL DATE -----
wstring GetInstallDate() {
    DWORD installDate = ReadRegistryDword(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"InstallDate");
    if (installDate > 0) {
        time_t rawTime = (time_t)installDate;
        tm localTm;
        if (localtime_s(&localTm, &rawTime) == 0) {
            wstringstream wss;
            wss << put_time(&localTm, L"%m/%d/%Y, %I:%M:%S %p");
            return wss.str();
        }
    }
    return NA;
}

// ----- SYSTEM BOOT TIME -----
wstring GetSystemBootTime() {
    ULONGLONG uptime = GetTickCount64();
    time_t now = time(NULL);
    time_t bootTime = now - (time_t)(uptime / 1000);
    tm localTm;
    if (localtime_s(&localTm, &bootTime) == 0) {
        wstringstream wss;
        wss << put_time(&localTm, L"%m/%d/%Y, %I:%M:%S %p");
        return wss.str();
    }
    return NA;
}

// ----- SYSTEM MANUFACTURER -----
wstring GetSystemManufacturer() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer");
}

// ----- SYSTEM MODEL -----
wstring GetSystemModel() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName");
}

// ----- SYSTEM TYPE -----
wstring GetSystemType() {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) return L"x64-based PC";
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) return L"x86-based PC";
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) return L"ARM64-based PC";
    return L"Unknown System Type";
}

// ----- PROCESSOR COUNT -----
int GetProcessorCount() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

// ----- PROCESSOR NAME -----
wstring GetProcessorName() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
}

// ----- PROCESSOR SPEED -----
wstring GetProcessorSpeed() {
    DWORD speed = ReadRegistryDword(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz");
    if (speed > 0) {
        return to_wstring(speed) + L" MHz";
    }
    return NA;
}

// ----- BIOS VENDOR -----
wstring GetBIOSVendor() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVendor");
}

// ----- BIOS VERSION -----
wstring GetBIOSVersion() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVersion");
}

// ----- BIOS RELEASE DATE -----
wstring GetBIOSReleaseDate() {
    return ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSReleaseDate");
}

// ----- WINDOWS DIRECTORY -----
wstring GetWindowsDirectory() {
    WCHAR buffer[MAX_PATH] = { 0 };
    return GetWindowsDirectoryW(buffer, MAX_PATH) > 0 ? wstring(buffer) : NA;
}

// ----- SYSTEM DIRECTORY -----
wstring GetSystemDirectory() {
    WCHAR buffer[MAX_PATH] = { 0 };
    return GetSystemDirectoryW(buffer, MAX_PATH) > 0 ? wstring(buffer) : NA;
}

// ----- BOOT DEVICE -----
wstring GetBootDevice() {
    wstring device = ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control", L"FirmwareBootDevice");
    if (device == NA || device.empty()) {
        device = ReadRegistryString(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control", L"BootDevice");
    }
    return device;
}

// ----- SYSTEM LOCALE -----
wstring GetSystemLocale() {
    WCHAR locale[LOCALE_NAME_MAX_LENGTH] = { 0 };
    return GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) ? wstring(locale) : NA;
}

// ----- INPUT LOCALE -----
wstring GetInputLocale() {
    WCHAR locale[LOCALE_NAME_MAX_LENGTH] = { 0 };
    return GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) ? wstring(locale) : NA;
}

// ----- TIME ZONE -----
wstring GetTimeZone() {
    TIME_ZONE_INFORMATION tzi;
    if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        return wstring(tzi.StandardName);
    }
    return NA;
}

// ----- TOTAL PHYSICAL MEMORY -----
wstring GetTotalPhysicalMemory() {
    MEMORYSTATUSEX statex = { sizeof(statex) };
    if (GlobalMemoryStatusEx(&statex)) {
        wstringstream wss;
        wss << (statex.ullTotalPhys / (1024 * 1024)) << L" MB";
        return wss.str();
    }
    return NA;
}

// ----- AVAILABLE PHYSICAL MEMORY -----
wstring GetAvailablePhysicalMemory() {
    MEMORYSTATUSEX statex = { sizeof(statex) };
    if (GlobalMemoryStatusEx(&statex)) {
        wstringstream wss;
        wss << (statex.ullAvailPhys / (1024 * 1024)) << L" MB";
        return wss.str();
    }
    return NA;
}

// ----- VIRTUAL MEMORY MAX SIZE -----
wstring GetVirtualMemoryMaxSize() {
    MEMORYSTATUSEX statex = { sizeof(statex) };
    if (GlobalMemoryStatusEx(&statex)) {
        wstringstream wss;
        wss << (statex.ullTotalPageFile / (1024 * 1024)) << L" MB";
        return wss.str();
    }
    return NA;
}

// ----- VIRTUAL MEMORY AVAILABLE -----
wstring GetVirtualMemoryAvailable() {
    MEMORYSTATUSEX statex = { sizeof(statex) };
    if (GlobalMemoryStatusEx(&statex)) {
        wstringstream wss;
        wss << (statex.ullAvailPageFile / (1024 * 1024)) << L" MB";
        return wss.str();
    }
    return NA;
}

// ----- VIRTUAL MEMORY IN USE -----
wstring GetVirtualMemoryInUse() {
    MEMORYSTATUSEX statex = { sizeof(statex) };
    if (GlobalMemoryStatusEx(&statex)) {
        ULONGLONG inUse = statex.ullTotalPageFile - statex.ullAvailPageFile;
        wstringstream wss;
        wss << (inUse / (1024 * 1024)) << L" MB";
        return wss.str();
    }
    return NA;
}

// ----- PAGE FILE LOCATION -----
wstring GetPageFileLocation() {
    wstring result = ReadRegistryString(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", L"PagingFiles");
    if (result != NA) {
        size_t pos = result.find(L' ');
        if (pos != wstring::npos) {
            result = result.substr(0, pos);
        }
        return result;
    }
    return NA;
}

// ----- DOMAIN -----
wstring GetDomainName() {
    WCHAR buffer[256] = { 0 };
    DWORD size = sizeof(buffer) / sizeof(buffer[0]);
    if (GetComputerNameExW(ComputerNameDnsDomain, buffer, &size) && wcslen(buffer) > 0) {
        return wstring(buffer);
    }
    return L"WORKGROUP";
}

// ----- LOGON SERVER -----
wstring GetLogonServer() {
    WCHAR buffer[256] = { 0 };
    return GetEnvironmentVariableW(L"LOGONSERVER", buffer, 256) > 0 ? wstring(buffer) : NA;
}

// ========================================================================
// HÀM TRÍCH XUẤT HOTFIX (WMI)
// ========================================================================

vector<HotfixInfo> GetHotfixes() {
    vector<HotfixInfo> hotfixes;

    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return hotfixes;

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) { CoUninitialize(); return hotfixes; }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { pLoc->Release(); CoUninitialize(); return hotfixes; }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM Win32_QuickFixEngineering"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

    if (SUCCEEDED(hres)) {
        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;
        while (pEnumerator) {
            hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (uReturn == 0) break;

            VARIANT vtProp;
            HotfixInfo hf;

            VariantInit(&vtProp);
            if (SUCCEEDED(pclsObj->Get(L"HotFixID", 0, &vtProp, 0, 0))) {
                if (vtProp.vt == VT_BSTR) hf.id = vtProp.bstrVal;
                VariantClear(&vtProp);
            }

            VariantInit(&vtProp);
            if (SUCCEEDED(pclsObj->Get(L"InstalledOn", 0, &vtProp, 0, 0))) {
                if (vtProp.vt == VT_BSTR) hf.installedOn = vtProp.bstrVal;
                VariantClear(&vtProp);
            }

            if (!hf.id.empty()) hotfixes.push_back(hf);
            pclsObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    return hotfixes;
}

// ========================================================================
// HÀM TRÍCH XUẤT NETWORK
// ========================================================================

vector<NetworkAdapter> GetNetworkAdapters() {
    vector<NetworkAdapter> adapters;
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
    if (!pAddresses) return adapters;

    ULONG ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        if (!pAddresses) return adapters;
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen);
    }

    if (ret == NO_ERROR) {
        int idx = 1;
        for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr; curr = curr->Next) {
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK || curr->IfType == IF_TYPE_TUNNEL) continue;
            if (curr->OperStatus != IfOperStatusUp) continue;

            NetworkAdapter adapter;
            adapter.index = idx++;
            adapter.friendlyName = curr->FriendlyName;
            adapter.description = curr->Description;
            adapter.dhcpEnabled = (curr->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;

            // MAC Address
            if (curr->PhysicalAddressLength > 0) {
                wstringstream wss;
                for (int i = 0; i < curr->PhysicalAddressLength; i++) {
                    if (i > 0) wss << L"-";
                    wss << hex << setw(2) << setfill(L'0') << (int)curr->PhysicalAddress[i];
                }
                adapter.macAddress = wss.str();
            }

            // IP Addresses và Gateway
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = curr->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                char buf[INET6_ADDRSTRLEN] = { 0 };
                SOCKADDR* addr = pUnicast->Address.lpSockaddr;

                if (addr->sa_family == AF_INET) {
                    inet_ntop(AF_INET, &((sockaddr_in*)addr)->sin_addr, buf, INET_ADDRSTRLEN);
                    wchar_t wBuf[INET6_ADDRSTRLEN];
                    size_t outSize;
                    mbstowcs_s(&outSize, wBuf, buf, INET6_ADDRSTRLEN);
                    adapter.ipv4Addresses.push_back(wBuf);
                }
                else if (addr->sa_family == AF_INET6) {
                    inet_ntop(AF_INET6, &((sockaddr_in6*)addr)->sin6_addr, buf, INET6_ADDRSTRLEN);
                    wchar_t wBuf[INET6_ADDRSTRLEN];
                    size_t outSize;
                    mbstowcs_s(&outSize, wBuf, buf, INET6_ADDRSTRLEN);
                    adapter.ipv6Addresses.push_back(wBuf);
                }
            }

            // Gateway
            PIP_ADAPTER_GATEWAY_ADDRESS pGateway = curr->FirstGatewayAddress;
            if (pGateway) {
                char buf[INET6_ADDRSTRLEN] = { 0 };
                SOCKADDR* addr = pGateway->Address.lpSockaddr;
                if (addr->sa_family == AF_INET) {
                    inet_ntop(AF_INET, &((sockaddr_in*)addr)->sin_addr, buf, INET_ADDRSTRLEN);
                    wchar_t wBuf[INET6_ADDRSTRLEN];
                    size_t outSize;
                    mbstowcs_s(&outSize, wBuf, buf, INET6_ADDRSTRLEN);
                    adapter.gateway = wBuf;
                }
            }

            // DNS Server
            PIP_ADAPTER_DNS_SERVER_ADDRESS pDns = curr->FirstDnsServerAddress;
            if (pDns) {
                char buf[INET6_ADDRSTRLEN] = { 0 };
                SOCKADDR* addr = pDns->Address.lpSockaddr;
                if (addr->sa_family == AF_INET) {
                    inet_ntop(AF_INET, &((sockaddr_in*)addr)->sin_addr, buf, INET_ADDRSTRLEN);
                    wchar_t wBuf[INET6_ADDRSTRLEN];
                    size_t outSize;
                    mbstowcs_s(&outSize, wBuf, buf, INET6_ADDRSTRLEN);
                    adapter.dnsServer = wBuf;
                }
            }

            if (!adapter.ipv4Addresses.empty() || !adapter.ipv6Addresses.empty()) {
                adapters.push_back(adapter);
            }
        }
    }
    free(pAddresses);
    return adapters;
}

// ========================================================================
// HÀM TRÍCH XUẤT HYPER-V
// ========================================================================

bool IsHypervisorPresent() {
    // NtAPI: Kiểm tra hypervisor
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto NtQuerySystemInformation = (NTSTATUS(WINAPI*)(int, PVOID, ULONG, PULONG))
            GetProcAddress(hNtdll, "NtQuerySystemInformation");
        if (NtQuerySystemInformation) {
            // SystemHypervisorInformation = 94
            DWORD hypervisorInfo = 0;
            ULONG returnLength = 0;
            if (NtQuerySystemInformation(94, &hypervisorInfo, sizeof(hypervisorInfo), &returnLength) == 0) {
                return hypervisorInfo != 0;
            }
        }
    }
    return false;
}

bool HasCpuVirtualization() {
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 1);
    return (cpuInfo[2] & (1 << 5)) != 0;  // VMX (Intel) hoặc SVM (AMD)
}

bool HasCpuSlat() {
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 0x80000001);
    return (cpuInfo[3] & (1 << 6)) != 0;  // SLAT
}

bool HasDataExecutionPrevention() {
    return IsProcessorFeaturePresent(PF_NX_ENABLED) != FALSE;
}

bool HasFirmwareVirtualization() {
    // Registry: BIOS
    DWORD value = ReadRegistryDword(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"FeatureSet");
    return (value & (1 << 5)) != 0;
}

// ========================================================================
// HÀM HIỂN THỊ
// ========================================================================

void PrintRow(const wstring& label, const wstring& value) {
    wcout << L"    " << left << setw(LABEL_WIDTH) << label << L": " << value << endl;
}

void PrintRowNA(const wstring& label, const wstring& value) {
    if (value == NA || value.empty()) {
        wcout << L"    " << left << setw(LABEL_WIDTH) << label << L": " << L"N/A" << endl;
    }
    else {
        wcout << L"    " << left << setw(LABEL_WIDTH) << label << L": " << value << endl;
    }
}

void PrintProcessorRow(int index, const wstring& cpuName) {
    wcout << L"                                [" << setfill(L'0') << setw(2) << index
        << L"]: " << setfill(L' ') << cpuName << endl;
}

void PrintHyperVRow(const wstring& property, bool supported) {
    wcout << L"    " << left << setw(LABEL_WIDTH) << property << L": "
        << (supported ? L"Yes" : L"No") << endl;
}

void PrintNetworkCard(const NetworkAdapter& adapter) {
    wcout << L"                                    [" << setfill(L'0') << setw(2) << adapter.index
        << L"]: " << setfill(L' ') << adapter.description << endl;
    wcout << L"                                          Connection Name: " << adapter.friendlyName << endl;
    wcout << L"                                          DHCP Enabled:    "
        << (adapter.dhcpEnabled ? L"Yes" : L"No") << endl;
    wcout << L"                                          MAC Address:     " << adapter.macAddress << endl;

    if (!adapter.ipv4Addresses.empty()) {
        wcout << L"                                          IPv4 Address(es):" << endl;
        // SỬA: Dùng size_t
        for (size_t i = 0; i < adapter.ipv4Addresses.size(); ++i) {
            wcout << L"                                              [" << (i + 1)
                << L"]: " << adapter.ipv4Addresses[i] << endl;
        }
    }

    if (!adapter.ipv6Addresses.empty()) {
        wcout << L"                                          IPv6 Address(es):" << endl;
        // SỬA: Dùng size_t
        for (size_t i = 0; i < adapter.ipv6Addresses.size(); ++i) {
            wcout << L"                                              [" << (i + 1)
                << L"]: " << adapter.ipv6Addresses[i] << endl;
        }
    }

    if (!adapter.gateway.empty()) {
        wcout << L"                                          Gateway:         " << adapter.gateway << endl;
    }

    if (!adapter.dnsServer.empty()) {
        wcout << L"                                          DNS Server:      " << adapter.dnsServer << endl;
    }
}

// ========================================================================
// HÀM THỰC THI
// ========================================================================

void ExecuteSystemInfo() {
    // ----- HOST -----
    PrintRow(L"Host Name", GetHostName());

    // ----- OS -----
    PrintRow(L"OS Name", GetOSName());
    PrintRow(L"OS Version", GetOSVersion());
    PrintRow(L"OS Manufacturer", GetOSManufacturer());
    PrintRow(L"OS Configuration", GetOSConfiguration());
    PrintRow(L"OS Build Type", GetOSBuildType());
    PrintRowNA(L"Registered Owner", GetRegisteredOwner());
    PrintRowNA(L"Registered Organization", GetRegisteredOrganization());
    PrintRowNA(L"Product ID", GetProductId());

    // ----- SYSTEM DATES -----
    PrintRowNA(L"Original Install Date", GetInstallDate());
    PrintRow(L"System Boot Time", GetSystemBootTime());

    // ----- HARDWARE -----
    PrintRowNA(L"System Manufacturer", GetSystemManufacturer());
    PrintRowNA(L"System Model", GetSystemModel());
    PrintRow(L"System Type", GetSystemType());
    PrintRow(L"Processor(s)", to_wstring(GetProcessorCount()) + L" Processor(s) Installed.");
    PrintProcessorRow(1, GetProcessorName());
    PrintRowNA(L"Processor Speed", GetProcessorSpeed());
    PrintRowNA(L"BIOS Vendor", GetBIOSVendor());
    PrintRowNA(L"BIOS Version", GetBIOSVersion());
    PrintRowNA(L"BIOS Release Date", GetBIOSReleaseDate());

    // ----- DIRECTORIES -----
    PrintRow(L"Windows Directory", GetWindowsDirectory());
    PrintRow(L"System Directory", GetSystemDirectory());
    PrintRowNA(L"Boot Device", GetBootDevice());
    PrintRowNA(L"System Locale", GetSystemLocale());
    PrintRowNA(L"Input Locale", GetInputLocale());
    PrintRowNA(L"Logon Server", GetLogonServer());

    // ----- HOTFIX -----
    vector<HotfixInfo> hotfixes = GetHotfixes();
    PrintRow(L"Hotfix(s)", to_wstring(hotfixes.size()) + L" Hotfix(s) Installed.");
    for (const auto& hf : hotfixes) {
        wcout << L"                                    [" << hf.id << L"]: " << hf.installedOn << endl;
    }

    // ----- TIME ZONE -----
    PrintRowNA(L"Time Zone", GetTimeZone());

    // ----- MEMORY -----
    PrintRow(L"Total Physical Memory", GetTotalPhysicalMemory());
    PrintRow(L"Available Physical Memory", GetAvailablePhysicalMemory());
    PrintRow(L"Virtual Memory: Max Size", GetVirtualMemoryMaxSize());
    PrintRow(L"Virtual Memory: Available", GetVirtualMemoryAvailable());
    PrintRow(L"Virtual Memory: In Use", GetVirtualMemoryInUse());
    PrintRowNA(L"Page File Location(s)", GetPageFileLocation());

    // ----- DOMAIN -----
    PrintRow(L"Domain", GetDomainName());

    // ----- NETWORK -----
    vector<NetworkAdapter> adapters = GetNetworkAdapters();
    PrintRow(L"Network Card(s)", to_wstring(adapters.size()) + L" NIC(s) Installed.");
    for (const auto& adapter : adapters) {
        PrintNetworkCard(adapter);
    }

    // ----- HYPER-V -----
    bool hypervisorPresent = IsHypervisorPresent();
    PrintRow(L"Hypervisor Present", hypervisorPresent ? L"Yes" : L"No");
    PrintRow(L"Hyper-V Requirements", L"");
    PrintHyperVRow(L"VM Monitor Mode Extensions", HasCpuVirtualization());
    PrintHyperVRow(L"Virtualization Enabled In Firmware", HasFirmwareVirtualization());
    PrintHyperVRow(L"Second Level Address Translation", HasCpuSlat());
    PrintHyperVRow(L"Data Execution Prevention Available", HasDataExecutionPrevention());
}

// ========================================================================
// MAIN
// ========================================================================

int main() {
    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
        return 1;
    }

    ExecuteSystemInfo();

    return 0;
}