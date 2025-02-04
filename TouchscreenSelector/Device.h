EXTERN_C_START

#define MAX_TS_NUMBER 16

typedef struct _TS_INFO
{
    UINT32 PanelID;
    UNICODE_STRING DeviceID;
} TS_INFO, *PTS_INFO;

typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE Device;
    UINT32 NumberOfTouchscreens;
    TS_INFO TSList[MAX_TS_NUMBER];
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS
TouchscreenSelectorCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EXTERN_C_END
