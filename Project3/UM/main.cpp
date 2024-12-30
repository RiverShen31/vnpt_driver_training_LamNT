#include <Windows.h>
#include <stdio.h>
#include <memory>
#include "..\ProcessMonitor\SysMonPublic.h"
#include <string>
#include <unordered_map>

bool bOutputToFile = false;

int Error(const char* text) {
	printf("%s (%u)\n", text, GetLastError());
	return 1;
}

std::string GetFormattedTime(const LARGE_INTEGER& time) {
	FILETIME local;
	FileTimeToLocalFileTime((FILETIME*)&time, &local);
	SYSTEMTIME st;
	FileTimeToSystemTime(&local, &st);
	char buffer[100];
	sprintf_s(buffer, "%02d:%02d:%02d %02d/%02d/%04d", st.wHour, st.wMinute, st.wSecond, st.wDay, st.wMonth, st.wYear);
	return std::string(buffer);
}

void LogEvent(FILE* logFile, const std::string& type, const std::string& message) {
	if (bOutputToFile) {
		fprintf(logFile, "[%s][%s] %s\n", GetFormattedTime({}).c_str(), type.c_str(), message.c_str());
	}
	else {
		printf("[%s][%s] %s\n", GetFormattedTime({}).c_str(), type.c_str(), message.c_str());
	}
}

void DisplayTime(const LARGE_INTEGER& time) {
	printf("%s: ", GetFormattedTime(time).c_str());
}

std::wstring GetDosNameFromNTName(PCWSTR path) {
	if (path[0] != L'\\')
		return path;

	static std::unordered_map<std::wstring, std::wstring> map;
	if (map.empty()) {
		auto drives = GetLogicalDrives();
		int c = 0;
		WCHAR root[] = L"X:";
		WCHAR target[128];
		while (drives) {
			if (drives & 1) {
				root[0] = 'A' + c;
				if (QueryDosDevice(root, target, _countof(target))) {
					map.insert({ target, root });
				}
			}
			drives >>= 1;
			c++;
		}
	}
	auto pos = wcschr(path + 1, L'\\');
	if (pos == nullptr)
		return path;

	pos = wcschr(pos + 1, L'\\');
	if (pos == nullptr)
		return path;

	std::wstring ntname(path, pos - path);
	if (auto it = map.find(ntname); it != map.end())
		return it->second + std::wstring(pos);

	return path;
}

std::string WStringToString(const std::wstring& wstr) {
	std::string str(wstr.begin(), wstr.end());
	return str;
}

void DisplayInfo(BYTE* buffer, DWORD size, FILE* logFile) {
	while (size > 0) {
		auto header = (ItemHeader*)buffer;
		auto timeStr = GetFormattedTime(header->Time);

		switch (header->Type) {
		case ItemType::ProcessExit:
		{
			auto info = (ProcessExitInfo*)buffer;
			LogEvent(logFile, "DELETE", "Process pid=" + std::to_string(info->ProcessId) + ". Command line: (N/A)");
			break;
		}

		case ItemType::ProcessCreate:
		{
			auto info = (ProcessCreateInfo*)buffer;
			std::wstring commandline(info->CommandLine, info->CommandLineLength);
			LogEvent(logFile, "CREATE", "Process pid=" + std::to_string(info->ProcessId) +
				" (of ppid=" + std::to_string(info->ParentProcessId) + "). Command line: " +
				WStringToString(commandline));
			break;
		}

		case ItemType::ThreadCreate:
		{
			auto info = (ThreadCreateInfo*)buffer;
			if (bOutputToFile) {
				fprintf(logFile, "%s: Thread %u Created in process %u\n", timeStr.c_str(), info->ThreadId, info->ProcessId);
			}
			else {
				DisplayTime(header->Time);
				printf("Thread %u Created in process %u\n", info->ThreadId, info->ProcessId);
			}
			break;
		}

		case ItemType::ThreadExit:
		{
			auto info = (ThreadExitInfo*)buffer;
			if (bOutputToFile) {
				fprintf(logFile, "%s: Thread %u Exited from process %u (Code: %u)\n", timeStr.c_str(), info->ThreadId, info->ProcessId, info->ExitCode);
			}
			else {
				DisplayTime(header->Time);
				printf("Thread %u Exited from process %u (Code: %u)\n", info->ThreadId, info->ProcessId, info->ExitCode);
			}
			break;
		}

		case ItemType::ImageLoad:
		{
			auto info = (ImageLoadInfo*)buffer;
			if (bOutputToFile) {
				fwprintf(logFile, L"%S: Image loaded into process %u at address 0x%llX (%ws)\n", timeStr.c_str(), info->ProcessId, info->LoadAddress, GetDosNameFromNTName(info->ImageFileName).c_str());
			}
			else {
				DisplayTime(header->Time);
				printf("Image loaded into process %u at address 0x%llX (%ws)\n", info->ProcessId, info->LoadAddress, GetDosNameFromNTName(info->ImageFileName).c_str());
			}
			break;
		}

		default:
			break;
		}
		buffer += header->Size;
		size -= header->Size;
	}
}

int SendKillCommand(HANDLE hDevice, DWORD pid) {
	DWORD bytesReturned;
	if (!DeviceIoControl(hDevice, IOCTL_TERMINATE_PROCESS, &pid, sizeof(pid), nullptr, 0, &bytesReturned, nullptr)) {
		printf("Failed to send kill command to PID %u. Error: %u\n", pid, GetLastError());
		return 1;
	}
	printf("Successfully sent kill command to PID %u.\n", pid);
}

int main(int argc, char* argv[]) {
	if (argc > 1 && strcmp(argv[1], "-f") == 0) {
		bOutputToFile = true;  // If argument is "-f", output to file
	}

	FILE* logFile = nullptr;
	if (bOutputToFile) {
		fopen_s(&logFile, "sysmon.txt", "a");
		if (!logFile) {
			return Error("Failed to open log file");
		}
	}

	auto hFile = CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return Error("Failed to open file");

	if (argc > 2 && strcmp(argv[1], "-k") == 0) {
		DWORD pid = atoi(argv[2]);
		return SendKillCommand(hFile, pid);
	}

	int size = 1 << 16;	// 64 KB
	auto buffer = std::make_unique<BYTE[]>(size);

	while (true) {
		DWORD bytes = 0;
		if (!ReadFile(hFile, buffer.get(), size, &bytes, nullptr))
			return Error("Failed to read");

		if (bytes)
			DisplayInfo(buffer.get(), bytes, logFile);

		Sleep(400);
	}
	CloseHandle(hFile);
	return 0;
}