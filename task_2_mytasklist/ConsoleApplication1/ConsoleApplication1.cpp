#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <iphlpapi.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <cstdio>


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

using namespace std;

typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

static wstring GetHostName(){
	WCHAR hostname[256];
	DWORD size = 256;
	if (GetComputerNameW(hostname, &size))
		return hostname;
	else
		return L"Unknown";
}

static wstring OSConfiguration() {
    HKEY hKey;
    WCHAR productType[256];
    DWORD size = sizeof(productType);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"ProductType",
            RRF_RT_REG_SZ,
            NULL,
            &productType,
            &size
        ) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return productType;
        }

        RegCloseKey(hKey);
        return L"Unknown";
        
    }
    return L"Unknown";
}

static RTL_OSVERSIONINFOW WindowsVersion() {
        RTL_OSVERSIONINFOW info = { 0 };

	HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (!hMod) return info;

	RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
	if (!fn) return info;

    
    info.dwOSVersionInfoSize = sizeof(info);
    if (fn(&info) == 0)
        return info;

    return info;
}

static wstring OSName() {
	HKEY hKey;
	WCHAR productName[256];
	DWORD size = sizeof(productName);
	if (RegOpenKeyExW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
		0,
		KEY_READ,
		&hKey
	) == ERROR_SUCCESS)
	{
		if (RegGetValueW(
			hKey,
			NULL,
			L"ProductName",
			RRF_RT_REG_SZ,
			NULL,
			productName,
			&size
		) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return productName;
		}
		RegCloseKey(hKey);
	}
	return L"Unknown";
}

static wstring OSManufacturer()
{
    return L"Microsoft Corporation";
}

static wstring RegisteredOwner()
{
    HKEY hKey;
    WCHAR value[256];
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"RegisteredOwner",
            RRF_RT_REG_SZ,
            NULL,
            value,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return value;
        }

        RegCloseKey(hKey);
    }

    return L"N/A";
}

static wstring RegisteredOrganization()
{
    HKEY hKey;
    WCHAR value[256];
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"RegisteredOrganization",
            RRF_RT_REG_SZ,
            NULL,
            value,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return value;
        }

        RegCloseKey(hKey);
    }

    return L"N/A";
}

static wstring ProductID()
{
    HKEY hKey;
    WCHAR value[256];
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"ProductID",
            RRF_RT_REG_SZ,
            NULL,
            value,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return value;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring InstallDate()
{
    HKEY hKey;

    DWORD installDate = 0;
    DWORD size = sizeof(installDate);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"InstallDate",
            RRF_RT_REG_DWORD,
            NULL,
            &installDate,
            &size
        ) == ERROR_SUCCESS)
        {
            time_t t = (time_t)installDate;

            tm local_tm;
            localtime_s(&local_tm, &t);

            wstringstream ss;

            ss << put_time(
                &local_tm,
                L"%m/%d/%Y, %I:%M:%S %p"
            );

            RegCloseKey(hKey);

            return ss.str();
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring SystemManufacturer()
{
    HKEY hKey;
    WCHAR manufacturer[256];
    DWORD size = sizeof(manufacturer);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"SystemManufacturer",
            RRF_RT_REG_SZ,
            NULL,
            manufacturer,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return manufacturer;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring SystemModel()
{
    HKEY hKey;
    WCHAR model[256];
    DWORD size = sizeof(model);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"SystemProductName",
            RRF_RT_REG_SZ,
            NULL,
            model,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return model;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring SystemType()
{
    SYSTEM_INFO si;

    GetSystemInfo(&si);

    switch (si.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return L"x64-based PC";

    case PROCESSOR_ARCHITECTURE_INTEL:
        return L"x86-based PC";

    case PROCESSOR_ARCHITECTURE_ARM64:
        return L"ARM64-based PC";

    case PROCESSOR_ARCHITECTURE_ARM:
        return L"ARM-based PC";

    default:
        return L"Unknown architecture";
    }
}

static wstring ProcessorInfo()
{
    HKEY hKey;
    WCHAR processorName[256];
    DWORD size = sizeof(processorName);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"ProcessorNameString",
            RRF_RT_REG_SZ,
            NULL,
            processorName,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return processorName;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring SystemDirectory()
{
    WCHAR systemDir[MAX_PATH];

    UINT len = GetSystemDirectoryW(
        systemDir,
        MAX_PATH
    );

    if (len == 0 || len >= MAX_PATH)
        return L"Unknown";

    return systemDir;
}

static wstring WindowsDirectory()
{
    WCHAR windowsDir[MAX_PATH];

    UINT len = GetWindowsDirectoryW(
        windowsDir,
        MAX_PATH
    );

    if (len == 0 || len >= MAX_PATH)
        return L"Unknown";

    return windowsDir;
}

static wstring BootDevice()
{
    HKEY hKey;
    WCHAR bootDevice[256];
    DWORD size = sizeof(bootDevice);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"BootDevice",
            RRF_RT_REG_SZ,
            NULL,
            bootDevice,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return bootDevice;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring Timezone()
{
    HKEY hKey;
    WCHAR timeZone[256];
    DWORD size = sizeof(timeZone);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"TimeZoneKeyName",
            RRF_RT_REG_SZ,
            NULL,
            timeZone,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return timeZone;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring Locale()
{
    HKEY hKey;
    WCHAR locale[256];
    DWORD size = sizeof(locale);

    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Control Panel\\International",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"LocaleName",
            RRF_RT_REG_SZ,
            NULL,
            locale,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return locale;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static MEMORYSTATUSEX MemoryInfo()
{
    MEMORYSTATUSEX statex;

    statex.dwLength = sizeof(statex);

    if (GlobalMemoryStatusEx(&statex))
    {
        return statex;
    }

    return MEMORYSTATUSEX{ 0 };
}

static wstring Domain()
{
    WCHAR domain[256];

    DWORD size = 256;

    if (GetComputerNameExW(
        ComputerNameDnsDomain,
        domain,
        &size
    ))
    {
        return domain;
    }

    return L"WORKGROUP";
}

static wstring LogonServer()
{
    WCHAR logon[256];

    DWORD size = 256;

    if (GetEnvironmentVariableW(
        L"LOGONSERVER",
        logon,
        size
    ))
    {
        return logon;
    }

    return L"Unknown";
}

static wstring BIOSVendor()
{
    HKEY hKey;

    WCHAR vendor[256] = { 0 };
    DWORD size = sizeof(vendor);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"BIOSVendor",
            RRF_RT_REG_SZ,
            NULL,
            vendor,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return vendor;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring BIOSVersion()
{
    HKEY hKey;

    WCHAR version[256] = { 0 };
    DWORD size = sizeof(version);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"BIOSVersion",
            RRF_RT_REG_SZ,
            NULL,
            version,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return version;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

static wstring BIOSReleaseDate()
{
    HKEY hKey;

    WCHAR date[256] = { 0 };
    DWORD size = sizeof(date);

    if (RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\BIOS",
        0,
        KEY_READ,
        &hKey
    ) == ERROR_SUCCESS)
    {
        if (RegGetValueW(
            hKey,
            NULL,
            L"BIOSReleaseDate",
            RRF_RT_REG_SZ,
            NULL,
            date,
            &size
        ) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return date;
        }

        RegCloseKey(hKey);
    }

    return L"Unknown";
}

struct NetworkAdapter
{
    wstring name;
    wstring description;
    wstring mac;
    vector<wstring> ips;
};

vector<NetworkAdapter> GetNetworkAdapters()
{
    vector<NetworkAdapter> result;

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_UNSPEC;
    ULONG outBufLen = 15000;

    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);

    if (!pAddresses) return result;

    ULONG ret = GetAdaptersAddresses(
        family,
        flags,
        NULL,
        pAddresses,
        &outBufLen
    );

    if (ret == ERROR_BUFFER_OVERFLOW)
    {
        free(pAddresses);

        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);

        if (!pAddresses) return result;

        ret = GetAdaptersAddresses(
            family,
            flags,
            NULL,
            pAddresses,
            &outBufLen
        );
    }

    if (ret != NO_ERROR)
    {
        free(pAddresses);
        return result;
    }

    for (auto curr = pAddresses; curr; curr = curr->Next)
    {
        NetworkAdapter adapter;

        adapter.name = curr->FriendlyName;
        adapter.description = curr->Description;

        // MAC
        if (curr->PhysicalAddressLength)
        {
            wstringstream ss;

            for (DWORD i = 0; i < curr->PhysicalAddressLength; i++)
            {
                ss << hex << uppercase << setw(2) << setfill(L'0') << (int)curr->PhysicalAddress[i];

                if (i + 1 < curr->PhysicalAddressLength) ss << L"-";
            }

            adapter.mac = ss.str();
        }
        else
        {
            adapter.mac = L"None";
        }

        // IPs
        auto* pUnicast = curr->FirstUnicastAddress;

        while (pUnicast)
        {
            char buf[INET6_ADDRSTRLEN] = { 0 };

            auto* addr = pUnicast->Address.lpSockaddr;

            if (addr->sa_family == AF_INET)
            {
                auto* ipv4 = (sockaddr_in*)addr;

                inet_ntop(AF_INET,
                    &ipv4->sin_addr,
                    buf,
                    INET_ADDRSTRLEN);
            }
            else if (addr->sa_family == AF_INET6)
            {
                auto* ipv6 = (sockaddr_in6*)addr;

                inet_ntop(AF_INET6,
                    &ipv6->sin6_addr,
                    buf,
                    INET6_ADDRSTRLEN);
            }

            WCHAR wbuf[INET6_ADDRSTRLEN];

            MultiByteToWideChar(
                CP_UTF8,
                0,
                buf,
                -1,
                wbuf,
                INET6_ADDRSTRLEN
            );

            adapter.ips.push_back(wbuf);

            pUnicast = pUnicast->Next;
        }

        result.push_back(adapter);
    }

    free(pAddresses);

    return result;
}

void PrintNetworkAdapters(const vector<NetworkAdapter>& adapters)
{
    int idx = 1;

    for (const auto& a : adapters)
    {
        wcout << L"[" << idx++ << L"] "
            << a.name << endl;

        wcout << L"    Description: " << a.description << endl;
        wcout << L"    MAC: " << a.mac << endl;

        if (a.ips.empty())
        {
            wcout << L"    IP: None" << endl;
        }
        else
        {
            wcout << L"    IPs:" << endl;

            for (const auto& ip : a.ips)
            {
                wcout << L"        " << ip << endl;
            }
        }

        wcout << endl;
    }
}

void PrintSysteminfo() {
    wcout << L"Host name:                     " << GetHostName() << "\n";

    RTL_OSVERSIONINFOW info = WindowsVersion();
    wcout << L"OS Version:                    " << info.dwMajorVersion << L"." << info.dwMinorVersion << L"." << info.dwBuildNumber << endl;

    wcout << L"OS Name:                       " << OSName() << endl;

    wstring productType = OSConfiguration();
    wcout << L"OS Configuration:              " << productType << endl;

    wcout << L"OS Manufacturer:               " << OSManufacturer() << endl;

    wcout << L"Registered Owner:              " << RegisteredOwner() << endl;

    wcout << L"Registered Organization:       " << RegisteredOrganization() << endl;

    wcout << L"Product ID:                    " << ProductID() << endl;

    wcout << L"Original Install Date:         " << InstallDate() << endl;

    wcout << L"System Manufacturer:           " << SystemManufacturer() << endl;

    wcout << L"System Model:                  " << SystemModel() << endl;

    wcout << L"System Type:                   " << SystemType() << endl;

    wcout << L"Processor Information:         " << ProcessorInfo() << endl;

    wcout << L"System Directory:              " << SystemDirectory() << endl;

    wcout << L"Windows Directory:             " << WindowsDirectory() << endl;

    wcout << L"Boot Device:                   " << BootDevice() << endl;

    wcout << L"Time Zone:                     " << Timezone() << endl;

    wcout << L"System Locale:                 " << Locale() << endl;

    MEMORYSTATUSEX mem = MemoryInfo();

    wcout << L"Total Physical Memory:         " << mem.ullTotalPhys / (1024 * 1024) << L" MB" << endl;

    wcout << L"Available Physical Memory:     " << mem.ullAvailPhys / (1024 * 1024) << L" MB" << endl;

    wcout << L"BIOS Vendor:                   " << BIOSVendor() << endl;

    wcout << L"BIOS Version:                  " << BIOSVersion() << endl;

    wcout << L"BIOS Release Date:             " << BIOSReleaseDate() << endl;

    auto adapters = GetNetworkAdapters();
    PrintNetworkAdapters(adapters);
}

int main() {
    PrintSysteminfo();
    return 0;
}