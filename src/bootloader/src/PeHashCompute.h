#ifndef _PE_HASH_COMPUTE_H_
#define _PE_HASH_COMPUTE_H_

#include <Uefi.h>

#ifdef __cplusplus
extern "C" {
#endif

// External variables for hash storage
extern UINT8 LegitimateBootmgfwHash[32];
extern BOOLEAN HashComputed;

// Function to compute Authenticode PE/COFF hash of an image
EFI_STATUS
ComputeAuthenticodeHash(
    IN  UINT8   *ImageBase,
    IN  UINTN   ImageSize,
    OUT UINT8   *Hash
    );

// Function to compute and store the legitimate bootmgfw hash
EFI_STATUS
ComputeAndStoreLegitimateBootmgfwHash(
    IN  UINT8   *ImageBase,
    IN  UINTN   ImageSize
    );

#ifdef __cplusplus
}
#endif

#endif // _PE_HASH_COMPUTE_H_
