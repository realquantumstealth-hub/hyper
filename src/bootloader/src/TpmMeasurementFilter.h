#ifndef _TPM_MEASUREMENT_FILTER_H_
#define _TPM_MEASUREMENT_FILTER_H_

#include <Uefi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to install the TPM measurement filter
// Call this early in your driver's entry point

    EFI_STATUS
        EFIAPI
        TpmMeasurementFilterEntry(
            IN EFI_HANDLE        ImageHandle,
            IN EFI_SYSTEM_TABLE* SystemTable
        );

    EFI_STATUS
        InitializeTpmResetReplay(
            VOID
        );

    VOID
        SetLegitimateBootmgfwInfo(
            IN EFI_PHYSICAL_ADDRESS ImageBase,
            IN UINT64               ImageSize
        );

    EFI_STATUS
        CleanTpmBeforeChaining(
            VOID
        );


#ifdef __cplusplus
}
#endif

#endif // _TPM_MEASUREMENT_FILTER_H_
