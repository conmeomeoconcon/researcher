#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include <filesystem>
#include <sstream>
using namespace std;

// ============================================================================
// CẤU TRÚC DỮ LIỆU
// ============================================================================

struct PEFileContext {
    string filePath;
    ifstream file;
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS64 ntHeaders;
    vector<IMAGE_SECTION_HEADER> sections;
    bool isValid;
};

struct ImportInfo {
    string dllName;
    vector<string> functions;
};

// ============================================================================
// HÀM CHUYỂN ĐỔI - RVA TO OFFSET
// ============================================================================

DWORD RvaToOffset(
    DWORD rva,
    IMAGE_SECTION_HEADER* sections,
    WORD numberOfSections)
{
    for (WORD i = 0; i < numberOfSections; i++)
    {
        DWORD sectionVA = sections[i].VirtualAddress;
        DWORD sectionSize = sections[i].Misc.VirtualSize;

        if (rva >= sectionVA && rva < sectionVA + sectionSize)
        {
            return rva - sectionVA + sections[i].PointerToRawData;
        }
    }
    return rva;
}

// ============================================================================
// HÀM CHUYỂN ĐỔI - MACHINE TO STRING
// ============================================================================

const char* MachineToString(WORD machine)
{
    switch (machine)
    {
    case IMAGE_FILE_MACHINE_I386:   return "x86";
    case IMAGE_FILE_MACHINE_AMD64:  return "x64";
    case IMAGE_FILE_MACHINE_IA64:   return "Itanium";
    case IMAGE_FILE_MACHINE_ARM:    return "ARM";
    case IMAGE_FILE_MACHINE_ARM64:  return "ARM64";
    default:                        return "Unknown";
    }
}

// ============================================================================
// HÀM TIỆN ÍCH - LẤY TÊN SECTION
// ============================================================================

string GetSectionName(const IMAGE_SECTION_HEADER& section)
{
    char name[9] = { 0 };
    memcpy(name, section.Name, 8);
    string result(name);
    while (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }
    return result;
}

// ============================================================================
// HÀM TIỆN ÍCH - ĐỌC STRING TỪ RVA (KHÔNG LƯU VỊ TRÍ)
// ============================================================================

string ReadStringFromRVA_Seek(
    ifstream& file,
    DWORD rva,
    IMAGE_SECTION_HEADER* sections,
    WORD numberOfSections)
{
    DWORD offset = RvaToOffset(rva, sections, numberOfSections);

    file.seekg(offset);

    string result;
    getline(file, result, '\0');

    return result;
}

// ============================================================================
// HÀM IN - SEPARATOR
// ============================================================================

void PrintSeparator()
{
    cout << "============================================================" << endl;
}

void PrintHeader(const string& title)
{
    cout << "\n============================================================\n";
    cout << "  " << title;
    cout << "\n============================================================\n";
}

// ============================================================================
// HÀM MỞ/ĐÓNG FILE
// ============================================================================

bool OpenPEFile(PEFileContext& ctx, const char* filePath)
{
    ctx.filePath = filePath;
    ctx.file.open(filePath, ios::binary);

    if (!ctx.file)
    {
        cout << "Cannot open file\n";
        return false;
    }

    return true;
}

void ClosePEFile(PEFileContext& ctx)
{
    if (ctx.file.is_open())
    {
        ctx.file.close();
    }
}

// ============================================================================
// HÀM LẤY - ĐỌC DOS HEADER
// ============================================================================

bool ReadDOSHeader(PEFileContext& ctx)
{
    ctx.file.read(reinterpret_cast<char*>(&ctx.dosHeader), sizeof(ctx.dosHeader));

    if (ctx.dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        cout << "Invalid DOS Signature\n";
        return false;
    }

    return true;
}

// ============================================================================
// HÀM LẤY - ĐỌC NT HEADERS
// ============================================================================

bool ReadNTHeaders(PEFileContext& ctx)
{
    ctx.file.seekg(ctx.dosHeader.e_lfanew);
    ctx.file.read(reinterpret_cast<char*>(&ctx.ntHeaders), sizeof(ctx.ntHeaders));

    if (ctx.ntHeaders.Signature != IMAGE_NT_SIGNATURE)
    {
        cout << "Invalid NT Signature\n";
        return false;
    }

    return true;
}

// ============================================================================
// HÀM LẤY - ĐỌC SECTIONS
// ============================================================================

bool ReadSections(PEFileContext& ctx)
{
    WORD numberOfSections = ctx.ntHeaders.FileHeader.NumberOfSections;
    ctx.sections.resize(numberOfSections);

    DWORD sectionTableOffset =
        ctx.dosHeader.e_lfanew +
        sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER) +
        ctx.ntHeaders.FileHeader.SizeOfOptionalHeader;

    ctx.file.seekg(sectionTableOffset);

    for (WORD i = 0; i < numberOfSections; i++)
    {
        ctx.file.read(
            reinterpret_cast<char*>(&ctx.sections[i]),
            sizeof(IMAGE_SECTION_HEADER));
    }

    return true;
}

// ============================================================================
// HÀM LẤY - PARSE TOÀN BỘ PE
// ============================================================================

bool ParsePEFile(PEFileContext& ctx)
{
    if (!ReadDOSHeader(ctx))
    {
        return false;
    }

    if (!ReadNTHeaders(ctx))
    {
        return false;
    }

    if (!ReadSections(ctx))
    {
        return false;
    }

    ctx.isValid = true;
    return true;
}

// ============================================================================
// HÀM LẤY - LẤY DANH SÁCH HÀM CỦA 1 DLL (LƯU VÀO VECTOR)
// ============================================================================

void GetImportFunctions(
    ifstream& file,
    IMAGE_IMPORT_DESCRIPTOR& importDesc,
    IMAGE_SECTION_HEADER* sections,
    WORD numberOfSections,
    vector<string>& functions)
{
    ULONGLONG thunkRva = importDesc.OriginalFirstThunk
        ? importDesc.OriginalFirstThunk
        : importDesc.FirstThunk;

    DWORD thunkOffset = RvaToOffset(
        (DWORD)thunkRva,
        sections,
        numberOfSections);

    // Di chuyển đến vị trí thunk
    file.seekg(thunkOffset);

    while (true)
    {
        IMAGE_THUNK_DATA64 thunk;
        file.read(reinterpret_cast<char*>(&thunk), sizeof(thunk));

        if (thunk.u1.AddressOfData == 0)
            break;

        if (IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal))
        {
            stringstream ss;
            ss << "Ordinal: " << IMAGE_ORDINAL64(thunk.u1.Ordinal);
            functions.push_back(ss.str());
        }
        else
        {
            DWORD importByNameOffset = RvaToOffset(
                (DWORD)thunk.u1.AddressOfData,
                sections,
                numberOfSections);

            streampos thunkPos = file.tellg();
            file.seekg(importByNameOffset);

            IMAGE_IMPORT_BY_NAME ibn;
            file.read(reinterpret_cast<char*>(&ibn.Hint), sizeof(WORD));

            string functionName;
            getline(file, functionName, '\0');

            functions.push_back(functionName);

            file.seekg(thunkPos);
        }
    }
}

// ============================================================================
// HÀM LẤY - LẤY TẤT CẢ IMPORTS (LƯU VÀO VECTOR)
// ============================================================================

void GetAllImports(
    PEFileContext& ctx,
    vector<ImportInfo>& imports)
{
    IMAGE_DATA_DIRECTORY importDir =
        ctx.ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (importDir.VirtualAddress == 0)
        return;

    DWORD importOffset = RvaToOffset(
        importDir.VirtualAddress,
        ctx.sections.data(),
        ctx.ntHeaders.FileHeader.NumberOfSections);

    ctx.file.seekg(importOffset);

    while (true)
    {
        // Đọc import descriptor
        IMAGE_IMPORT_DESCRIPTOR importDesc;
        ctx.file.read(reinterpret_cast<char*>(&importDesc), sizeof(importDesc));

        if (importDesc.Name == 0)
            break;

        ImportInfo info;

        // LƯU VỊ TRÍ HIỆN TẠI (đầu descriptor)
        streampos descriptorPos = ctx.file.tellg();

        // Đọc tên DLL - hàm này sẽ di chuyển file pointer
        info.dllName = ReadStringFromRVA_Seek(
            ctx.file,
            importDesc.Name,
            ctx.sections.data(),
            ctx.ntHeaders.FileHeader.NumberOfSections);

        // QUAY LẠI VỊ TRÍ ĐẦU DESCRIPTOR
        ctx.file.seekg(descriptorPos);

        // Đọc các hàm import - hàm này sẽ di chuyển file pointer đến thunk
        GetImportFunctions(
            ctx.file,
            importDesc,
            ctx.sections.data(),
            ctx.ntHeaders.FileHeader.NumberOfSections,
            info.functions);

        imports.push_back(info);

        // QUAY LẠI VỊ TRÍ ĐẦU DESCRIPTOR để đọc descriptor tiếp theo
        ctx.file.seekg(descriptorPos);
    }
}

// ============================================================================
// HÀM IN - FILE INFO
// ============================================================================

void PrintFileInfo(const PEFileContext& ctx)
{
    cout << "\n========== TARGET FILE ==========\n";
    cout << "Full Path : " << ctx.filePath << endl;
    cout << "File Name : " << filesystem::path(ctx.filePath).filename().string() << endl;
}

// ============================================================================
// HÀM IN - DOS HEADER
// ============================================================================

void PrintDOSHeader(const PEFileContext& ctx)
{
    cout << "\n========== DOS HEADER ==========\n";
    cout << "e_magic  : 0x" << hex << ctx.dosHeader.e_magic << endl;
    cout << "e_lfanew : 0x" << hex << ctx.dosHeader.e_lfanew << endl;
}

// ============================================================================
// HÀM IN - NT HEADERS
// ============================================================================

void PrintNTHeaders(const PEFileContext& ctx)
{
    cout << "\n========== FILE HEADER ==========\n";
    cout << "Machine          : " << MachineToString(ctx.ntHeaders.FileHeader.Machine) << endl;
    cout << "Sections         : " << dec << ctx.ntHeaders.FileHeader.NumberOfSections << endl;
    cout << "TimeDateStamp    : 0x" << hex << ctx.ntHeaders.FileHeader.TimeDateStamp << endl;

    cout << "\n========== OPTIONAL HEADER ==========\n";
    cout << "ImageBase        : 0x" << hex << ctx.ntHeaders.OptionalHeader.ImageBase << endl;
    cout << "EntryPoint RVA   : 0x" << hex << ctx.ntHeaders.OptionalHeader.AddressOfEntryPoint << endl;
    cout << "SizeOfImage      : 0x" << hex << ctx.ntHeaders.OptionalHeader.SizeOfImage << endl;
    cout << "SizeOfHeaders    : 0x" << hex << ctx.ntHeaders.OptionalHeader.SizeOfHeaders << endl;
}

// ============================================================================
// HÀM IN - SECTIONS
// ============================================================================

void PrintSections(const PEFileContext& ctx)
{
    cout << "\n========== SECTIONS ==========\n";

    for (size_t i = 0; i < ctx.sections.size(); i++)
    {
        string name = GetSectionName(ctx.sections[i]);

        cout << "\n[" << dec << (i + 1) << "] " << name << endl;
        cout << "VirtualAddress : 0x" << hex << ctx.sections[i].VirtualAddress << endl;
        cout << "VirtualSize    : 0x" << hex << ctx.sections[i].Misc.VirtualSize << endl;
        cout << "RawDataPtr     : 0x" << hex << ctx.sections[i].PointerToRawData << endl;
        cout << "RawDataSize    : 0x" << hex << ctx.sections[i].SizeOfRawData << endl;
    }
}

// ============================================================================
// HÀM IN - IMPORTS (IN TỪ VECTOR ĐÃ LẤY)
// ============================================================================

void PrintImports(const vector<ImportInfo>& imports)
{
    cout << "\n========== IMPORTS ==========\n";

    if (imports.empty())
    {
        cout << "\nNo Import Table\n";
        return;
    }

    for (size_t i = 0; i < imports.size(); i++)
    {
        cout << "\nDLL: " << imports[i].dllName << endl;

        for (size_t j = 0; j < imports[i].functions.size(); j++)
        {
            cout << "    " << imports[i].functions[j] << endl;
        }
    }

    cout << "\n  Total DLLs imported: " << dec << imports.size() << endl;
}

// ============================================================================
// HÀM IN - TOÀN BỘ REPORT
// ============================================================================

void PrintReport(
    const PEFileContext& ctx,
    const vector<ImportInfo>& imports)
{
    PrintFileInfo(ctx);
    PrintDOSHeader(ctx);
    PrintNTHeaders(ctx);
    PrintSections(ctx);
    PrintImports(imports);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[])
{
    // Kiểm tra tham số
    if (argc != 2)
    {
        cout << "Usage: PEParser.exe <file.exe>\n";
        cout << "Example: PEParser.exe C:\\Windows\\System32\\notepad.exe\n";
        return 1;
    }

    // Khởi tạo context
    PEFileContext ctx;
    ctx.isValid = false;

    // BƯỚC 1: Mở file
    if (!OpenPEFile(ctx, argv[1]))
    {
        return 1;
    }

    // BƯỚC 2: Parse PE
    if (!ParsePEFile(ctx))
    {
        ClosePEFile(ctx);
        return 1;
    }

    // BƯỚC 3: Lấy dữ liệu imports
    vector<ImportInfo> imports;
    GetAllImports(ctx, imports);

    // BƯỚC 4: In report
    PrintReport(ctx, imports);

    // BƯỚC 5: Dọn dẹp
    ClosePEFile(ctx);

    return 0;
}