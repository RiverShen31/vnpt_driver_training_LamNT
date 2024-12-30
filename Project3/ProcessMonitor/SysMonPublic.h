#pragma once
#pragma once

#define IOCTL_TERMINATE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BLOCK_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)

typedef struct _TERMINATE_PROCESS_REQUEST {
	ULONG ProcessId; // PID to terminate
} TERMINATE_PROCESS_REQUEST, *PTERMINATE_PROCESS_REQUEST;



enum class ItemType : short {
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad
};

struct ItemHeader {
	ItemType Type;
	USHORT Size;
	LARGE_INTEGER Time;
};

struct ProcessExitInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ExitCode;
};

struct ProcessCreateInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ParentProcessId;
	ULONG CreatingThreadId;
	ULONG CreatingProcessId;
	USHORT CommandLineLength;
	WCHAR CommandLine[1];
};

struct ThreadCreateInfo : ItemHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};

struct ThreadExitInfo : ThreadCreateInfo {
	ULONG ExitCode;
};

const int MaxImageFileSize = 300;

struct ImageLoadInfo : ItemHeader {
	ULONG ProcessId;
	ULONG ImageSize;
	ULONG64 LoadAddress;
	WCHAR ImageFileName[MaxImageFileSize + 1];
};
