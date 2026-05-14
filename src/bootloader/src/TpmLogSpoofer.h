#ifndef _TPM_LOG_SPOOFER_H_
#define _TPM_LOG_SPOOFER_H_

#include <Uefi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main function to spoof TPM event log
EFI_STATUS
EFIAPI
SpoofTpmEventLog(
  VOID
  );

#ifdef __cplusplus
}
#endif

#endif // _TPM_LOG_SPOOFER_H_
