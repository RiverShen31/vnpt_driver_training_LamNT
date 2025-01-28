#include "pch.h"
#include "Main.h"
#include "Globals.h"
#include "Callouts.h"

NTSTATUS OnCalloutNotify(
	_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	_In_ const GUID* filterKey,
	_Inout_ FWPS_FILTER* filter);

void OnCalloutClassify(
	_In_ const FWPS_INCOMING_VALUES* inFixedValues,
	_In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	_Inout_opt_ void* layerData,
	_In_opt_ const void* classifyContext,
	_In_ const FWPS_FILTER* filter,
	_In_ UINT64 flowContext,
	_Inout_ FWPS_CLASSIFY_OUT* classifyOut);

Callouts::Callouts() {
	s_Callouts = this;
}

Callouts& Callouts::Get() {
	return *s_Callouts;
}

Callouts::~Callouts() {
	const GUID* guids[] = {
		&GUID_CALLOUT_PROCESS_BLOCK_V4,
		&GUID_CALLOUT_PROCESS_BLOCK_V6,
		&GUID_CALLOUT_PROCESS_BLOCK_UDP_V4,
		&GUID_CALLOUT_PROCESS_BLOCK_UDP_V6,
	};
	for (auto& guid : guids)
		FwpsCalloutUnregisterByKey(guid);
}

NTSTATUS Callouts::RegisterCallouts(PDEVICE_OBJECT devObj) {
	const GUID* guids[] = {
		&GUID_CALLOUT_PROCESS_BLOCK_V4,
		&GUID_CALLOUT_PROCESS_BLOCK_V6,
		&GUID_CALLOUT_PROCESS_BLOCK_UDP_V4,
		&GUID_CALLOUT_PROCESS_BLOCK_UDP_V6,
	};
	NTSTATUS status = STATUS_SUCCESS;

	for (auto& guid : guids) {
		FWPS_CALLOUT callout{};
		callout.calloutKey = *guid;
		callout.notifyFn = OnCalloutNotify;
		callout.classifyFn = OnCalloutClassify;
		status |= FwpsCalloutRegister(devObj, &callout, nullptr);
	}
	return status;
}

NTSTATUS Callouts::DoCalloutNotify(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, FWPS_FILTER* filter) {
	UNREFERENCED_PARAMETER(filter);

	UNICODE_STRING sguid = RTL_CONSTANT_STRING(L"<Noguid>");
	if (filterKey)
		RtlStringFromGUID(*filterKey, &sguid);

	if (notifyType == FWPS_CALLOUT_NOTIFY_ADD_FILTER) {
		KdPrint((DRIVER_PREFIX "Filter added: %wZ\n", sguid));
	}
	else if (notifyType == FWPS_CALLOUT_NOTIFY_DELETE_FILTER) {
		KdPrint((DRIVER_PREFIX "Filter deleted: %wZ\n", sguid));
	}
	if (filterKey)
		RtlFreeUnicodeString(&sguid);

	return STATUS_SUCCESS;
}

void Callouts::DoCalloutClassify(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData,
	const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut) {
	UNREFERENCED_PARAMETER(flowContext);
	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(classifyOut);
	// search for the PID (if available)
	if ((inMetaValues->currentMetadataValues & FWPS_METADATA_FIELD_PROCESS_ID) == 0)
		return;


	USHORT allocSize = sizeof(FullItem<ConnectionInfo>);
	USHORT ProcessPathSize = 0;


	// L?c k?t n?i TCP/IP, IPV4
	if (inFixedValues->layerId == FWPS_LAYER_ALE_RESOURCE_ASSIGNMENT_V4 || inFixedValues->layerId == FWPS_LAYER_ALE_AUTH_CONNECT_V4) {

		if (inMetaValues->processPath != nullptr) {
			ProcessPathSize = (USHORT)inMetaValues->processPath->size;
			allocSize += ProcessPathSize;
		}

		auto info = (FullItem<ConnectionInfo>*)ExAllocatePool2(POOL_FLAG_PAGED, allocSize, 'abcd');
		if (info == nullptr) {
			DbgPrint("Failed allocation\n");
			return;
		}
		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Timestamp);
		item.pid = (ULONG)inMetaValues->processId;
		item.Size = sizeof(ConnectionInfo) + ProcessPathSize;
		item.Direction = FALSE;
		item.LocalAddress = inFixedValues->incomingValue[FWPS_FIELD_INBOUND_IPPACKET_V4_IP_LOCAL_ADDRESS_TYPE].value.uint32;
		item.RemoteAddress = inFixedValues->incomingValue[FWPS_FIELD_INBOUND_IPPACKET_V4_IP_REMOTE_ADDRESS].value.uint32;
		item.RemotePort = inFixedValues->incomingValue[FWPS_FIELD_INBOUND_TRANSPORT_V4_IP_LOCAL_PORT].value.uint16;
		item.LocalPort = inFixedValues->incomingValue[FWPS_FIELD_INBOUND_TRANSPORT_V4_IP_REMOTE_PORT].value.uint16;

		if (ProcessPathSize > 0) {
			memcpy(item.ProcessPath, inMetaValues->processPath->data, ProcessPathSize);
			item.ProcessPathLength = ProcessPathSize / sizeof(WCHAR);
		}
		else {
			item.ProcessPathLength = 0;
		}
		g_Data.AddItem(&info->Entry);
	}
}

NTSTATUS OnCalloutNotify(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, FWPS_FILTER* filter) {
	return Callouts::Get().DoCalloutNotify(notifyType, filterKey, filter);
}

void OnCalloutClassify(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData, const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut) {
	Callouts::Get().DoCalloutClassify(inFixedValues, inMetaValues, layerData, classifyContext, filter, flowContext, classifyOut);
}