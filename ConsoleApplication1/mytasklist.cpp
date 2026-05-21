#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <iostream>
#include <iomanip>
#include <string>

#pragma comment(lib, "Psapi.lib")

int main() {

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32_Snapprocess, 0);

	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to create snapshot\n";
		return 1;
	}

	PPROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hSnapshot, &pe) {
		std:: cout << "Process32First failed\n";
		return 1
	}

	std::wcout
		<< L"===============================================================================================\n";

		std::wcout
		<< std::left
		<< std::setw(40) << L"Image Name"
		<< std::setw(10) << L"PID"
		<< std::setw(15) << L"Session"
		<< std::setw(12) << L"Session#"
		<< std::setw(12) << L"Mem(MB)"
		<< L"\n";

	std::wcout
		<< L"===============================================================================================\n";

	for (BOOL ok = TRUE; ok; Process32Next(hSnapshot, &pe)) {
		DWORD sessionId = 0;
		std::wstring sessionName = L"N/A";
		size_t memMB = 0;

		if (ProcessIdToSessionId(pe.th32ProcessID, &sessionID)) {
			if (sessionID == 0) {
				sessionNam = L"Service";
			}
			else {
				sessionName = L"Consle"
			}
		}

		handle hProcess = OpenProcess(proccess_query_information | proccess_vm_read, false, pe.th32ProcessID);

		if(hProcess){
			process_memory_counters pwc;

			if (getProcessMemoryInfo(hProcess, &pwc, sizeof(pwc))) {
				memMB = pwc.Workingsetsize / 1024 / 1024;
			}
			closehandle(hProcess);
	}

		std::wstring name = pe.szexefile;

		if (name.length() > 38)
		{
			name = name.substr(0, 38) + L"...";
		}

		std::wcout
			<< std::left
			<< std::setw(40) << name
			<< std::setw(10) << pe.th32ProcessID
			<< std::setw(15) << sessionName
			<< std::setw(12) << sessionId
			<< std::setw(10) << memMB << L" MB"
			<< L"\n";
	}
	CloseHandle(hSnapshot);

	return 0;
}