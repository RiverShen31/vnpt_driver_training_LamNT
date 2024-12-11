#include <ntddk.h>
#include <ntstrsafe.h>

#define IOCTL_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1337, METHOD_BUFFERED, FILE_ANY_ACCESS)

UNICODE_STRING DeviceName, SymbolicLinkName;
PDEVICE_OBJECT DeviceObject;

char globalInput[256] = { 0 };

NTSTATUS FUNC_IRP_MJ_READ(PDEVICE_OBJECT DriverObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DriverObject);

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG lengthToRead = stack->Parameters.Read.Length;
	UCHAR* buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

	LARGE_INTEGER systemTime;
	KeQuerySystemTime(&systemTime);

	// Thời gian UNIX (epoch time)
	UINT64 unixTime = (UINT64)((systemTime.QuadPart - 116444736000000000LL) / 10000000LL);

	// Format timestamp và dữ liệu từ globalInput
	char timestamp[64] = { 0 };
	RtlStringCchPrintfA(timestamp, sizeof(timestamp), "%llu", unixTime);

	char combinedOutput[512] = { 0 };  // Đảm bảo đủ lớn để chứa timestamp + dữ liệu
	RtlStringCchPrintfA(combinedOutput, sizeof(combinedOutput), "[%s] %s", timestamp, globalInput);

	

	// Copy dữ liệu vào buffer (dọn dẹp trước)
	RtlZeroMemory(buffer, lengthToRead);
	size_t copyLength = min(lengthToRead, strlen(combinedOutput));
	RtlCopyMemory(buffer, combinedOutput, copyLength);

	// Thiết lập thông tin trả về
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = copyLength;  // Thiết lập số byte đã sao chép

	// Hoàn tất IRP
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS FUNC_IRP_FILTER(PDEVICE_OBJECT DriverObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(Irp);

	return STATUS_SUCCESS;
}

NTSTATUS FUNC_IRP_MJ_DEVICE_CONTROL(PDEVICE_OBJECT DriverObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DriverObject);

	PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
	void* buffer = Irp->AssociatedIrp.SystemBuffer;
	char msg[256] = { 0 };

	// Đảm bảo biến được khởi tạo trước khi switch
	SIZE_T inputBufferLength = 0;

	DbgPrint("IRP_MJ_DEVICE_CONTROL called.\n");

	switch (pIoStackLocation->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_TEST:
	{
		// Lấy kích thước bộ đệm đầu vào
		inputBufferLength = pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength;

		if (inputBufferLength > 0 && buffer != NULL)
		{
			// Ghi log chuỗi nhận được từ user-mode
			DbgPrint("Message received from user-mode: %s\n", (char*)buffer);

			// Sao chép chuỗi từ buffer (UM) vào msg
			SIZE_T copyLength = min(inputBufferLength, sizeof(msg) - 1);
			RtlZeroMemory(msg, sizeof(msg));
			RtlCopyMemory(msg, buffer, copyLength);
			RtlZeroMemory(globalInput, sizeof(globalInput));
			RtlCopyMemory(globalInput, buffer, copyLength);
			msg[copyLength] = '\0'; // Đảm bảo null-terminated

			// Sao chép lại chuỗi nhận được vào buffer để gửi trả lại
			RtlZeroMemory(buffer, inputBufferLength);
			RtlCopyMemory(buffer, msg, strlen(msg));

			// Trả về thông tin
			Irp->IoStatus.Information = strlen(msg);
			Irp->IoStatus.Status = STATUS_SUCCESS;
		}
		else
		{
			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		}
	}
	break;

	default:
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		break;
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DbgPrint("Request completed.\n");

	return Irp->IoStatus.Status;
}


void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	IoDeleteSymbolicLink(&SymbolicLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);

	DbgPrint("Driver unloaded.\n");
}

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;

	DbgPrint("Entry point called.\n");

	RtlInitUnicodeString(&DeviceName, L"\\Device\\ExampleKernelDriver");
	RtlInitUnicodeString(&SymbolicLinkName, L"\\DosDevices\\ExampleKernelDriver");

	status = IoCreateDevice(DriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		TRUE,
		&DeviceObject);


	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice failed.\n");
		return status;
	}

	DeviceObject->Flags |= DO_DIRECT_IO;


	status = IoCreateSymbolicLink(&SymbolicLinkName, &DeviceName);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateSymbolicLink failed.\n");
		return status;
	}

	// We need to implement IRP_MJ_CREATE otherwise CreateFileW in usermode fails with error code 1
	for (auto i = 0u; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = FUNC_IRP_FILTER;

	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_READ] = FUNC_IRP_MJ_READ;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = FUNC_IRP_MJ_DEVICE_CONTROL;
	
	DbgPrint("Initialized driver.\n");

	return STATUS_SUCCESS;
}