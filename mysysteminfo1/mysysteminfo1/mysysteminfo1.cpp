#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>       
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <iphlpapi.h>
#include <ws2tcpip.h>       

// Ép buộc liên kết thư viện hệ thống
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ws2_32.lib")

const double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;

struct VirtualMemoryInfo {
    double maxSizeGB;
    double availableGB;
    double inUseGB;
};

// Cấu trúc lưu thông tin Card Mạng nâng cao
struct NetworkCardInfo {
    std::string description;
    std::vector<std::string> ipv4Addresses;
    std::vector<std::string> ipv6Addresses;
};

// --- HÀM TRỢ GIÚP ĐỊNH DẠNG ---
std::string formatDouble(double value, int precision = 2) {
    std::string text = std::to_string(value);
    size_t dotPosition = text.find('.');
    if (dotPosition != std::string::npos) {
        return text.substr(0, dotPosition + precision + 1) + " GB";
    }
    return text + " GB";
}

// --- CÁC HÀM TRUY VẤN THÔNG TIN HỆ THỐNG ---

std::string getRegistryValue(HKEY hKeyParent, const std::string& subKey, const std::string& valueName) {
    HKEY hKey;
    std::string result = "N/A";
    if (RegOpenKeyExA(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256] = { 0 };
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            result = buffer;
        }
        RegCloseKey(hKey);
    }
    return result;
}

std::string getOSVersion() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
}

std::string getProductID() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductId");
}

std::string getOriginalInstallDate() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD installTimeSeconds = 0;
        DWORD bufferSize = sizeof(installTimeSeconds);
        if (RegQueryValueExA(hKey, "InstallDate", NULL, NULL, (LPBYTE)&installTimeSeconds, &bufferSize) == ERROR_SUCCESS) {
            __time64_t time64 = installTimeSeconds;
            char timeBuffer[26];
            ctime_s(timeBuffer, sizeof(timeBuffer), &time64);

            std::string dateStr(timeBuffer);
            if (!dateStr.empty() && dateStr.back() == '\n') {
                dateStr.pop_back();
            }
            RegCloseKey(hKey);
            return dateStr;
        }
        RegCloseKey(hKey);
    }
    return "N/A";
}

std::string getSystemBootTime() {
    ULONGLONG bootTimeMS = GetTickCount64();
    __time64_t currentSystemTime;
    _time64(&currentSystemTime);

    __time64_t bootSystemTime = currentSystemTime - (bootTimeMS / 1000);
    char timeBuffer[26];
    ctime_s(timeBuffer, sizeof(timeBuffer), &bootSystemTime);

    std::string bootTimeStr(timeBuffer);
    if (!bootTimeStr.empty() && bootTimeStr.back() == '\n') {
        bootTimeStr.pop_back();
    }
    return bootTimeStr;
}

std::string getSystemManufacturer() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer");
}

std::string getSystemModel() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");
}

std::string getSystemType() {
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        return "x64-based PC";
    }
    else if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        return "x86-based PC";
    }
    else if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        return "ARM64-based PC";
    }
    return "Unknown System Type";
}

MEMORYSTATUSEX getMemoryStatus() {
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    GlobalMemoryStatusEx(&memoryStatus);
    return memoryStatus;
}

VirtualMemoryInfo calculateVirtualMemory(const MEMORYSTATUSEX& memoryStatus) {
    VirtualMemoryInfo info;
    info.maxSizeGB = memoryStatus.ullTotalPageFile / BYTES_TO_GB;
    info.availableGB = memoryStatus.ullAvailPageFile / BYTES_TO_GB;
    info.inUseGB = info.maxSizeGB - info.availableGB;
    return info;
}


std::vector<NetworkCardInfo> getActiveNetworkCards() {
    std::vector<NetworkCardInfo> cardList;

    // GAA_FLAG_DEFAULT = 0 để tương thích mọi phiên bản SDK
    ULONG flags = 0;
    ULONG bufferSize = 15000; // Bộ đệm lớn hẳn 15KB tránh tràn bộ nhớ

    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapterAddresses, &bufferSize);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapterAddresses, &bufferSize);
    }

    if (result == NO_ERROR) {
        while (adapterAddresses) {
            // Lọc: Chỉ lấy card mạng thực sự đang hoạt động (OperStatus == IfOperStatusUp)
            if (adapterAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK && adapterAddresses->OperStatus == IfOperStatusUp) {
                NetworkCardInfo card;

                // Lấy tên mô tả card mạng
                std::wstring wDescription(adapterAddresses->Description);
                card.description = std::string(wDescription.begin(), wDescription.end());

                // Duyệt tìm địa chỉ IP Unicast gắn với card này
                PIP_ADAPTER_UNICAST_ADDRESS unicast = adapterAddresses->FirstUnicastAddress;
                while (unicast) {
                    char ipBuffer[INET6_ADDRSTRLEN] = { 0 };

                    // Xác định cấu trúc họ địa chỉ (IPv4 hay IPv6)
                    auto family = unicast->Address.lpSockaddr->sa_family;

                    if (family == AF_INET) { // Nếu là IPv4
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
                        if (inet_ntop(AF_INET, &(sa_in->sin_addr), ipBuffer, sizeof(ipBuffer))) {
                            card.ipv4Addresses.push_back(std::string(ipBuffer));
                        }
                    }
                    else if (family == AF_INET6) { // Nếu là IPv6
                        sockaddr_in6* sa_in6 = reinterpret_cast<sockaddr_in6*>(unicast->Address.lpSockaddr);
                        if (inet_ntop(AF_INET6, &(sa_in6->sin6_addr), ipBuffer, sizeof(ipBuffer))) {
                            card.ipv6Addresses.push_back(std::string(ipBuffer));
                        }
                    }
                    unicast = unicast->Next;
                }

                cardList.push_back(card);
            }
            adapterAddresses = adapterAddresses->Next;
        }
    }
    return cardList;
}

// --- HÀM IN KẾT QUẢ ---
void printSystemInfo() {
    std::cout << "==================================================\n";
    std::cout << "               SYSTEM INFORMATION                 \n";
    std::cout << "==================================================\n";

    std::cout << std::left << std::setw(28) << "OS Version:" << getOSVersion() << "\n";
    std::cout << std::left << std::setw(28) << "Product ID:" << getProductID() << "\n";
    std::cout << std::left << std::setw(28) << "Original Install Date:" << getOriginalInstallDate() << "\n";
    std::cout << std::left << std::setw(28) << "System Boot Time:" << getSystemBootTime() << "\n";
    std::cout << std::left << std::setw(28) << "System Manufacturer:" << getSystemManufacturer() << "\n";
    std::cout << std::left << std::setw(28) << "System Model:" << getSystemModel() << "\n";
    std::cout << std::left << std::setw(28) << "System Type:" << getSystemType() << "\n";

    MEMORYSTATUSEX memoryStatus = getMemoryStatus();
    VirtualMemoryInfo virtualMemory = calculateVirtualMemory(memoryStatus);

    double totalPhysicalGB = memoryStatus.ullTotalPhys / BYTES_TO_GB;
    double availablePhysicalGB = memoryStatus.ullAvailPhys / BYTES_TO_GB;

    std::cout << std::left << std::setw(28) << "Total Physical Memory:" << formatDouble(totalPhysicalGB) << "\n";
    std::cout << std::left << std::setw(28) << "Available Physical Memory:" << formatDouble(availablePhysicalGB) << "\n";
    std::cout << std::left << std::setw(28) << "Virtual Memory: Max Size:" << formatDouble(virtualMemory.maxSizeGB) << "\n";
    std::cout << std::left << std::setw(28) << "Virtual Memory: Available:" << formatDouble(virtualMemory.availableGB) << "\n";
    std::cout << std::left << std::setw(28) << "Virtual Memory: In Use:" << formatDouble(virtualMemory.inUseGB) << "\n";

    std::cout << "Network Card(s):\n";
    std::vector<NetworkCardInfo> networkCards = getActiveNetworkCards();
    if (networkCards.empty()) {
        std::cout << "   [N/A or Disconnected]\n";
    }
    else {
        int index = 1;
        for (const auto& card : networkCards) {
            std::cout << "   [" << index++ << "]: " << card.description << "\n";

            // In danh sách IPv4
            if (!card.ipv4Addresses.empty()) {
                for (const auto& ip : card.ipv4Addresses) {
                    std::cout << "        -> IPv4 Address: " << ip << "\n";
                }
            }
            else {
                std::cout << "        -> IPv4 Address: N/A (No connection)\n";
            }

            // In danh sách IPv6
            if (!card.ipv6Addresses.empty()) {
                for (const auto& ip : card.ipv6Addresses) {
                    std::cout << "        -> IPv6 Address: " << ip << "\n";
                }
            }
        }
    }
    std::cout << "==================================================\n";
}

int main() {
    printSystemInfo();
    return 0;
}