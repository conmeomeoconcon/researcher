#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <iphlpapi.h>
#include <lm.h>
#include <ctime>

// Khai báo liên kết thư viện cho Linker
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Netapi32.lib")
using namespace std;

// Cấu trúc lưu thông tin Card mạng động đầy đủ chi tiết
struct RawNetworkCard {
    wstring description;
    wstring connectionName;
    wstring status;
    wstring dhcpEnabled;
    wstring dhcpServer;
    wstring macAddress;
    vector<wstring> ipAddresses;
    vector<wstring> gateways;
    vector<wstring> dnsServers;
};

// ==========================================
// 🛠️ I. CÁC HÀM TIỆN ÍCH TRÍCH XUẤT THÔ (GET RAW UTILITIES)
// ==========================================

wstring GetRawComputerName() {
    WCHAR hostname[256] = { 0 };
    DWORD size = sizeof(hostname) / sizeof(hostname[0]);
    return GetComputerNameExW(ComputerNamePhysicalDnsHostname, hostname, &size) ? wstring(hostname) : L"N/A";
}

wstring GetRawEnvironmentVariable(LPCWSTR varName) {
    WCHAR buffer[512] = { 0 };
    return GetEnvironmentVariableW(varName, buffer, 512) > 0 ? wstring(buffer) : L"N/A";
}

wstring GetRawDynamicRegString(HKEY hKeyRoot, LPCWSTR subKey, LPCWSTR valueName) {
    HKEY hKey;
    WCHAR buffer[512] = { 0 };
    DWORD bufSize = sizeof(buffer);
    if (RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)buffer, &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return wstring(buffer);
        }
        RegCloseKey(hKey);
    }
    return L"N/A";
}

DWORD GetRawRegDword(HKEY hKeyRoot, LPCWSTR subKey, LPCWSTR valueName) {
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(DWORD);
    if (RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value;
        }
        RegCloseKey(hKey);
    }
    return 0;
}

wstring GetKernelVersion() {
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW osi = { 0 };
            osi.dwOSVersionInfoSize = sizeof(osi);
            if (pRtlGetVersion(&osi) == 0) {
                return to_wstring(osi.dwMajorVersion) + L"." + to_wstring(osi.dwMinorVersion) + L"." + to_wstring(osi.dwBuildNumber);
            }
        }
    }
    return L"10.0." + GetRawDynamicRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild");
}

wstring GetOSConfigurationReal() {
    LPWSTR buffer = nullptr;
    NETSETUP_JOIN_STATUS status = NetSetupUnknownStatus;
    if (NetGetJoinInformation(nullptr, &buffer, &status) == ERROR_SUCCESS) {
        if (buffer) NetApiBufferFree(buffer);
        if (status == NetSetupDomainName) return L"Member Workstation";
        if (status == NetSetupMachine) return L"Domain Controller";
    }
    return L"Standalone Workstation";
}

wstring FormatUnixTimestamp(time_t timestamp) {
    if (timestamp == 0) return L"N/A";
    tm tmbuf;
    localtime_s(&tmbuf, &timestamp);
    wchar_t wstr[64];
    wcsftime(wstr, 64, L"%m/%d/%Y, %I:%M:%S %p", &tmbuf);
    return wstring(wstr);
}

wstring CalculateSystemBootTime() {
    ULONGLONG uptimeMS = GetTickCount64();
    time_t now = time(0);
    time_t bootTime = now - (uptimeMS / 1000);
    return FormatUnixTimestamp(bootTime);
}

wstring GetSystemLocaleDetails() {
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
    WCHAR displayName[256] = { 0 };
    if (GetSystemDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
        GetLocaleInfoEx(localeName, LOCALE_SENGLISHDISPLAYNAME, displayName, 256);
        wstring lName(localeName);
        for (auto& c : lName) c = towlower(c);
        return lName + L";" + wstring(displayName);
    }
    return L"en-us;English (United States)";
}

wstring GetInputLocaleDetails() {
    HKL hkl = GetKeyboardLayout(0);
    WORD langId = LOWORD(hkl);
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
    WCHAR displayName[256] = { 0 };
    if (LCIDToLocaleName(langId, localeName, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
        GetLocaleInfoEx(localeName, LOCALE_SENGLISHDISPLAYNAME, displayName, 256);
        wstring lName(localeName);
        for (auto& c : lName) c = towlower(c);
        return lName + L";" + wstring(displayName);
    }
    return L"en-us;English (United States)";
}

// ==========================================
// 🖨️ II. CÁC HÀM HIỂN THỊ CON CHUYÊN TRÁCH (PRINT FUNCTIONS)
// ==========================================

void PrintSystemRow(const wstring& label, const wstring& value) {
    wcout << left << setw(32) << label << L" " << value << L"\n";
}

void PrintProcessorHeader(int total) {
    wcout << left << setw(32) << L"Processor(s):" << total << L" Processor(s) Installed.\n";
}

void PrintProcessorRow(int index, const wstring& cpuName) {
    wchar_t idxStr[16];
    swprintf_s(idxStr, L"   [%02d]:", index);
    wcout << left << setw(32) << idxStr << L" " << cpuName << L"\n";
}

void PrintHotfixHeader(int total) {
    wcout << left << setw(32) << L"Hotfix(s):" << total << L" Hotfix(s) Installed.\n";
}

void PrintHotfixRow(int index, const wstring& kbName) {
    wchar_t idxStr[16];
    swprintf_s(idxStr, L"   [%02d]:", index);
    wcout << left << setw(32) << idxStr << L" " << kbName << L"\n";
}

void PrintNetworkHeader(int total) {
    wcout << left << setw(32) << L"Network Card(s):" << total << L" NIC(s) Installed.\n";
}

void PrintNetworkCardDetails(int index, const RawNetworkCard& card) {
    wchar_t idxStr[16];
    swprintf_s(idxStr, L"   [%02d]:", index);
    wcout << left << setw(32) << idxStr << L" " << card.description << L"\n";
    wcout << left << setw(32) << L"" << L"      Connection Name: " << card.connectionName << L"\n";
    wcout << left << setw(32) << L"" << L"      Status:          " << card.status << L"\n";
    wcout << left << setw(32) << L"" << L"      DHCP Enabled:    " << card.dhcpEnabled << L"\n";
    if (card.dhcpEnabled == L"Yes" && !card.dhcpServer.empty()) {
        wcout << left << setw(32) << L"" << L"      DHCP Server:     " << card.dhcpServer << L"\n";
    }
    wcout << left << setw(32) << L"" << L"      MAC Address:     " << card.macAddress << L"\n";
}

void PrintVbsHeader(const wstring& status) {
    wcout << left << setw(32) << L"Virtualization-based security:" << L"Status: " << status << L"\n";
}

void PrintVbsSection(const wstring& sectionName, const vector<wstring>& properties) {
    wcout << left << setw(32) << L"" << L"   " << sectionName << L":\n";
    if (properties.empty()) {
        wcout << left << setw(32) << L"" << L"         None\n";
    }
    else {
        for (const auto& prop : properties) {
            wcout << left << setw(32) << L"" << L"         " << prop << L"\n";
        }
    }
}

// ==========================================
// 🚀 III. KHU VỰC THỰC THI BỘ LỌC (SIEVE FUNCTIONS)
// ==========================================

void ExecuteHostNameSieve() {
    PrintSystemRow(L"Host Name:", GetRawComputerName());
}

void ExecuteOSDetailsSieve() {
    LPCWSTR ntKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    wstring osName = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, ntKey, L"ProductName");
    wstring currentBuild = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, ntKey, L"CurrentBuild");

    if (currentBuild >= L"22000" && osName.find(L"Windows 10") != wstring::npos) {
        size_t pos = osName.find(L"Windows 10");
        osName.replace(pos, 10, L"Windows 11");
    }

    PrintSystemRow(L"OS Name:", osName);
    PrintSystemRow(L"OS Version:", GetKernelVersion() + L" N/A Build " + currentBuild);
    PrintSystemRow(L"OS Manufacturer:", L"Microsoft Corporation");
    PrintSystemRow(L"OS Configuration:", GetOSConfigurationReal());
    PrintSystemRow(L"OS Build Type:", L"Multiprocessor Free");
    PrintSystemRow(L"Registered Owner:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, ntKey, L"RegisteredOwner"));
    PrintSystemRow(L"Registered Organization:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, ntKey, L"RegisteredOrganization"));
    PrintSystemRow(L"Product ID:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, ntKey, L"ProductId"));
}

void ExecuteSystemDatesSieve() {
    time_t installTimestamp = GetRawRegDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"InstallDate");
    PrintSystemRow(L"Original Install Date:", FormatUnixTimestamp(installTimestamp));
    PrintSystemRow(L"System Boot Time:", CalculateSystemBootTime());
}

void ExecuteHardwareDetailsSieve() {
    LPCWSTR sysKey = L"HARDWARE\\DESCRIPTION\\System";
    PrintSystemRow(L"System Manufacturer:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, sysKey, L"SystemManufacturer"));
    PrintSystemRow(L"System Model:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, sysKey, L"SystemProductName"));

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    wstring systemType = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? L"x64-based PC" : L"x86-based PC";
    PrintSystemRow(L"System Type:", systemType);
}

void ExecuteProcessorSieve() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    wstring cpuName = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    DWORD mhz = GetRawRegDword(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz");

    wstring fullCpuStr = cpuName + L" ~" + to_wstring(mhz) + L" Mhz";
    PrintProcessorHeader(si.dwNumberOfProcessors);
    for (DWORD i = 1; i <= si.dwNumberOfProcessors; ++i) {
        PrintProcessorRow(i, fullCpuStr);
    }
}

void ExecuteBiosSieve() {
    LPCWSTR biosKey = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    wstring vendor = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, biosKey, L"BIOSVendor");
    wstring version = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, biosKey, L"BIOSVersion");
    wstring releaseDate = GetRawDynamicRegString(HKEY_LOCAL_MACHINE, biosKey, L"BIOSReleaseDate");
    PrintSystemRow(L"BIOS Version:", vendor + L" " + version + L", " + releaseDate);
}

void ExecuteEnvironmentPathsSieve() {
    PrintSystemRow(L"Windows Directory:", GetRawWindowsDirectory());
    PrintSystemRow(L"System Directory:", GetRawSystemDirectory());
    PrintSystemRow(L"Boot Device:", GetRawDynamicRegString(HKEY_LOCAL_MACHINE, L"SYSTEM\\Setup", L"SystemPartition"));
    PrintSystemRow(L"System Locale:", GetSystemLocaleDetails());
    PrintSystemRow(L"Input Locale:", GetInputLocaleDetails());

    DYNAMIC_TIME_ZONE_INFORMATION dtzi;
    if (GetDynamicTimeZoneInformation(&dtzi) != TIME_ZONE_ID_INVALID) {
        PrintSystemRow(L"Time Zone:", dtzi.TimeZoneKeyName);
    }
    else {
        PrintSystemRow(L"Time Zone:", L"N/A");
    }
}

void ExecuteMemorySieve() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&mem)) {
        DWORDLONG types[5] = { mem.ullTotalPhys, mem.ullAvailPhys, mem.ullTotalPageFile, mem.ullAvailPageFile, mem.ullTotalPageFile - mem.ullAvailPageFile };
        wstring labels[5] = { L"Total Physical Memory:", L"Available Physical Memory:", L"Virtual Memory: Max Size:", L"Virtual Memory: Available:", L"Virtual Memory: In Use:" };
        for (int i = 0; i < 5; i++) {
            wstring res = to_wstring(types[i] / (1024 * 1024));
            if (res.length() > 3) res.insert(res.length() - 3, L",");
            PrintSystemRow(labels[i], res + L" MB");
        }
    }
    PrintSystemRow(L"Page File Location(s):", L"C:\\pagefile.sys");
    PrintSystemRow(L"Domain:", GetRawEnvironmentVariable(L"USERDOMAIN"));
    PrintSystemRow(L"Logon Server:", GetRawEnvironmentVariable(L"LOGONSERVER"));
}

void ExecuteHotfixesSieve() {
    vector<wstring> list;
    FILE* pipe = _wpopen(L"powershell -Command \"Get-HotFix | Select-Object -ExpandProperty HotFixID\" 2>nul", L"r");
    if (pipe) {
        wchar_t buffer[128];
        while (fgetws(buffer, 128, pipe)) {
            wstring line(buffer);
            line.erase(line.find_last_not_of(L" \t\r\n") + 1);
            if (!line.empty() && line.find(L"KB") != wstring::npos) list.push_back(line);
        }
        _pclose(pipe);
    }
    PrintHotfixHeader((int)list.size());
    for (size_t i = 0; i < list.size(); ++i) {
        PrintHotfixRow((int)(i + 1), list[i]);
    }
}

void ExecuteNetworkCardsSieve() {
    vector<RawNetworkCard> cards;
    ULONG outBufLen = sizeof(IP_ADAPTER_ADDRESSES);
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES*)malloc(outBufLen);

    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    }

    if (pAddresses && GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
            if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

            RawNetworkCard card;
            card.description = pCurr->Description;
            card.connectionName = pCurr->FriendlyName;
            card.status = (pCurr->OperStatus == IfOperStatusUp) ? L"Connected" : L"Media disconnected";
            card.dhcpEnabled = (pCurr->Dhcpv4Enabled) ? L"Yes" : L"No";

            // Lấy địa chỉ MAC định dạng chuỗi gạch ngang hex
            wchar_t macBuf[32] = { 0 };
            if (pCurr->PhysicalAddressLength > 0) {
                swprintf_s(macBuf, L"%02X-%02X-%02X-%02X-%02X-%02X", pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1], pCurr->PhysicalAddress[2], pCurr->PhysicalAddress[3], pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
                card.macAddress = macBuf;
            }
            else { card.macAddress = L"N/A"; }

            if (pCurr->OperStatus == IfOperStatusUp) {
                if (pCurr->Dhcpv4Enabled && pCurr->Dhcpv4Server.iSockaddrLength > 0) {
                    char dhcpStr[INET_ADDRSTRLEN] = { 0 }; DWORD len = INET_ADDRSTRLEN;
                    WSAAddressToStringA(pCurr->Dhcpv4Server.lpSockaddr, pCurr->Dhcpv4Server.iSockaddrLength, NULL, dhcpStr, &len);
                    string s(dhcpStr); card.dhcpServer = wstring(s.begin(), s.end());
                }

                // Lấy danh sách IP Address (IPv4 & IPv6)
                for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                    char ipStr[INET6_ADDRSTRLEN] = { 0 }; DWORD ipStrLen = INET6_ADDRSTRLEN;
                    if (WSAAddressToStringA(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, NULL, ipStr, &ipStrLen) == 0) {
                        string s(ipStr); wstring ws(s.begin(), s.end());
                        size_t pct = ws.find(L'%'); if (pct != wstring::npos) ws = ws.substr(0, pct);
                        card.ipAddresses.push_back(ws);
                    }
                }

                // Lấy danh sách Gateways
                for (PIP_ADAPTER_GATEWAY_ADDRESS_LH pGw = pCurr->FirstGatewayAddress; pGw != NULL; pGw = pGw->Next) {
                    char gwStr[INET6_ADDRSTRLEN] = { 0 }; DWORD gwStrLen = INET6_ADDRSTRLEN;
                    if (WSAAddressToStringA(pGw->Address.lpSockaddr, pGw->Address.iSockaddrLength, NULL, gwStr, &gwStrLen) == 0) {
                        string s(gwStr); wstring ws(s.begin(), s.end());
                        card.gateways.push_back(ws);
                    }
                }

                // Lấy danh sách DNS Servers
                for (PIP_ADAPTER_DNS_SERVER_ADDRESS_XP pDns = pCurr->FirstDnsServerAddress; pDns != NULL; pDns = pDns->Next) {
                    char dnsStr[INET6_ADDRSTRLEN] = { 0 }; DWORD dnsStrLen = INET6_ADDRSTRLEN;
                    if (WSAAddressToStringA(pDns->Address.lpSockaddr, pDns->Address.iSockaddrLength, NULL, dnsStr, &dnsStrLen) == 0) {
                        string s(dnsStr); wstring ws(s.begin(), s.end());
                        card.dnsServers.push_back(ws);
                    }
                }
            }
            cards.push_back(card);
        }
    }
    if (pAddresses) free(pAddresses);

    PrintNetworkHeader((int)cards.size());
    for (size_t i = 0; i < cards.size(); ++i) {
        PrintNetworkCardDetails((int)(i + 1), cards[i]);
        if (cards[i].status == L"Connected") {
            // In Cụm IP
            if (!cards[i].ipAddresses.empty()) {
                PrintNetworkIpHeader();
                for (size_t j = 0; j < cards[i].ipAddresses.size(); ++j) PrintNetworkIpRow((int)(j + 1), cards[i].ipAddresses[j]);
            }
            // In Cụm Gateway và DNS dạng dòng phụ trợ giống systeminfo gộp nhóm
            if (!cards[i].gateways.empty()) {
                wcout << left << setw(32) << L"" << L"      Default Gateway: " << cards[i].gateways[0] << L"\n";
            }
            if (!cards[i].dnsServers.empty()) {
                wcout << left << setw(32) << L"" << L"      DNS Server(s):   " << cards[i].dnsServers[0] << L"\n";
                for (size_t d = 1; d < cards[i].dnsServers.size(); ++d) {
                    wcout << left << setw(32) << L"" << L"                       " << cards[i].dnsServers[d] << L"\n";
                }
            }
        }
    }
}

void ExecuteVbsSecuritySieve() {
    LPCWSTR dgKey = L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard";
    DWORD vbsEnabled = GetRawRegDword(HKEY_LOCAL_MACHINE, dgKey, L"EnableVirtualizationBasedSecurity");
    wstring status = (vbsEnabled == 1) ? L"Running" : L"Disabled";

    DWORD reqProps = GetRawRegDword(HKEY_LOCAL_MACHINE, dgKey, L"RequiredSecurityProperties");
    DWORD availProps = GetRawRegDword(HKEY_LOCAL_MACHINE, dgKey, L"AvailableSecurityProperties");

    auto bockTachProps = [](DWORD mask) -> vector<wstring> {
        vector<wstring> p;
        if (mask & 0x1) p.push_back(L"Base Virtualization Support");
        if (mask & 0x2) p.push_back(L"Secure Boot");
        if (mask & 0x4) p.push_back(L"DMA Protection");
        return p;
        };

    PrintVbsHeader(status);
    PrintVbsSection(L"Required Security Properties", bockTachProps(reqProps));
    PrintVbsSection(L"Available Security Properties", bockTachProps(availProps));

    // Đọc trạng thái các dịch vụ đang cấu hình / đang chạy thực tế
    DWORD hvci = GetRawRegDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", L"Enabled");
    vector<wstring> svc = (hvci == 1) ? vector<wstring>{ L"Hypervisor enforced Code Integrity" } : vector<wstring>{};

    PrintVbsSection(L"Services Configured", svc);
    PrintVbsSection(L"Services Running", svc);

    PrintSystemRow(L"   App Control for Business policy:", L"Enforced");
    PrintSystemRow(L"   App Control for Business user mode policy:", L"Off");
    PrintSystemRow(L"Hyper-V Requirements:", L"A hypervisor has been detected. Features required for Hyper-V will not be displayed.");
}

// ==========================================
// 🎮 IV. LUỒNG ĐIỀU PHỐI CHÍNH (MAIN)
// ==========================================

int main() {
    _wsetlocale(LC_ALL, L"");

    ExecuteHostNameSieve();
    ExecuteOSDetailsSieve();
    ExecuteSystemDatesSieve();
    ExecuteHardwareDetailsSieve();
    ExecuteProcessorSieve();
    ExecuteBiosSieve();
    ExecuteEnvironmentPathsSieve();
    ExecuteMemorySieve();
    ExecuteHotfixesSieve();
    ExecuteNetworkCardsSieve();
    ExecuteVbsSecuritySieve();

    return 0;
}