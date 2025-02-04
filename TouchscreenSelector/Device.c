#include "driver.h"
#include "device.tmh"
#include <acpiioct.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, TouchscreenSelectorCreateDevice)
#endif

NTSTATUS
ParseConfig(
    PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    ACPI_EVAL_INPUT_BUFFER inputBuffer;
    PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
    WDF_MEMORY_DESCRIPTOR inputDescriptor;
    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    PACPI_METHOD_ARGUMENT packageArgument;
    PACPI_METHOD_ARGUMENT dataArgument;
    ANSI_STRING deviceIdString;
    WDFIOTARGET ioTarget;
    ULONG sizeReturned = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    RtlZeroMemory(&inputBuffer, sizeof(ACPI_EVAL_INPUT_BUFFER));
    inputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_SIGNATURE;

    inputBuffer.MethodNameAsUlong = (ULONG)'FCST';
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor, (PVOID)&inputBuffer, sizeof(ACPI_EVAL_INPUT_BUFFER));

    outputBuffer = (PACPI_EVAL_OUTPUT_BUFFER)ExAllocatePoolWithTag(NonPagedPoolNx, 4096, POOL_TAG);

    if (outputBuffer == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Unable to allocate memory");
        status = STATUS_MEMORY_NOT_ALLOCATED;
        goto Exit;
    }

    RtlZeroMemory(outputBuffer, 4096);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, (PVOID)outputBuffer, 4096);

    ioTarget = WdfDeviceGetIoTarget(Context->Device);

    status = WdfIoTargetSendIoctlSynchronously(ioTarget,
        NULL,
        IOCTL_ACPI_EVAL_METHOD,
        &inputDescriptor,
        &outputDescriptor,
        NULL,
        (PULONG_PTR)&sizeReturned);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Failed to read config from ACPI");
        status = STATUS_ACPI_INVALID_DATA;
        goto Exit;
    }

    if (outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE || outputBuffer->Count == 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Invalid buffer signature or count");
        status = STATUS_ACPI_INVALID_DATA;
        goto Exit;
    }

    if (outputBuffer->Argument[0].Type != ACPI_METHOD_ARGUMENT_INTEGER) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Invalid argument type");
        status = STATUS_ACPI_INVALID_DATA;
        goto Exit;
    }

    Context->NumberOfTouchscreens = outputBuffer->Argument[0].Argument;
    packageArgument = (ACPI_METHOD_ARGUMENT*)ACPI_METHOD_NEXT_ARGUMENT(&(outputBuffer->Argument[0]));

    for (UINT32 i = 0; i < Context->NumberOfTouchscreens; i++) {
        if (packageArgument->Type != ACPI_METHOD_ARGUMENT_PACKAGE || packageArgument->DataLength == 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Invalid argument type");
            status = STATUS_ACPI_INVALID_DATA;
            goto Exit;
        }

        dataArgument = (PACPI_METHOD_ARGUMENT)packageArgument->Data;
        if (dataArgument->Type != ACPI_METHOD_ARGUMENT_INTEGER)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Invalid argument type");
            status = STATUS_ACPI_INVALID_DATA;
            goto Exit;
        }

        Context->TSList[i].PanelID = dataArgument->Argument;

        dataArgument = (ACPI_METHOD_ARGUMENT*)ACPI_METHOD_NEXT_ARGUMENT(dataArgument);
        if (dataArgument->Type != ACPI_METHOD_ARGUMENT_STRING)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Invalid argument type");
            status = STATUS_ACPI_INVALID_DATA;
            goto Exit;
        }
        RtlInitAnsiString(&deviceIdString, (PCSZ)dataArgument->Data);
        RtlAnsiStringToUnicodeString(&Context->TSList[i].DeviceID, &deviceIdString, TRUE);

        packageArgument = (ACPI_METHOD_ARGUMENT*)ACPI_METHOD_NEXT_ARGUMENT(packageArgument);
    }

    status = STATUS_SUCCESS;
Exit:
    if (outputBuffer != NULL) {
        ExFreePoolWithTag(outputBuffer, POOL_TAG);
    }
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS
TouchscreenSelectorCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;
    UINT32 PanelID;
    UINT32 i;
    PWDFDEVICE_INIT deviceInit;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
    WDFDEVICE child = NULL;

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        deviceContext = DeviceGetContext(device);

        deviceContext->Device = device;

        status = ParseConfig(deviceContext);
        if (!NT_SUCCESS(status)) {
            goto Exit;
        }

        {
            GUID guid = { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };
            UNICODE_STRING efiVar;
            unsigned int outVar[0x78];
            ULONG outVarLen;

            RtlInitUnicodeString(&efiVar, L"UEFIDisplayInfo");

            outVarLen = sizeof(outVar);
            status = ExGetFirmwareEnvironmentVariable(&efiVar, &guid, &outVar, &outVarLen, NULL);
            if (!NT_SUCCESS(status) || outVarLen != 0x78) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Failed to read UEFIDisplayInfo");
                status = STATUS_DATA_ERROR;
                goto Exit;
            }

            PanelID = outVar[10];
        }

        for (i = 0; i < deviceContext->NumberOfTouchscreens; i++) {
            if (PanelID == deviceContext->TSList[i].PanelID) {
                deviceInit = WdfPdoInitAllocate(device);

                if (deviceInit == NULL) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    goto Exit;
                }

                WdfDeviceInitSetDeviceType(deviceInit, FILE_DEVICE_BUS_EXTENDER);

                status = WdfPdoInitAssignDeviceID(deviceInit, &deviceContext->TSList[i].DeviceID);
                if (!NT_SUCCESS(status)) {
                    goto Exit;
                }

                status = WdfPdoInitAddHardwareID(deviceInit, &deviceContext->TSList[i].DeviceID);
                if (!NT_SUCCESS(status)) {
                    goto Exit;
                }

                status = WdfDeviceCreate(&deviceInit, WDF_NO_OBJECT_ATTRIBUTES, &child);
                if (!NT_SUCCESS(status)) {
                    goto Exit;
                }

                WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);

                pnpCaps.Address = 0;
                pnpCaps.UINumber = 0;

                WdfDeviceSetPnpCapabilities(child, &pnpCaps);

                status = WdfFdoAddStaticChild(device, child);
                if (!NT_SUCCESS(status)) {
                    goto Exit;
                }
                break;
            }
        }

        if (child == NULL) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Unknown PanelID: 0x%X", PanelID);
            status = STATUS_NOT_FOUND;
            goto Exit;
        }
    }

Exit:
    return status;
}