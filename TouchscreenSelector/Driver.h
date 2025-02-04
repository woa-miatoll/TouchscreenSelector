#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "device.h"
#include "trace.h"

#define POOL_TAG 'DSST'

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD TouchscreenSelectorEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP TouchscreenSelectorEvtDriverContextCleanup;

EXTERN_C_END
