#pragma once

class Callouts {
public:
	Callouts();
	static Callouts& Get();
	Callouts(Callouts const&) = delete;
	Callouts& operator=(Callouts const&) = delete;
	~Callouts();

	NTSTATUS RegisterCallouts(PDEVICE_OBJECT devObj);

	NTSTATUS DoCalloutNotify(
		_In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
		_In_ const GUID* filterKey,
		_Inout_ FWPS_FILTER* filter);

	void DoCalloutClassify(
		_In_ const FWPS_INCOMING_VALUES* inFixedValues,
		_In_ const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
		_Inout_opt_ void* layerData,
		_In_opt_ const void* classifyContext,
		_In_ const FWPS_FILTER* filter,
		_In_ UINT64 flowContext,
		_Inout_ FWPS_CLASSIFY_OUT* classifyOut);

private:
	inline static Callouts* s_Callouts;
};