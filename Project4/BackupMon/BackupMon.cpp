#include <Windows.h>
#include <fltUser.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <iostream>

enum ChanGhi {
    denyWrite,
    allowWrite
};


enum ChanDelete {
    denyDelete,
    allowDelete
};

enum ACTION {
    Connect,
    Disconnect,
    Read,
    Write,
    Create,
    Open,
    Delete,
    DenyCreate,
    DenyWrite,
    DenyDelete,
};

struct FileAccessPortMessage {
    ACTION Action;
    LARGE_INTEGER Time;
    USHORT FileNameLength;
    WCHAR FileName[260];
};

#pragma comment(lib, "fltlib")

// Hàm ??nh d?ng th?i gian
std::string GetFormattedTime(const LARGE_INTEGER& time) {
    FILETIME local;
    FileTimeToLocalFileTime((FILETIME*)&time, &local);
    SYSTEMTIME st;
    FileTimeToSystemTime(&local, &st);
    char buffer[100];
    sprintf_s(buffer, "%02d:%02d:%02d %02d/%02d/%04d", st.wHour, st.wMinute, st.wSecond, st.wDay, st.wMonth, st.wYear);
    return std::string(buffer);
}

// Hàm l?y tên hành ??ng t? enum ACTION
std::string GetActionName(ACTION action) {
    switch (action) {
    case ACTION::Connect: return "CONNECT";
    case ACTION::Disconnect: return "DISCONNECT";
    case ACTION::Read: return "READ";
    case ACTION::Write: return "WRITE";
    case ACTION::Create: return "CREATE";
    case ACTION::Open: return "OPEN";
    case ACTION::Delete: return "DELETE";
    case ACTION::DenyWrite: return "DenyWrite";
    case ACTION::DenyDelete: return "DenyDelete";
    case ACTION::DenyCreate: return "DenyCreate";
    default: return "UNKNOWN";
    }
}

// Hàm chuy?n ??i NT Name sang DOS Name
std::wstring GetDosNameFromNTName(PCWSTR path) {
    if (path[0] != L'\\')
        return path;

    static std::unordered_map<std::wstring, std::wstring> map;
    if (map.empty()) {
        DWORD drives = GetLogicalDrives();
        WCHAR root[] = L"A:";
        WCHAR target[512];
        for (int i = 0; drives; ++i, drives >>= 1) {
            if (drives & 1) {
                root[0] = L'A' + i;
                if (QueryDosDeviceW(root, target, _countof(target))) {
                    map[target] = root;
                }
            }
        }
    }

    // Tìm ph?n volume t? NT path
    const WCHAR* restPath = wcschr(path + 1, L'\\');
    if (!restPath)
        return path;

    std::wstring ntVolume(path, restPath - path);
    auto it = map.find(ntVolume);
    if (it != map.end()) {
        return it->second + std::wstring(restPath);
    }

    return path;
}

// Hàm x? lý tin nh?n
void HandleMessage(const BYTE* buffer) {
    auto msg = (FileAccessPortMessage*)buffer;
    std::string timeStr = GetFormattedTime(msg->Time);
    std::string actionName = GetActionName(msg->Action);
    std::wstring filename(msg->FileName, msg->FileNameLength); // Chuy?n ??i t? byte sang wchar_t

    // In thông tin hành ??ng
    printf("[%s][%s][C:] %ws\n", timeStr.c_str(), actionName.c_str(), GetDosNameFromNTName(msg->FileName).c_str());
}

std::wstring SimplifyNTPath(const std::wstring& ntPath) {
    const std::wstring devicePrefix = L"\\Device";
    if (ntPath.rfind(devicePrefix, 0) == 0) { // Ki?m tra n?u ???ng d?n b?t ??u b?ng "\Device"
        return L"\\" + ntPath.substr(devicePrefix.length() + 1); // Lo?i b? "\Device" và thêm "\"
    }
    return ntPath; // Tr? l?i nguyên b?n n?u không ph?i "\Device"
}

std::wstring ConvertToVolumePath(const std::wstring& dosPath) {
    // Extract the drive letter from the DOS path
    WCHAR driveLetter[3] = { dosPath[0], dosPath[1], L'\0' }; // e.g., "C:"

    // Query the volume path for the drive letter
    WCHAR volumePath[MAX_PATH] = { 0 };
    if (!QueryDosDeviceW(driveLetter, volumePath, _countof(volumePath))) {
        std::wcerr << L"Failed to query volume path for drive: " << driveLetter << L" (Error: " << GetLastError() << L")\n";
        return L"";
    }

    // Replace the drive letter with the volume path
    std::wstring ntPath = dosPath;
    ntPath.replace(0, 2, volumePath);

    return SimplifyNTPath(ntPath);
}

// Entry point
int main() {
    HANDLE hPort;

    // K?t n?i ??n communication port
    auto hr = FilterConnectCommunicationPort(L"\\BackupPort", 0, nullptr, 0, nullptr, &hPort);
    if (FAILED(hr)) {
        printf("Error connecting to port (HR=0x%08X)\n", hr);
        Sleep(5000);
        return 1;
    }

    // ???ng d?n ng??i dùng cung c?p
    //std::wstring monitorFolder = L"C:\\Users\\tienl\\Desktop"; 

    std::wcout << L"Enter the folder path to monitor: ";
    std::wstring monitorFolder;
    std::getline(std::wcin, monitorFolder);

    if (monitorFolder.empty()) {
        std::wcerr << L"Invalid input. Folder path cannot be empty.\n";
        return 1;
    }

    // Convert to NT path
    std::wstring ntPath = ConvertToVolumePath(monitorFolder);

    if (!ntPath.empty()) {
        std::wcout << L"Converted NT Path: " << ntPath << L"\n";
    }

    // Hiển thị danh sách action cho người dùng chọn
    std::cout << "Select actions:\n";
    std::cout << "1. Deny Write\n";
    std::cout << "2. Allow Write\n";
    std::cout << "3. Deny Delete\n";
    std::cout << "4. Allow Delete\n";

    int actionWrite, actionDelete;
    std::cout << "Enter choice for Write Action (1 or 2): ";
    std::cin >> actionWrite;
    std::cout << "Enter choice for Delete Action (3 or 4): ";
    std::cin >> actionDelete;

    // Xác định action
    ChanGhi selectedWriteAction = (actionWrite == 1) ? denyWrite : allowWrite;
    ChanDelete selectedDeleteAction = (actionDelete == 3) ? denyDelete : allowDelete;

    struct {
        USHORT Length;
        WCHAR Buffer[260];
        ChanGhi ActionGhi;
        ChanDelete ActionDelete;
    } messageAction;

    messageAction.Length = (USHORT)(ntPath.length() * sizeof(WCHAR));
    memcpy(messageAction.Buffer, ntPath.c_str(), messageAction.Length);
    messageAction.Buffer[messageAction.Length / sizeof(WCHAR)] = L'\0'; // Null terminate
    printf("TEST");
    messageAction.ActionDelete = selectedDeleteAction;
    messageAction.ActionGhi = selectedWriteAction;

    DWORD testBytesRetuned = 0;
    hr = FilterSendMessage(hPort, &messageAction, sizeof(messageAction), nullptr, 0, &testBytesRetuned);
    if (FAILED(hr)) {
        printf("Error sending message (HR=0x%08X)\n", hr);
    }
    else {
        printf("Message sent successfully! Bytes returned: %lu\n", testBytesRetuned);
    }

    // B? ??m ?? nh?n thông ?i?p
    BYTE buffer[4096]; // 4 KB buffer
    auto message = (FILTER_MESSAGE_HEADER*)buffer;

    printf("Test\n");

    // Vòng l?p nh?n và x? lý tin nh?n
    while (true) {

        hr = FilterGetMessage(hPort, message, sizeof(buffer), nullptr);
        if (FAILED(hr)) {
            printf("Error receiving message (HR=0x%08X)\n", hr);
            break;
        }

        HandleMessage(buffer + sizeof(FILTER_MESSAGE_HEADER));
    }

    CloseHandle(hPort);
    return 0;
}
