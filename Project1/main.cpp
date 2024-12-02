#include "ntddk.h"

// Unload routine
VOID Unload(PDRIVER_OBJECT pDriverObject) {
    UNREFERENCED_PARAMETER(pDriverObject);
    DbgPrint("Bye Kernel World!\n");
}

// Driver entry point
extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(pDriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    // Initialize components
    pDriverObject->DriverUnload = Unload;

    // Log message
    DbgPrint("Hello Kernel World!\n");

    return STATUS_SUCCESS; // Return success
}
