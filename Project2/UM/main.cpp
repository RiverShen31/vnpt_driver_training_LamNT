#include <Windows.h>
#include <stdio.h>

#define IOCTL_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1337, METHOD_BUFFERED, FILE_ANY_ACCESS)

bool LoadDriver()
{
	char inputBuffer[100] = { 0 };
	char outputBuffer[100] = { 0 };

	printf("Enter a string to send to the kernel: ");
	fgets(inputBuffer, sizeof(inputBuffer), stdin);

	// Xóa ký tự xuống dòng do fgets thêm
	size_t len = strlen(inputBuffer);
	if (len > 0 && inputBuffer[len - 1] == '\n')
		inputBuffer[len - 1] = '\0';

	HANDLE driver = CreateFileW(
		L"\\\\.\\ExampleKernelDriver",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		INVALID_HANDLE_VALUE);

	if (driver == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		printf("CreateFileW failed. %d\n", error);
		return false;
	}

	DWORD bytesReturned = 0;
	if (!DeviceIoControl(
		driver,
		IOCTL_TEST,
		inputBuffer,
		static_cast<DWORD>(strlen(inputBuffer) + 1), // Truyen ca ky tu NULL
		outputBuffer,
		sizeof(outputBuffer),
		&bytesReturned,
		nullptr))
	{
		printf("DeviceIoControl failed.\n");
		CloseHandle(driver);
		return false;
	}

	printf("Response from kernel: %s\n", outputBuffer);

	CloseHandle(driver);
	return true;
}

int main()
{
	LoadDriver();

	getchar();

	return 0;
}