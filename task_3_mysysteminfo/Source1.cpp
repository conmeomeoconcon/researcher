#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <iomanip>
#include <iphlpapi.h>

// Ép buộc Visual Studio tự động liên kết thư viện hệ thống để tránh lỗi Linker (LNK2019)
#pragma comment(lib, "IPHLPAPI.lib")

const double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;

// Cấu trúc lưu trữ dữ liệu bộ nhớ ảo theo nguyên tắc Clean Code (Gom cụm dữ liệu)
struct VirtualMemoryInfo {
    double maxSizeGB;
    double availableGB;
    double inUseGB;
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

// 1. Lấy thông tin từ Windows Registry (Sử dụng bảng mã ANSI chuẩn để không bị lỗi Build)
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

// 2. Lấy ngày cài đặt Windows ban đầu
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

// 3. Lấy thời gian khởi động máy (System Boot Time)
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

// 4. Lấy nhà sản xuất và Model máy
std::string getSystemManufacturer() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer");
}

std::string getSystemModel() {
    return getRegistryValue(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");
}

// 5. Lấy kiến trúc hệ thống (Kiểu OS x64 hay x86)
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

// 6. Truy vấn thông tin bộ nhớ vật lý và bộ nhớ ảo
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

// 7. Lấy danh sách Card mạng đang hoạt động
std::vector<std::string> getActiveNetworkCards() {
    std::vector<std::string> cardNames;
    ULONG bufferSize = sizeof(IP_ADAPTER_ADDRESSES);
    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    ULONG result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses, &bufferSize);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses, &bufferSize);
    }

    if (result == NO_ERROR) {
        while (adapterAddresses) {
            if (adapterAddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK && adapterAddresses->OperStatus == IfOperStatusUp) {
                // Chuyển đổi tên card từ Wide String sang Regular String an toàn
                std::wstring wDescription(adapterAddresses->Description);
                std::string sDescription(wDescription.begin(), wDescription.end());
                cardNames.push_back(sDescription);
            }
            adapterAddresses = adapterAddresses->Next;
        }
    }
    return cardNames;
}

// --- HÀM IN KẾT QUẢ CHUẨN ĐỒNG BỘ ---
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
    std::vector<std::string> networkCards = getActiveNetworkCards();
    if (networkCards.empty()) {
        std::cout << "   [N/A or Disconnected]\n";
    }
    else {
        int index = 1;
        for (const auto& card : networkCards) {
            std::cout << "   [" << index++ << "]: " << card << "\n";
        }
    }
    std::cout << "==================================================\n";
}

int main() {
    printSystemInfo();
    return 0;
}