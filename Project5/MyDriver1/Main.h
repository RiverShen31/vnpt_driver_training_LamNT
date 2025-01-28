#pragma once

#define DRIVER_TAG 'rfnp'
#define DRIVER_PREFIX "ProcNetFilter: "
#include "ProcNetFilterPublic.h"
#include "Locker.h"
#include "Globals.h"
#include "Callouts.h"

extern Globals g_Data;
extern Callouts* g_Callouts;

template<typename T>
struct FullItem {
	LIST_ENTRY Entry;
	T Data;
};
