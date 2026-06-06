#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include <filesystem>
using namespace std;

DWORD RvaToOffset(
    DWORD rva,
    IMAGE_SECTION_HEADER* sections,
    WORD numberOfSections)
{
    for (WORD i = 0; i < numberOfSections; i++)
    {
        DWORD sectionVA =
            sections[i].VirtualAddress;

        DWORD sectionSize =
            sections[i].Misc.VirtualSize;

        if (rva >= sectionVA &&
            rva < sectionVA + sectionSize)
        {
            return rva
                - sectionVA
                + sections[i].PointerToRawData;
        }
    }

    return rva;
}

const char* MachineToString(WORD machine)
{
    switch (machine)
    {
    case IMAGE_FILE_MACHINE_I386:
        return "x86";

    case IMAGE_FILE_MACHINE_AMD64:
        return "x64";

    default:
        return "Unknown";
    }
}

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

    cout << "\n========== TARGET FILE ==========\n";

    cout << "Full Path : "
        << argv[1]
        << endl;

    cout << "File Name : "
        << filesystem::path(argv[1]).filename().string()
        << endl;

    // =========================
    // DOS HEADER
    // =========================

    IMAGE_DOS_HEADER dosHeader;

    file.read(
        reinterpret_cast<char*>(&dosHeader),
        sizeof(dosHeader));

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
    {
        cout << "Invalid DOS Signature\n";
        return 1;
    }

    cout << "\n========== DOS HEADER ==========\n";

    cout << "e_magic  : 0x"
        << hex << dosHeader.e_magic
        << endl;

    cout << "e_lfanew : 0x"
        << hex << dosHeader.e_lfanew
        << endl;

    // =========================
    // NT HEADERS
    // =========================

    file.seekg(dosHeader.e_lfanew);

    IMAGE_NT_HEADERS64 ntHeaders;

    file.read(
        reinterpret_cast<char*>(&ntHeaders),
        sizeof(ntHeaders));

    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
    {
        cout << "Invalid NT Signature\n";
        return 1;
    }

    cout << "\n========== FILE HEADER ==========\n";

    cout << "Machine          : "
        << MachineToString(
            ntHeaders.FileHeader.Machine)
        << endl;

    cout << "Sections         : "
        << dec
        << ntHeaders.FileHeader.NumberOfSections
        << endl;

    cout << "TimeDateStamp    : 0x"
        << hex
        << ntHeaders.FileHeader.TimeDateStamp
        << endl;

    cout << "\n========== OPTIONAL HEADER ==========\n";

    cout << "ImageBase        : 0x"
        << hex
        << ntHeaders.OptionalHeader.ImageBase
        << endl;

    cout << "EntryPoint RVA   : 0x"
        << hex
        << ntHeaders.OptionalHeader.AddressOfEntryPoint
        << endl;

    cout << "SizeOfImage      : 0x"
        << hex
        << ntHeaders.OptionalHeader.SizeOfImage
        << endl;

    cout << "SizeOfHeaders    : 0x"
        << hex
        << ntHeaders.OptionalHeader.SizeOfHeaders
        << endl;

    // =========================
    // SECTION TABLE
    // =========================

    vector<IMAGE_SECTION_HEADER> sections(
        ntHeaders.FileHeader.NumberOfSections);

    file.seekg(
        dosHeader.e_lfanew +
        sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER) +
        ntHeaders.FileHeader.SizeOfOptionalHeader);

    cout << "\n========== SECTIONS ==========\n";

    for (WORD i = 0;
        i < ntHeaders.FileHeader.NumberOfSections;
        i++)
    {
        file.read(
            reinterpret_cast<char*>(&sections[i]),
            sizeof(IMAGE_SECTION_HEADER));

        char sectionName[9] = { 0 };

        memcpy(
            sectionName,
            sections[i].Name,
            8);

        cout << "\n[" << i + 1 << "] "
            << sectionName
            << endl;

        cout << "VirtualAddress : 0x"
            << hex
            << sections[i].VirtualAddress
            << endl;

        cout << "VirtualSize    : 0x"
            << hex
            << sections[i].Misc.VirtualSize
            << endl;

        cout << "RawDataPtr     : 0x"
            << hex
            << sections[i].PointerToRawData
            << endl;

        cout << "RawDataSize    : 0x"
            << hex
            << sections[i].SizeOfRawData
            << endl;
    }

    // =========================
    // IMPORT DIRECTORY
    // =========================

    IMAGE_DATA_DIRECTORY importDir =
        ntHeaders.OptionalHeader.DataDirectory[
            IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (importDir.VirtualAddress == 0)
    {
        cout << "\nNo Import Table\n";
        return 0;
    }

    DWORD importOffset =
        RvaToOffset(
            importDir.VirtualAddress,
            sections.data(),
            ntHeaders.FileHeader.NumberOfSections);

    file.seekg(importOffset);

    cout << "\n========== IMPORTS ==========\n";

    while (true)
    {
        IMAGE_IMPORT_DESCRIPTOR importDesc;

        file.read(
            reinterpret_cast<char*>(&importDesc),
            sizeof(importDesc));

        if (importDesc.Name == 0)
            break;

        DWORD dllNameOffset =
            RvaToOffset(
                importDesc.Name,
                sections.data(),
                ntHeaders.FileHeader.NumberOfSections);

        streampos currentPos =
            file.tellg();

        file.seekg(dllNameOffset);

        string dllName;
        getline(file, dllName, '\0');

        cout << "\nDLL: "
            << dllName
            << endl;

        ULONGLONG thunkRva =
            importDesc.OriginalFirstThunk
            ? importDesc.OriginalFirstThunk
            : importDesc.FirstThunk;

        DWORD thunkOffset =
            RvaToOffset(
                (DWORD)thunkRva,
                sections.data(),
                ntHeaders.FileHeader.NumberOfSections);

        file.seekg(thunkOffset);

        while (true)
        {
            IMAGE_THUNK_DATA64 thunk;

            file.read(
                reinterpret_cast<char*>(&thunk),
                sizeof(thunk));

            if (thunk.u1.AddressOfData == 0)
                break;

            if (IMAGE_SNAP_BY_ORDINAL64(
                thunk.u1.Ordinal))
            {
                cout << "    Ordinal: "
                    << IMAGE_ORDINAL64(
                        thunk.u1.Ordinal)
                    << endl;
            }
            else
            {
                DWORD importByNameOffset =
                    RvaToOffset(
                        (DWORD)thunk.u1.AddressOfData,
                        sections.data(),
                        ntHeaders.FileHeader.NumberOfSections);

                streampos thunkPos =
                    file.tellg();

                file.seekg(importByNameOffset);

                IMAGE_IMPORT_BY_NAME ibn;

                file.read(
                    reinterpret_cast<char*>(&ibn.Hint),
                    sizeof(WORD));

                string functionName;
                getline(file, functionName, '\0');

                cout << "    "
                    << functionName
                    << endl;

                file.seekg(thunkPos);
            }
        }

        file.seekg(currentPos);
    }

    file.close();

    return 0;
}