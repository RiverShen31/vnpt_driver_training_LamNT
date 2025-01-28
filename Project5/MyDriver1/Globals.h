#pragma once

#include "FastMutex.h"

struct ConnectionInfo {
	ULONG pid;
	USHORT Size;
	UINT16 LocalPort;
	UINT16 RemotePort;
	UINT32 LocalAddress;
	UINT32 RemoteAddress;
	UINT8 Protocol;
	BOOLEAN Direction; // TRUE = OUT, FALSE = IN
	LARGE_INTEGER Timestamp;
	USHORT ProcessPathLength;
	WCHAR ProcessPath[1];
};

struct Globals {
	void Init(ULONG maxItems);
	void AddItem(LIST_ENTRY* entry);
	void AddHeadItem(LIST_ENTRY* entry);
	bool IsEmpty();
	LIST_ENTRY* RemoveItem();

private:
	LIST_ENTRY m_ItemsHead;
	ULONG m_Count;
	ULONG m_MaxCount;
	FastMutex m_Lock;
};

