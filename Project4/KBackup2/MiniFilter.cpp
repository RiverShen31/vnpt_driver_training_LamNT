#include "pch.h"
#include "Driver.h"
#include <ktl.h>
#include <Locker.h>

struct FileContext {
	Mutex Lock;
	LARGE_INTEGER BackupTime;
	BOOLEAN Written;
};

NTSTATUS BackupUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS BackupInstanceSetup(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_SETUP_FLAGS Flags,
	_In_ DEVICE_TYPE VolumeDeviceType,
	_In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType);
NTSTATUS BackupInstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);
VOID BackupInstanceTeardownStart(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags);
VOID BackupInstanceTeardownComplete(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags);
FLT_POSTOP_CALLBACK_STATUS OnPostCreate(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, PVOID, FLT_POST_OPERATION_FLAGS flags);
FLT_PREOP_CALLBACK_STATUS OnPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*);
FLT_POSTOP_CALLBACK_STATUS OnPostCleanup(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, PVOID, FLT_POST_OPERATION_FLAGS flags);
bool ShouldBackupFile(FilterFileNameInformation& nameInfo);
NTSTATUS BackupFile(PUNICODE_STRING path, PCFLT_RELATED_OBJECTS FltObjects);
//NTSTATUS BackupFileWithSection(PUNICODE_STRING path, PCFLT_RELATED_OBJECTS FltObjects);


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


FLT_PREOP_CALLBACK_STATUS OnPreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);
FLT_PREOP_CALLBACK_STATUS OnPreRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);
FLT_PREOP_CALLBACK_STATUS OnPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);
FLT_PREOP_CALLBACK_STATUS OnPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext);

UNICODE_STRING g_MonitorFolder = RTL_CONSTANT_STRING(L"");
ChanGhi g_ChanGhi = ChanGhi::allowWrite;
ChanDelete g_ChanDelete = ChanDelete::allowDelete;

NTSTATUS InitMiniFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	HANDLE hKey = nullptr, hSubKey = nullptr;
	NTSTATUS status;
	do {
		//
		// add registry data for proper mini-filter registration
		//
		OBJECT_ATTRIBUTES keyAttr = RTL_CONSTANT_OBJECT_ATTRIBUTES(RegistryPath, OBJ_KERNEL_HANDLE);
		status = ZwOpenKey(&hKey, KEY_WRITE, &keyAttr);
		if (!NT_SUCCESS(status))
			break;

		UNICODE_STRING subKey = RTL_CONSTANT_STRING(L"Instances");
		OBJECT_ATTRIBUTES subKeyAttr;
		InitializeObjectAttributes(&subKeyAttr, &subKey, OBJ_KERNEL_HANDLE, hKey, nullptr);
		status = ZwCreateKey(&hSubKey, KEY_WRITE, &subKeyAttr, 0, nullptr, 0, nullptr);
		if (!NT_SUCCESS(status))
			break;

		//
		// set "DefaultInstance" value
		//
		UNICODE_STRING valueName = RTL_CONSTANT_STRING(L"DefaultInstance");
		WCHAR name[] = L"BackupDefaultInstance";
		status = ZwSetValueKey(hSubKey, &valueName, 0, REG_SZ, name, sizeof(name));
		if (!NT_SUCCESS(status))
			break;

		//
		// create "instance" key under "Instances"
		//
		UNICODE_STRING instKeyName;
		RtlInitUnicodeString(&instKeyName, name);
		HANDLE hInstKey;
		InitializeObjectAttributes(&subKeyAttr, &instKeyName, OBJ_KERNEL_HANDLE, hSubKey, nullptr);
		status = ZwCreateKey(&hInstKey, KEY_WRITE, &subKeyAttr, 0, nullptr, 0, nullptr);
		if (!NT_SUCCESS(status))
			break;

		//
		// write out altitude
		//
		WCHAR altitude[] = L"335342";
		UNICODE_STRING altitudeName = RTL_CONSTANT_STRING(L"Altitude");
		status = ZwSetValueKey(hInstKey, &altitudeName, 0, REG_SZ, altitude, sizeof(altitude));
		if (!NT_SUCCESS(status))
			break;

		//
		// write out flags
		//
		UNICODE_STRING flagsName = RTL_CONSTANT_STRING(L"Flags");
		ULONG flags = 0;
		status = ZwSetValueKey(hInstKey, &flagsName, 0, REG_DWORD, &flags, sizeof(flags));
		if (!NT_SUCCESS(status))
			break;

		ZwClose(hInstKey);

		if (hSubKey) {
			if (!NT_SUCCESS(status))
				ZwDeleteKey(hSubKey);
			ZwClose(hSubKey);
		}
		if (hKey)
			ZwClose(hKey);

		FLT_OPERATION_REGISTRATION const callbacks[] = {
			{ IRP_MJ_CREATE, 0, OnPreCreate, nullptr},
			{ IRP_MJ_READ, 0, OnPreRead, nullptr },
			{ IRP_MJ_WRITE, 0, OnPreWrite, nullptr },
			{ IRP_MJ_SET_INFORMATION, 0, OnPreSetInformation, nullptr},
			//{ IRP_MJ_CLEANUP, 0, OnPreDelete, nullptr },
			//{ IRP_MJ_CREATE, 0, nullptr, OnPostCreate },
			//{ IRP_MJ_WRITE, 0, OnPreWrite },
			//{ IRP_MJ_CLEANUP, 0, nullptr, OnPostCleanup },
			{ IRP_MJ_OPERATION_END }
		};

		const FLT_CONTEXT_REGISTRATION context[] = {
			//{ FLT_FILE_CONTEXT, 0, nullptr, sizeof(FileContext), DRIVER_TAG },
			{ FLT_CONTEXT_END }
		};

		FLT_REGISTRATION const reg = {
			sizeof(FLT_REGISTRATION),
			FLT_REGISTRATION_VERSION,
			0,                       //  Flags
			context,                 //  Context
			callbacks,               //  Operation callbacks
			BackupUnload,                   //  MiniFilterUnload
			BackupInstanceSetup,            //  InstanceSetup
			BackupInstanceQueryTeardown,    //  InstanceQueryTeardown
			BackupInstanceTeardownStart,    //  InstanceTeardownStart
			BackupInstanceTeardownComplete, //  InstanceTeardownComplete
		};
		status = FltRegisterFilter(DriverObject, &reg, &g_Filter);
	} while (false);

	return status;
}

FLT_PREOP_CALLBACK_STATUS OnPreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext) {
	//UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->RequestorMode == KernelMode) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	ULONG createDisposition = (Data->Iopb->Parameters.Create.Options & 0xFF);

	if (Data->Iopb && Data->Iopb->TargetFileObject) {
		PFLT_FILE_NAME_INFORMATION fileNameInfo;
		NTSTATUS status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&fileNameInfo
		);

		if (NT_SUCCESS(status)) {
			FltParseFileNameInformation(fileNameInfo);

			switch (createDisposition) {
			case FILE_SUPERSEDE:
			case FILE_CREATE: {
				// Example filter condition
				if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
					//if (g_ChanGhi == ChanGhi::denyWrite) {
					if (g_ClientPort) { // Ensure the client port is connected
						// Define a custom message structure
						struct {
							ACTION Action;
							LARGE_INTEGER Time;
							USHORT FileNameLength;
							WCHAR FileName[260]; // Adjust size as needed
						} message;

						// Populate the message
						message.Action = ACTION::Create;
						KeQuerySystemTime(&message.Time);
						message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
						wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
						message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0'; // Null-terminate

						// Send message to UM
						NTSTATUS sendStatus = FltSendMessage(
							g_Filter,
							&g_ClientPort,
							&message,
							sizeof(message),
							nullptr,
							nullptr,
							nullptr
						);

						if (!NT_SUCCESS(sendStatus)) {
							DbgPrint("OnPreCreate: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
						}
					}
					/*return FLT_PREOP_COMPLETE;
				}*/


				}

				FltReleaseFileNameInformation(fileNameInfo);
				break;
			}

			case FILE_OPEN:
			case FILE_OVERWRITE:
			{
				// Example filter condition
				if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
					//if (g_ChanGhi == ChanGhi::denyWrite) {
					if (g_ClientPort) { // Ensure the client port is connected
						// Define a custom message structure
						struct {
							ACTION Action;
							LARGE_INTEGER Time;
							USHORT FileNameLength;
							WCHAR FileName[260]; // Adjust size as needed
						} message;

						// Populate the message
						message.Action = ACTION::Open;
						KeQuerySystemTime(&message.Time);
						message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
						wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
						message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0'; // Null-terminate

						// Send message to UM
						NTSTATUS sendStatus = FltSendMessage(
							g_Filter,
							&g_ClientPort,
							&message,
							sizeof(message),
							nullptr,
							nullptr,
							nullptr
						);

						if (!NT_SUCCESS(sendStatus)) {
							DbgPrint("OnPreCreate: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
						}
					}
					/*return FLT_PREOP_COMPLETE;
				}*/


				}

				FltReleaseFileNameInformation(fileNameInfo);
				break;
			}
			case FILE_OPEN_IF:
			case FILE_OVERWRITE_IF: {
				// check if the file has already existed
				// if yes, then its an open operation
				NTSTATUS fiStatus = FltQueryInformationFile(
					FltObjects->Instance,
					FltObjects->FileObject,
					&fileNameInfo,
					sizeof(FILE_BASIC_INFORMATION),
					FileBasicInformation,
					NULL
				);
				if (NT_SUCCESS(fiStatus)) {
					// this is an open operation
					// Example filter condition
					if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
						//if (g_ChanGhi == ChanGhi::denyWrite) {
						if (g_ClientPort) { // Ensure the client port is connected
							// Define a custom message structure
							struct {
								ACTION Action;
								LARGE_INTEGER Time;
								USHORT FileNameLength;
								WCHAR FileName[260]; // Adjust size as needed
							} message;

							// Populate the message
							message.Action = ACTION::Open;
							KeQuerySystemTime(&message.Time);
							message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
							wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
							message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0'; // Null-terminate

							// Send message to UM
							NTSTATUS sendStatus = FltSendMessage(
								g_Filter,
								&g_ClientPort,
								&message,
								sizeof(message),
								nullptr,
								nullptr,
								nullptr
							);

							if (!NT_SUCCESS(sendStatus)) {
								DbgPrint("OnPreCreate: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
							}
						}
						/*return FLT_PREOP_COMPLETE;
					}*/


					}

					FltReleaseFileNameInformation(fileNameInfo);
					break;
				}
				else {
					// this is an open operation
					// Example filter condition
					if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
						//if (g_ChanGhi == ChanGhi::denyWrite) {
						if (g_ClientPort) { // Ensure the client port is connected
							// Define a custom message structure
							struct {
								ACTION Action;
								LARGE_INTEGER Time;
								USHORT FileNameLength;
								WCHAR FileName[260]; // Adjust size as needed
							} message;

							// Populate the message
							message.Action = ACTION::DenyCreate;
							KeQuerySystemTime(&message.Time);
							message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
							wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
							message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0'; // Null-terminate

							// Send message to UM
							NTSTATUS sendStatus = FltSendMessage(
								g_Filter,
								&g_ClientPort,
								&message,
								sizeof(message),
								nullptr,
								nullptr,
								nullptr
							);

							if (!NT_SUCCESS(sendStatus)) {
								DbgPrint("OnPreCreate: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
							}
						}
						/*return FLT_PREOP_COMPLETE;
					}*/


					}
					FltReleaseFileNameInformation(fileNameInfo);
					return FLT_PREOP_COMPLETE;
					break;
				}
				break;
			}
			default:

				FltReleaseFileNameInformation(fileNameInfo);
				break;
			}

			
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS OnPreRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->Iopb && Data->Iopb->TargetFileObject) {
		PFLT_FILE_NAME_INFORMATION fileNameInfo;
		NTSTATUS status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&fileNameInfo
		);

		if (NT_SUCCESS(status)) {
			FltParseFileNameInformation(fileNameInfo);

			// Example filter condition
			if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {

				if (g_ClientPort) { // Ensure the client port is connected
					// Define a custom message structure
					struct {
						ACTION Action;
						LARGE_INTEGER Time;
						USHORT FileNameLength;
						WCHAR FileName[260]; // Adjust size as needed
					} message;

					// Populate the message
					message.Action = ACTION::Read;
					KeQuerySystemTime(&message.Time);
					message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
					wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
					message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0'; // Null-terminate

					// Send message to UM
					NTSTATUS sendStatus = FltSendMessage(
						g_Filter,
						&g_ClientPort,
						&message,
						sizeof(message),
						nullptr,
						nullptr,
						nullptr
					);

					if (!NT_SUCCESS(sendStatus)) {
						DbgPrint("OnPreCreate: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
					}
				}
			}

			FltReleaseFileNameInformation(fileNameInfo);
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS OnPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->RequestorMode == KernelMode) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (Data->Iopb && Data->Iopb->TargetFileObject) {
		PFLT_FILE_NAME_INFORMATION fileNameInfo;
		NTSTATUS status = FltGetFileNameInformation(
			Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&fileNameInfo
		);

		if (NT_SUCCESS(status)) {
			FltParseFileNameInformation(fileNameInfo);

			// Ki?m tra n?u file n?m trong th? m?c ???c giám sát
			if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
				// N?u ghi b? ch?n
				if (g_ChanGhi == ChanGhi::denyWrite) {
					// Ghi log g?i ??n user mode
					if (g_ClientPort) {
						struct {
							ACTION Action;
							LARGE_INTEGER Time;
							USHORT FileNameLength;
							WCHAR FileName[260];
						} message;

						message.Action = ACTION::DenyWrite;
						KeQuerySystemTime(&message.Time);
						message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
						wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
						message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0';

						NTSTATUS sendStatus = FltSendMessage(
							g_Filter,
							&g_ClientPort,
							&message,
							sizeof(message),
							nullptr,
							nullptr,
							nullptr
						);

						if (!NT_SUCCESS(sendStatus)) {
							DbgPrint("OnPreWrite: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
						}
					}

					// Ch?n ghi
					FltReleaseFileNameInformation(fileNameInfo);
					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					Data->IoStatus.Information = 0;
					return FLT_PREOP_COMPLETE;
				}
				else if (g_ChanGhi == ChanGhi::allowWrite) {
					if (g_ClientPort) {
						struct {
							ACTION Action;
							LARGE_INTEGER Time;
							USHORT FileNameLength;
							WCHAR FileName[260];
						} message;

						message.Action = ACTION::Write;
						KeQuerySystemTime(&message.Time);
						message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
						wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
						message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0';

						NTSTATUS sendStatus = FltSendMessage(
							g_Filter,
							&g_ClientPort,
							&message,
							sizeof(message),
							nullptr,
							nullptr,
							nullptr
						);

						if (!NT_SUCCESS(sendStatus)) {
							DbgPrint("OnPreWrite: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
						}
					}
				}
			}

			FltReleaseFileNameInformation(fileNameInfo);
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS OnPreSetInformation(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext) {
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);

	if (Data->RequestorMode == KernelMode) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	UNICODE_STRING BootVolumePrefix = RTL_CONSTANT_STRING(L"\\Device\\HardDiskVolume3");

	if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation) {
		PFILE_DISPOSITION_INFORMATION fileInfo =
			(PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

		if (fileInfo->DeleteFile) {
			PFLT_FILE_NAME_INFORMATION fileNameInfo;
			NTSTATUS status = FltGetFileNameInformation(
				Data,
				FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
				&fileNameInfo
			);

			if (NT_SUCCESS(status)) {
				FltParseFileNameInformation(fileNameInfo);

				// Ki?m tra n?u file n?m trong th? m?c ???c giám sát
				if (wcsstr(fileNameInfo->Name.Buffer, g_MonitorFolder.Buffer) != nullptr) {
					// N?u xóa b? ch?n
					if (g_ChanDelete == ChanDelete::denyDelete) {
						// Ghi log g?i ??n user mode
						if (g_ClientPort) {
							struct {
								ACTION Action;
								LARGE_INTEGER Time;
								USHORT FileNameLength;
								WCHAR FileName[260];
							} message;

							message.Action = ACTION::DenyDelete;
							KeQuerySystemTime(&message.Time);
							message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
							wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
							message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0';

							NTSTATUS sendStatus = FltSendMessage(
								g_Filter,
								&g_ClientPort,
								&message,
								sizeof(message),
								nullptr,
								nullptr,
								nullptr
							);

							if (!NT_SUCCESS(sendStatus)) {
								DbgPrint("OnPreSetInformation: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
							}
						}

						// Ch?n xóa
						FltReleaseFileNameInformation(fileNameInfo);
						Data->IoStatus.Status = STATUS_ACCESS_DENIED;
						Data->IoStatus.Information = 0;
						return FLT_PREOP_COMPLETE;
					}
					else if (g_ChanDelete == ChanDelete::allowDelete) {
						if (g_ClientPort) {
							struct {
								ACTION Action;
								LARGE_INTEGER Time;
								USHORT FileNameLength;
								WCHAR FileName[260];
							} message;

							message.Action = ACTION::Delete;
							KeQuerySystemTime(&message.Time);
							message.FileNameLength = (USHORT)(fileNameInfo->Name.Length / sizeof(WCHAR));
							wcsncpy(message.FileName, fileNameInfo->Name.Buffer, ARRAYSIZE(message.FileName) - 1);
							message.FileName[ARRAYSIZE(message.FileName) - 1] = L'\0';

							NTSTATUS sendStatus = FltSendMessage(
								g_Filter,
								&g_ClientPort,
								&message,
								sizeof(message),
								nullptr,
								nullptr,
								nullptr
							);

							if (!NT_SUCCESS(sendStatus)) {
								DbgPrint("OnPreSetInformation: Failed to send message to user-mode (Status=0x%08X)\n", sendStatus);
							}
						}
					}
				}

				FltReleaseFileNameInformation(fileNameInfo);
			}
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

NTSTATUS BackupUnload(FLT_FILTER_UNLOAD_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);

	// Free allocated buffer
	if (g_MonitorFolder.Buffer) {
		ExFreePoolWithTag(g_MonitorFolder.Buffer, 'fldG');
		g_MonitorFolder.Buffer = nullptr;
	}

	FltCloseCommunicationPort(g_Port);
	FltUnregisterFilter(g_Filter);

	return STATUS_SUCCESS;
}

NTSTATUS BackupInstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType) {
	KdPrint((DRIVER_PREFIX "BackupInstanceSetup FS: %u\n", VolumeFilesystemType));

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(VolumeDeviceType);

	return VolumeFilesystemType == FLT_FSTYPE_NTFS ? STATUS_SUCCESS : STATUS_FLT_DO_NOT_ATTACH;
}

NTSTATUS BackupInstanceQueryTeardown(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	KdPrint((DRIVER_PREFIX "BackupInstanceQueryTeardown\n"));

	return STATUS_SUCCESS;
}

VOID BackupInstanceTeardownStart(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_TEARDOWN_FLAGS Flags) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
}

VOID BackupInstanceTeardownComplete(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_TEARDOWN_FLAGS Flags) {
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
}

FLT_POSTOP_CALLBACK_STATUS OnPostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID, FLT_POST_OPERATION_FLAGS Flags) {
	if (Flags & FLTFL_POST_OPERATION_DRAINING)
		return FLT_POSTOP_FINISHED_PROCESSING;

	const auto& params = Data->Iopb->Parameters.Create;
	BOOLEAN dir = FALSE;
	FltIsDirectory(FltObjects->FileObject, FltObjects->Instance, &dir);
	if (dir
		|| Data->RequestorMode == KernelMode
		|| (params.SecurityContext->DesiredAccess & FILE_WRITE_DATA) == 0
		|| Data->IoStatus.Status != STATUS_SUCCESS
		|| Data->IoStatus.Information == FILE_CREATED) {
		//
		// kernel caller, not write access or a new file - skip
		//
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	FilterFileNameInformation fileNameInfo(Data);
	if (!fileNameInfo) {
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	if (!ShouldBackupFile(fileNameInfo))
		return FLT_POSTOP_FINISHED_PROCESSING;

	//
	// if it's not the default stream, we don't care
	//
	if (fileNameInfo->Stream.Length > 0)
		return FLT_POSTOP_FINISHED_PROCESSING;

	//
	// allocate and initialize a file context
	//
	FileContext* context;
	auto status = FltAllocateContext(FltObjects->Filter,
		FLT_FILE_CONTEXT, sizeof(FileContext), PagedPool,
		(PFLT_CONTEXT*)&context);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to allocate file context (0x%08X)\n", status));
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//
	// initialize context
	//
	context->Written = FALSE;
	context->Lock.Init();
	context->BackupTime.QuadPart = 0;

	//
	// set file context
	//
	status = FltSetFileContext(FltObjects->Instance,
		FltObjects->FileObject,
		FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
		context, nullptr);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to set file context (0x%08X)\n", status));
	}

	//
	// release context
	//
	FltReleaseContext(context);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

bool ShouldBackupFile(FilterFileNameInformation& nameInfo) {
	if (!NT_SUCCESS(nameInfo.Parse()))
		return false;

	static PCWSTR extensions[] = {
		L"txt", L"docx", L"doc", L"jpg", L"png"
	};

	for (auto ext : extensions)
		if (nameInfo->Extension.Buffer != nullptr && _wcsnicmp(ext, nameInfo->Extension.Buffer, wcslen(ext)) == 0)
			return true;

	return false;
}

NTSTATUS BackupFile(PUNICODE_STRING path, PCFLT_RELATED_OBJECTS FltObjects) {
	//
	// get source file size
	//
	LARGE_INTEGER fileSize;
	auto status = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
	if (!NT_SUCCESS(status) || fileSize.QuadPart == 0)
		return status;

	HANDLE hSourceFile = nullptr;
	HANDLE hTargetFile = nullptr;
	PFILE_OBJECT sourceFile = nullptr;
	PFILE_OBJECT targetFile = nullptr;
	IO_STATUS_BLOCK ioStatus;
	void* buffer = nullptr;

	do {
		//
		// open source file
		OBJECT_ATTRIBUTES sourceFileAttr;
		//
		InitializeObjectAttributes(&sourceFileAttr, path,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFileEx(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hSourceFile,			// resulting handle
			&sourceFile,			// resulting file object
			GENERIC_READ | SYNCHRONIZE, // access mask
			&sourceFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			FILE_SHARE_READ | FILE_SHARE_WRITE,	// share flags
			FILE_OPEN,			// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,				// extended attributes, EA length
			IO_IGNORE_SHARE_ACCESS_CHECK);	// flags
		if (!NT_SUCCESS(status))
			break;

		//
		// open target file
		//
		UNICODE_STRING targetFileName;
		const WCHAR backupStream[] = L":backup";
		targetFileName.MaximumLength = path->Length + sizeof(backupStream);
		targetFileName.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, targetFileName.MaximumLength, DRIVER_TAG);
		if (targetFileName.Buffer == nullptr) {
			status = STATUS_NO_MEMORY;
			break;
		}
		RtlCopyUnicodeString(&targetFileName, path);
		RtlAppendUnicodeToString(&targetFileName, backupStream);

		OBJECT_ATTRIBUTES targetFileAttr;
		InitializeObjectAttributes(&targetFileAttr, &targetFileName,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

		status = FltCreateFileEx(
			FltObjects->Filter,		// filter object
			FltObjects->Instance,	// filter instance
			&hTargetFile,			// resulting handle
			&targetFile,			// resulting file object
			GENERIC_WRITE | SYNCHRONIZE, // access mask
			&targetFileAttr,		// object attributes
			&ioStatus,				// resulting status
			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
			0,				// share flags
			FILE_OVERWRITE_IF,		// create disposition
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
			nullptr, 0,		// extended attributes, EA length
			0);	// flags

		ExFreePool(targetFileName.Buffer);
		if (!NT_SUCCESS(status)) {
			//
			// could fail if a restore operation is in progress
			//
			break;
		}

		//
		// allocate buffer for copying purposes
		//
		ULONG size = 1 << 20;	// 1 MB
		buffer = ExAllocatePool2(POOL_FLAG_PAGED, size, DRIVER_TAG);
		if (!buffer) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		// loop - read from source, write to target
		//
		ULONG bytes;
		auto saveSize = fileSize;
		while (fileSize.QuadPart > 0) {
			status = FltReadFile(
				FltObjects->Instance,
				sourceFile,			// source file object
				nullptr,			// byte offset
				(ULONG)min((LONGLONG)size, fileSize.QuadPart),	// # of bytes
				buffer,
				0,					// flags
				&bytes,				// bytes read
				nullptr, nullptr);	// no callback
			if (!NT_SUCCESS(status))
				break;

			//
			// write to target file
			//
			status = FltWriteFile(
				FltObjects->Instance,
				targetFile,			// target file
				nullptr,			// offset
				bytes,				// bytes to write
				buffer,				// data to write
				0,					// flags
				nullptr,			// written
				nullptr, nullptr);	// no callback

			if (!NT_SUCCESS(status))
				break;

			//
			// update byte count remaining
			//
			fileSize.QuadPart -= bytes;
		}
		FILE_END_OF_FILE_INFORMATION info;
		info.EndOfFile = saveSize;
		status = FltSetInformationFile(FltObjects->Instance,
			targetFile, &info, sizeof(info), FileEndOfFileInformation);
	} while (false);

	//
	// cleanup
	//
	if (buffer)
		ExFreePool(buffer);
	if (hSourceFile)
		FltClose(hSourceFile);
	if (hTargetFile)
		FltClose(hTargetFile);
	if (sourceFile)
		ObDereferenceObject(sourceFile);
	if (targetFile)
		ObDereferenceObject(targetFile);

	return status;
}

//NTSTATUS BackupFileWithSection(PUNICODE_STRING path, PCFLT_RELATED_OBJECTS FltObjects) {
//	LARGE_INTEGER fileSize;
//	auto status = FsRtlGetFileSize(FltObjects->FileObject, &fileSize);
//	if (!NT_SUCCESS(status) || fileSize.QuadPart == 0)
//		return status;
//
//	HANDLE hSourceFile = nullptr;
//	HANDLE hTargetFile = nullptr;
//	PFILE_OBJECT sourceFile = nullptr;
//	PFILE_OBJECT targetFile = nullptr;
//	IO_STATUS_BLOCK ioStatus;
//	HANDLE hSection = nullptr;
//
//	do {
//		//
//		// open source file
//		//
//		OBJECT_ATTRIBUTES sourceFileAttr;
//		InitializeObjectAttributes(&sourceFileAttr, path,
//			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
//
//		status = FltCreateFileEx(
//			FltObjects->Filter,		// filter object
//			FltObjects->Instance,	// filter instance
//			&hSourceFile,			// resulting handle
//			&sourceFile,			// resulting file object
//			GENERIC_READ | SYNCHRONIZE, // access mask
//			&sourceFileAttr,		// object attributes
//			&ioStatus,				// resulting status
//			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
//			FILE_SHARE_READ | FILE_SHARE_WRITE,	// share flags
//			FILE_OPEN,			// create disposition
//			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
//			nullptr, 0,				// extended attributes, EA length
//			IO_IGNORE_SHARE_ACCESS_CHECK);	// flags
//		if (!NT_SUCCESS(status))
//			break;
//
//		//
//		// open target file
//		//
//		UNICODE_STRING targetFileName;
//		const WCHAR backupStream[] = L":backup";
//		targetFileName.MaximumLength = path->Length + sizeof(backupStream);
//		targetFileName.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_PAGED, targetFileName.MaximumLength, DRIVER_TAG);
//		if (targetFileName.Buffer == nullptr) {
//			status = STATUS_NO_MEMORY;
//			break;
//		}
//		RtlCopyUnicodeString(&targetFileName, path);
//		RtlAppendUnicodeToString(&targetFileName, backupStream);
//
//		OBJECT_ATTRIBUTES targetFileAttr;
//		InitializeObjectAttributes(&targetFileAttr, &targetFileName,
//			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
//
//		status = FltCreateFileEx(
//			FltObjects->Filter,		// filter object
//			FltObjects->Instance,	// filter instance
//			&hTargetFile,			// resulting handle
//			&targetFile,			// resulting file object
//			GENERIC_WRITE | SYNCHRONIZE, // access mask
//			&targetFileAttr,		// object attributes
//			&ioStatus,				// resulting status
//			nullptr, FILE_ATTRIBUTE_NORMAL, 	// allocation size, file attributes
//			0,				// share flags
//			FILE_OVERWRITE_IF,		// create disposition
//			FILE_SYNCHRONOUS_IO_NONALERT | FILE_SEQUENTIAL_ONLY, // create options (sync I/O)
//			nullptr, 0,		// extended attributes, EA length
//			0);	// flags
//
//		ExFreePool(targetFileName.Buffer);
//		if (!NT_SUCCESS(status)) {
//			//
//			// could fail if a restore operation is in progress
//			//
//			break;
//		}
//
//		OBJECT_ATTRIBUTES sectionAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, OBJ_KERNEL_HANDLE);
//		status = ZwCreateSection(&hSection, SECTION_MAP_READ | SECTION_QUERY, &sectionAttributes,
//			nullptr, PAGE_READONLY, 0, hSourceFile);
//		if (!NT_SUCCESS(status))
//			break;
//
//		//
//		// loop - read from source, write to target
//		//
//		ULONG bytes;
//		LARGE_INTEGER offset{};
//		auto saveSize = fileSize;
//		PVOID buffer;
//		SIZE_T size = 1 << 20;
//		while (fileSize.QuadPart > 0) {
//			buffer = nullptr;
//			status = ZwMapViewOfSection(hSection, nullptr, &buffer, 0, 0, &offset, &size, ViewUnmap, 0, PAGE_READWRITE);
//			if (!NT_SUCCESS(status))
//				break;
//
//			bytes = (ULONG)min((LONGLONG)size, fileSize.QuadPart),	// # of bytes
//
//			status = FltWriteFile(
//				FltObjects->Instance,
//				targetFile,			// target file
//				nullptr,			// offset
//				bytes,				// bytes to write
//				buffer,				// data to write
//				0,					// flags
//				&bytes,			// written
//				nullptr, nullptr);	// no callback
//
//			ZwUnmapViewOfSection(nullptr, buffer);
//			if (!NT_SUCCESS(status))
//				break;
//
//			//
//			// update byte count remaining
//			//
//			fileSize.QuadPart -= bytes;
//			offset.QuadPart += bytes;
//		}
//		FILE_END_OF_FILE_INFORMATION info;
//		info.EndOfFile = saveSize;
//		status = FltSetInformationFile(FltObjects->Instance,
//			targetFile, &info, sizeof(info), FileEndOfFileInformation);
//	} while (false);
//
//	//
//	// cleanup
//	//
//	if (hSection)
//		ZwClose(hSection);
//	if (hSourceFile)
//		FltClose(hSourceFile);
//	if (hTargetFile)
//		FltClose(hTargetFile);
//	if (sourceFile)
//		ObDereferenceObject(sourceFile);
//	if (targetFile)
//		ObDereferenceObject(targetFile);
//
//	return status;
//}

//FLT_PREOP_CALLBACK_STATUS OnPreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
//	//
//	// get the file context if exists
//	//
//	FileContext* context;
//
//	auto status = FltGetFileContext(FltObjects->Instance,
//		FltObjects->FileObject,
//		(PFLT_CONTEXT*)&context);
//	if (!NT_SUCCESS(status) || context == nullptr) {
//		//
//		// no context, continue normally
//		//
//		return FLT_PREOP_SUCCESS_NO_CALLBACK;
//	}
//
//	do {
//		Locker locker(context->Lock);
//		if (context->Written) {
//			//
//			// already written, nothing to do
//			//
//			break;
//		}
//		FilterFileNameInformation name(Data);
//		if (!name)
//			break;
//
//		status = BackupFile(&name->Name, FltObjects);
//		if (!NT_SUCCESS(status)) {
//			KdPrint(("Failed to backup file! (0x%X)\n", status));
//		}
//		else {
//			KeQuerySystemTimePrecise(&context->BackupTime);
//			if (g_ClientPort) {
//				USHORT nameLen = name->Name.Length;
//				USHORT len = sizeof(FileBackupPortMessage) + nameLen;
//				auto msg = (FileBackupPortMessage*)ExAllocatePool2(
//					POOL_FLAG_PAGED, len, DRIVER_TAG);
//				if (msg) {
//					msg->FileNameLength = nameLen / sizeof(WCHAR);
//					RtlCopyMemory(msg->FileName, name->Name.Buffer, nameLen);
//					LARGE_INTEGER timeout;
//					timeout.QuadPart = -10000 * 100; // 100 msec
//					FltSendMessage(g_Filter, &g_ClientPort, msg, len,
//						nullptr, nullptr, &timeout);
//					ExFreePool(msg);
//				}
//			}
//		}
//		context->Written = TRUE;
//	} while (false);
//
//	FltReleaseContext(context);
//
//	//
//	// don't prevent the write regardless
//	//
//	return FLT_PREOP_SUCCESS_NO_CALLBACK;
//}

FLT_POSTOP_CALLBACK_STATUS OnPostCleanup(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID, FLT_POST_OPERATION_FLAGS Flags) {
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(Data);

	FileContext* context;

	auto status = FltGetFileContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
	if (!NT_SUCCESS(status) || context == nullptr) {
		//
		// no context, continue normally
		//
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	FltReleaseContext(context);
	FltDeleteContext(context);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS PortConnectNotify(PFLT_PORT ClientPort, PVOID ServerPortCookie, PVOID ConnectionContext, ULONG SizeOfContext, PVOID* ConnectionPortCookie) {
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);

	g_ClientPort = ClientPort;

	return STATUS_SUCCESS;
}

void PortDisconnectNotify(PVOID ConnectionCookie) {
	UNREFERENCED_PARAMETER(ConnectionCookie);

	FltCloseClientPort(g_Filter, &g_ClientPort);
	g_ClientPort = nullptr;
}

NTSTATUS PortMessageNotify(PVOID PortCookie, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, PULONG ReturnOutputBufferLength) {
	UNREFERENCED_PARAMETER(PortCookie);
	//UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

	struct UserMessageToKernel {
		USHORT Length;
		WCHAR Buffer[260];
		ChanGhi ActionGhi;
		ChanDelete ActionDelete;
	};

	auto info = (UserMessageToKernel*)InputBuffer;

	/*if (g_MonitorFolder.Buffer) {
		ExFreePoolWithTag(g_MonitorFolder.Buffer, 'aaaa');
		g_MonitorFolder.Buffer = nullptr;
	}*/

	auto size = info->Length;
	g_ChanGhi = info->ActionGhi;
	g_ChanDelete = info->ActionDelete;
	g_MonitorFolder.Length = size + sizeof(WCHAR);
	g_MonitorFolder.MaximumLength = size;
	g_MonitorFolder.Buffer = (PWSTR)ExAllocatePool2(POOL_FLAG_PAGED, size, 'fldG');

	if (!g_MonitorFolder.Buffer) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Copy the string safely
	RtlCopyMemory(g_MonitorFolder.Buffer, info->Buffer, info->Length);
	g_MonitorFolder.Buffer[info->Length / sizeof(WCHAR)] = L'\0';


	return STATUS_SUCCESS;
}
