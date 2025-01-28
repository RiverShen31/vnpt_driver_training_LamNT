#include "pch.h"
#include "Main.h"

// globals

Globals g_Data;
Callouts* g_Callouts;

// prototypes

void ProcNetFilterUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ProcNetFilterCreateClose(PDEVICE_OBJECT, PIRP Irp);
//NTSTATUS ProcNetFilterDeviceControl(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS ProcNetFilterRead(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);

// DriverEntry

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ProcNetFilter");
	PDEVICE_OBJECT devObj;
	auto status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
	if (!NT_SUCCESS(status))
		return status;

	devObj->Flags |= DO_DIRECT_IO;

	bool symLinkCreated = false;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcNetFilter");
	do {
		g_Data.Init(1000);
		/*g_Data = new (PoolType::NonPaged) Globals;
		if (!g_Data) {
			status = STATUS_NO_MEMORY;
			break;
		}*/
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
			break;
		symLinkCreated = true;

		status = g_Callouts->RegisterCallouts(devObj);
		if (!NT_SUCCESS(status))
			break;
	} while (false);

	if (!NT_SUCCESS(status)) {
		KdPrint((DRIVER_PREFIX "DriverEntry failed (0x%X)\n", status));
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		IoDeleteDevice(devObj);
		return status;
	}

	DriverObject->DriverUnload = ProcNetFilterUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProcNetFilterCreateClose;
	//DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcNetFilterDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_READ] = ProcNetFilterRead;
	return STATUS_SUCCESS;
}

void ProcNetFilterUnload(PDRIVER_OBJECT DriverObject) {
	LIST_ENTRY* entry;
	while ((entry = g_Data.RemoveItem()) != nullptr)
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ConnectionInfo>, Entry));

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcNetFilter");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ProcNetFilterCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	return CompleteRequest(Irp);
}

//NTSTATUS ProcNetFilterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
//	auto irpSp = IoGetCurrentIrpStackLocation(Irp);
//	auto const& dic = irpSp->Parameters.DeviceIoControl;
//	auto status = STATUS_INVALID_DEVICE_REQUEST;
//	ULONG info = 0;
//
//	switch (dic.IoControlCode) {
//		case IOCTL_PNF_CLEAR:
//			status = g_Data->ClearProcesses();
//			break;
//
//		case IOCTL_PNF_BLOCK_PROCESS:
//		case IOCTL_PNF_PERMIT_PROCESS:
//			if (dic.InputBufferLength < sizeof(ULONG)) {
//				status = STATUS_BUFFER_TOO_SMALL;
//				break;
//			}
//			auto pid = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
//			status = dic.IoControlCode == IOCTL_PNF_BLOCK_PROCESS ? g_Data->AddProcess(pid) : g_Data->DeleteProcess(pid);
//			if (NT_SUCCESS(status))
//				info = sizeof(ULONG);
//			break;
//	}
//
//	return CompleteRequest(Irp, status, info);
//}

NTSTATUS ProcNetFilterRead(PDEVICE_OBJECT, PIRP Irp) {
	auto irpSp = IoGetCurrentIrpStackLocation(Irp);
	auto len = irpSp->Parameters.Read.Length;
	auto status = STATUS_SUCCESS;
	ULONG bytes = 0;
	NT_ASSERT(Irp->MdlAddress);        // we're using Direct I/O

	auto buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		// Check if the list is empty
		if (g_Data.IsEmpty()) {
			// If empty, return immediately
			status = STATUS_NO_MORE_ENTRIES;
		}
		else {
			while (true) {
				auto entry = g_Data.RemoveItem();
				if (entry == nullptr)
					break;

				auto info = CONTAINING_RECORD(entry, FullItem<ConnectionInfo>, Entry);
				auto size = info->Data.Size;
				if (len < size) {
					// user's buffer too small, insert item back
					g_Data.AddHeadItem(entry);
					break;
				}
				memcpy(buffer, &info->Data, size);
				len -= size;
				buffer += size;
				bytes += size;
				ExFreePool(info);
			}
		}
	}
	return CompleteRequest(Irp, status, bytes);
}


NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
