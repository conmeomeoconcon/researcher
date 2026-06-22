#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include <filesystem>
using namespace std;

// ============================================================================
// HÀM CHUYỂN ĐỔI RVA -> OFFSET
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
// HÀM CHUYỂN ĐỔI MACHINE TYPE
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
// HÀM LẤY TÊN SECTION
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
// MAIN
// ============================================================================

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cout << "Usage: PEParser.exe <file.exe>\n";
        return 1;
    }

    ifstream file(argv[1], ios::binary);

    if (!file)
    {
        cout << "Cannot open file\n";
        return 1;
    }

    // =========================
    // TARGET FILE
    // =========================

    cout << "\n============================================================\n";
    cout << "  TARGET FILE\n";
    cout << "============================================================\n";
    cout << "Full Path : " << argv[1] << endl;
    cout << "File Name : " << filesystem::path(argv[1]).filename().string() << endl;
    cout << "============================================================\n";

    // =========================
    // DOS HEADER
    // =========================

    IMAGE_DOS_HEADER dosHeader;
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        cout << "Invalid DOS Signature\n";
        file.close();
        return 1;
    }

    cout << "\n============================================================\n";
    cout << "  DOS HEADER\n";
    cout << "============================================================\n";
    cout << "e_magic  : 0x" << hex << setw(4) << setfill('0') << dosHeader.e_magic << endl;
    cout << "e_lfanew : 0x" << hex << dosHeader.e_lfanew << endl;
    cout << "============================================================\n";

    // =========================
    // NT HEADERS
    // =========================

    file.seekg(dosHeader.e_lfanew);

    IMAGE_NT_HEADERS64 ntHeaders;
    file.read(reinterpret_cast<char*>(&ntHeaders), sizeof(ntHeaders));

    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
    {
        cout << "Invalid NT Signature\n";
        file.close();
        return 1;
    }

    cout << "\n============================================================\n";
    cout << "  FILE HEADER\n";
    cout << "============================================================\n";
    cout << "Machine          : " << MachineToString(ntHeaders.FileHeader.Machine) << endl;
    cout << "Sections         : " << dec << ntHeaders.FileHeader.NumberOfSections << endl;
    cout << "TimeDateStamp    : 0x" << hex << ntHeaders.FileHeader.TimeDateStamp << endl;
    cout << "============================================================\n";

    cout << "\n============================================================\n";
    cout << "  OPTIONAL HEADER\n";
    cout << "============================================================\n";
    cout << "ImageBase        : 0x" << hex << ntHeaders.OptionalHeader.ImageBase << endl;
    cout << "EntryPoint RVA   : 0x" << hex << ntHeaders.OptionalHeader.AddressOfEntryPoint << endl;
    cout << "SizeOfImage      : 0x" << hex << ntHeaders.OptionalHeader.SizeOfImage << endl;
    cout << "SizeOfHeaders    : 0x" << hex << ntHeaders.OptionalHeader.SizeOfHeaders << endl;
    cout << "============================================================\n";

    // =========================
    // SECTION TABLE
    // =========================

    vector<IMAGE_SECTION_HEADER> sections(ntHeaders.FileHeader.NumberOfSections);

    file.seekg(
        dosHeader.e_lfanew +
        sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER) +
        ntHeaders.FileHeader.SizeOfOptionalHeader);

    cout << "\n============================================================\n";
    cout << "  SECTIONS\n";
    cout << "============================================================\n";

    for (WORD i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++)
    {
        file.read(reinterpret_cast<char*>(&sections[i]), sizeof(IMAGE_SECTION_HEADER));

        string sectionName = GetSectionName(sections[i]);

        cout << "\n[" << dec << (i + 1) << "] " << sectionName << endl;
        cout << "  VirtualAddress : 0x" << hex << sections[i].VirtualAddress << endl;
        cout << "  VirtualSize    : 0x" << hex << sections[i].Misc.VirtualSize << endl;
        cout << "  RawDataPtr     : 0x" << hex << sections[i].PointerToRawData << endl;
        cout << "  RawDataSize    : 0x" << hex << sections[i].SizeOfRawData << endl;
    }
    cout << "============================================================\n";

    // =========================
    // IMPORT DIRECTORY
    // =========================

    IMAGE_DATA_DIRECTORY importDir =
        ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (importDir.VirtualAddress == 0)
    {
        cout << "\n  No Import Table found.\n";
        file.close();
        return 0;
    }

    DWORD importOffset = RvaToOffset(
        importDir.VirtualAddress,
        sections.data(),
        ntHeaders.FileHeader.NumberOfSections);

    file.seekg(importOffset);

    cout << "\n============================================================\n";
    cout << "  IMPORTS\n";
    cout << "============================================================\n";

    int dllCount = 0;
    while (true)
    {
        IMAGE_IMPORT_DESCRIPTOR importDesc;
        file.read(reinterpret_cast<char*>(&importDesc), sizeof(importDesc));

        if (importDesc.Name == 0)
            break;

        dllCount++;

        DWORD dllNameOffset = RvaToOffset(
            importDesc.Name,
            sections.data(),
            ntHeaders.FileHeader.NumberOfSections);

        streampos currentPos = file.tellg();
        file.seekg(dllNameOffset);

        string dllName;
        getline(file, dllName, '\0');

        cout << "\n  DLL [" << dec << dllCount << "]: " << dllName << endl;
        cout << "  -----------------------------------------\n";

        ULONGLONG thunkRva = importDesc.OriginalFirstThunk
            ? importDesc.OriginalFirstThunk
            : importDesc.FirstThunk;

        DWORD thunkOffset = RvaToOffset(
            (DWORD)thunkRva,
            sections.data(),
            ntHeaders.FileHeader.NumberOfSections);

        file.seekg(thunkOffset);

        int funcCount = 0;
        while (true)
        {
            IMAGE_THUNK_DATA64 thunk;
            file.read(reinterpret_cast<char*>(&thunk), sizeof(thunk));

            if (thunk.u1.AddressOfData == 0)
                break;

            funcCount++;

            if (IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal))
            {
                cout << "    " << setw(3) << funcCount << ". Ordinal: "
                    << dec << IMAGE_ORDINAL64(thunk.u1.Ordinal) << endl;
            }
            else
            {
                DWORD importByNameOffset = RvaToOffset(
                    (DWORD)thunk.u1.AddressOfData,
                    sections.data(),
                    ntHeaders.FileHeader.NumberOfSections);

                streampos thunkPos = file.tellg();
                file.seekg(importByNameOffset);

                IMAGE_IMPORT_BY_NAME ibn;
                file.read(reinterpret_cast<char*>(&ibn.Hint), sizeof(WORD));

                string functionName;
                getline(file, functionName, '\0');

                cout << "    " << setw(3) << funcCount << ". " << functionName << endl;

                file.seekg(thunkPos);
            }
        }

        cout << "    Total functions: " << dec << funcCount << endl;

        file.seekg(currentPos);
    }

    cout << "\n  Total DLLs imported: " << dec << dllCount << endl;
    cout << "============================================================\n";

    file.close();

    return 0;
}