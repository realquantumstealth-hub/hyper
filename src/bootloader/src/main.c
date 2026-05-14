#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>

#include "bootmgfw/bootmgfw.h"
#include "hyperv_attachment/hyperv_attachment.h"
#include "TpmMeasurementFilter.h"
#include "disk/disk.h"

const UINT8 _gDriverUnloadImageCount = 1;
const UINT32 _gUefiDriverRevision = 0x200;
CHAR8* gEfiCallerBaseName = "hyper";

#define d_bootmgfw_path L"\\efi\\microsoft\\boot\\bootmgfw.efi"

EFI_STATUS
EFIAPI
UefiUnload(
    IN EFI_HANDLE image_handle
)
{
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE image_handle,
    IN EFI_SYSTEM_TABLE* system_table
)
{
    EFI_HANDLE device_handle = NULL;
    EFI_STATUS status;
    
    Print(L"\n=== Hyper UEFI Boot Manager ===\n");
    Print(L"[*] Initializing TPM measurement filtering...\n");
    
    //Print(L"[*] Installing TPM measurement filter...\n");
    //status = TpmMeasurementFilterEntry(image_handle, system_table);
    //if (EFI_ERROR(status)) {
    //    Print(L"[!] Failed to install TPM measurement filter: %r\n", status);
    //    // Continue anyway, the filter is optional
    //} else {
    //    Print(L"[*] TPM measurement filter installed successfully\n");
    //}
    //
    // Step 2: Restore original bootmgfw.efi file
    Print(L"[*] Restoring original bootmgfw.efi...\n");
    status = bootmgfw_restore_original_file(&device_handle);
    if (status != EFI_SUCCESS) {
        Print(L"[!] Failed to restore original bootmgfw.efi: %r\n", status);
        return status;
    }
    Print(L"[*] Original bootmgfw.efi restored successfully\n");
    

    Print(L"[*] Setting up Hyper-V attachment...\n");
    status = hyperv_attachment_set_up();
    if (status != EFI_SUCCESS) {
        Print(L"[!] Failed to set up Hyper-V attachment: %r\n", status);
        return status;
    }
    Print(L"[*] Hyper-V attachment set up successfully\n");






    Print(L"[*] Running original bootmgfw.efi with hooks...\n");
    status = bootmgfw_run_original_image(image_handle, device_handle);

    if (EFI_ERROR(status)) {
        Print(L"[!] Failed to run original bootmgfw.efi: %r\n", status);
    }

    return status;
}
