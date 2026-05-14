#include "winload.h"
#include "../hooks/hooks.h"
#include "../image/image.h"
#include "../bootmgfw/bootmgfw.h"
#include "../structures/ntdef.h"
#include "../hvloader/hvloader.h"
#include "../hyperv_attachment/hyperv_attachment.h"

// Additional includes for debug file functionality
#include <Library/UefiLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Library/UefiBootServicesTableLib.h>
#include <intrin.h>

UINT64 pml4_physical_allocation = 0;
UINT64 pdpt_physical_allocation = 0;

hook_data_t winload_load_pe_image_hook_data = { 0 };
hook_data_t oslp_build_kernel_memory_map_hook_data = { 0 };
hook_data_t osl_arch_transfer_to_kernel_hook_data = { 0 };

// Note: OslArchTransferToKernel hook removed - can't hook post-ExitBootServices from UEFI app

#ifndef _NTDEF_
#define _NTDEF_
typedef void* PVOID;
typedef UINT32 ULONG;
typedef ULONG* PULONG;
typedef INT32 NTSTATUS;
typedef UINT64 ULONGLONG;
#endif // _NTDEF_

// EDK2-compatible CONTAINING_RECORD macro
#define CONTAINING_RECORD(address, type, field) \
    ((type *)((CHAR8 *)(address) - (UINTN)(&((type *)0)->field)))

// Define the MEMORY_DESCRIPTOR structure that winload uses (before conversion to kernel structures)
typedef enum _TYPE_OF_MEMORY
{
    LoaderExceptionBlock = 0,
    LoaderSystemBlock = 1,
    LoaderFree = 2,
    LoaderBad = 3,
    LoaderLoadedProgram = 4,
    LoaderFirmwareTemporary = 5,
    LoaderFirmwarePermanent = 6,
    LoaderOsloaderHeap = 7,
    LoaderOsloaderStack = 8,
    LoaderSystemCode = 9,
    LoaderHalCode = 10,
    LoaderBootDriver = 11,
    LoaderConsoleInDriver = 12,
    LoaderConsoleOutDriver = 13,
    LoaderStartupDpcStack = 14,
    LoaderStartupKernelStack = 15,
    LoaderStartupPanicStack = 16,
    LoaderStartupPcrPage = 17,
    LoaderStartupPdrPage = 18,
    LoaderRegistryData = 19,
    LoaderMemoryData = 20,
    LoaderNlsData = 21,
    LoaderSpecialMemory = 22,
    LoaderBBTMemory = 23,
    LoaderReserve = 24,
    LoaderXIPRom = 25,
    LoaderHALCachedMemory = 26,
    LoaderLargePageFiller = 27,
    LoaderErrorLogMemory = 28,
    LoaderMaximum = 29,
} TYPE_OF_MEMORY, * PTYPE_OF_MEMORY;

typedef struct _MEMORY_ALLOCATION_DESCRIPTOR
{
    /* 0x0000 */ struct _LIST_ENTRY ListEntry;
    /* 0x0010 */ enum _TYPE_OF_MEMORY MemoryType;
    /* 0x0018 */ unsigned __int64 BasePage;
    /* 0x0020 */ unsigned __int64 PageCount;
} MEMORY_ALLOCATION_DESCRIPTOR, * PMEMORY_ALLOCATION_DESCRIPTOR; /* size: 0x0028 */

// Correct typedef for the function we are hooking, OslpBuildKernelMemoryMap.
typedef NTSTATUS(EFIAPI* oslp_build_kernel_memory_map_t)(
    PVOID LoaderBlock,                          // PLOADER_PARAMETER_BLOCK
    PLIST_ENTRY LoaderMemoryMap,                // The list we need to modify
    ULONG KernelBufferSize,
    PVOID KernelMapBuffer,                      // PMEMORY_ALLOCATION_DESCRIPTOR
    PLIST_ENTRY FreeDescriptorsList,
    PULONG ReservedDescriptorsCount
    );

//====================================================================================
// DEBUG FILE LOGGING FUNCTIONALITY
//====================================================================================

EFI_STATUS WriteDebugFile(CHAR16 *Message) {
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_STATUS Status;
    UINTN BufferSize;
    CHAR8 AsciiBuffer[512];
    
    // Get ESP root directory
    Status = gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Open/create debug file
    Status = Root->Open(Root, &File, L"\debug.txt", 
                       EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    
    if (!EFI_ERROR(Status)) {
        // Seek to end
        File->SetPosition(File, 0xFFFFFFFFFFFFFFFF);
        
        // Convert and write
        UnicodeStrToAsciiStr(Message, AsciiBuffer);
        BufferSize = AsciiStrLen(AsciiBuffer);
        File->Write(File, &BufferSize, AsciiBuffer);
        File->Close(File);
    }
    
    Root->Close(Root);
    return Status;
}

//====================================================================================
// !! THE CORRECTED HOOK FUNCTION !!
//====================================================================================

NTSTATUS EFIAPI winload_oslp_build_kernel_memory_map_detour(
    PVOID LoaderBlock,
    PLIST_ENTRY LoaderMemoryMap,
    ULONG KernelBufferSize,
    PVOID KernelMapBuffer,
    PLIST_ENTRY FreeDescriptorsList,
    PULONG ReservedDescriptorsCount)
{
    // Note: We can't hook OslArchTransferToKernel from here since our UEFI app will be unloaded
    // The ntoskrnl base address is captured in winload_load_pe_image_detour instead

    PLIST_ENTRY list_head = LoaderMemoryMap;

    if (hyperv_attachment_heap_allocation_base == 0) {
  
    }
    else {
     
        if (list_head == NULL || list_head->ForwardLink == NULL || list_head->BackLink == NULL) {
        
        }
        else if (list_head->ForwardLink == list_head) {
      
        }
        else {
            PLIST_ENTRY current_entry = list_head->ForwardLink;
            BOOLEAN found = FALSE;

            while (current_entry != list_head) {
      
                PMEMORY_ALLOCATION_DESCRIPTOR descriptor = CONTAINING_RECORD(current_entry, MEMORY_ALLOCATION_DESCRIPTOR, ListEntry);

                PLIST_ENTRY next_entry = current_entry->ForwardLink;

                UINT64 physical_address = descriptor->BasePage << 12;
                if (descriptor->PageCount > 0x100000) { 
                 
                    current_entry = next_entry;
                    continue;
                }

                if (physical_address == hyperv_attachment_heap_allocation_base) {

                /*   PLIST_ENTRY prev = current_entry->BackLink;
                    PLIST_ENTRY next = current_entry->ForwardLink;
                    prev->ForwardLink = next;
                    next->BackLink = prev;*/

                    descriptor->MemoryType = 24;

                    found = TRUE;
                }

                current_entry = next_entry;
            }

            if (!found) {
          
            }
        }
    }


    hook_disable(&oslp_build_kernel_memory_map_hook_data);

    oslp_build_kernel_memory_map_t original_subroutine = (oslp_build_kernel_memory_map_t)oslp_build_kernel_memory_map_hook_data.hooked_subroutine_address;
    NTSTATUS status = original_subroutine(
        LoaderBlock,
        LoaderMemoryMap,
        KernelBufferSize,
        KernelMapBuffer,
        FreeDescriptorsList,
        ReservedDescriptorsCount
    );

    hook_enable(&oslp_build_kernel_memory_map_hook_data);

    return status;
}

UINT64 winload_load_pe_image_detour(bl_file_info_t* file_info, INT32 a2, UINT64* image_base, UINT32* image_size, UINT64* a5, UINT32* a6, UINT32* a7, UINT64 a8, UINT64 a9, unknown_param_t a10, unknown_param_t a11, unknown_param_t a12, unknown_param_t a13, unknown_param_t a14, unknown_param_t a15)
{
    hook_disable(&winload_load_pe_image_hook_data);

    boot_load_pe_image_t original_subroutine = (boot_load_pe_image_t)winload_load_pe_image_hook_data.hooked_subroutine_address;

    UINT64 return_value = original_subroutine(file_info, a2, image_base, image_size, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);

    // Check if this is ntoskrnl.exe being loaded
    if (file_info && file_info->file_name && 
        (StrStr(file_info->file_name, L"ntoskrnl.exe") != NULL ||
         StrStr(file_info->file_name, L"ntoskrnl") != NULL ||
         StrStr(file_info->file_name, L"NTOSKRNL.EXE") != NULL ||
         StrStr(file_info->file_name, L"NTOSKRNL") != NULL))
    {
        CHAR16 buffer[256];
        UnicodeSPrint(buffer, sizeof(buffer), L"[*] ntoskrnl.exe loaded at: 0x%llx\r\n", *image_base);
        WriteDebugFile(buffer);
        
        // Store the ntoskrnl base address for later use by the hypervisor
        g_ntoskrnl_base_address = *image_base;
        
        UnicodeSPrint(buffer, sizeof(buffer), L"[*] Stored ntoskrnl base address: 0x%llx for hypervisor\r\n", g_ntoskrnl_base_address);
        WriteDebugFile(buffer);
        
        // Now update the hypervisor with the ntoskrnl base address
        // Call the hypervisor to update its ntoskrnl base
        hvloader_update_ntoskrnl_base(g_ntoskrnl_base_address);
        
        WriteDebugFile(L"[*] Updated hypervisor with ntoskrnl base address\r\n");
        
        return return_value;
    }

    if (StrStr(file_info->file_name, L"hvloader") != NULL)
    {
        hvloader_place_hooks(*image_base, *image_size);
        return return_value;
    }

    hook_enable(&winload_load_pe_image_hook_data);

    return return_value;
}

EFI_STATUS winload_place_load_pe_image_hook(UINT64 image_base, UINT64 image_size)
{
    CHAR8* code_ref_to_load_pe_image = NULL;

    // ImgpLoadPEImage
    EFI_STATUS status = scan_image(&code_ref_to_load_pe_image, (CHAR8*)image_base, image_size, d_boot_load_pe_image_pattern, d_boot_load_pe_image_mask);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    CHAR8* load_pe_image_subroutine = (code_ref_to_load_pe_image + 10) + *(UINT32*)(code_ref_to_load_pe_image + 6);

    status = hook_create(&winload_load_pe_image_hook_data, load_pe_image_subroutine, (void*)winload_load_pe_image_detour);

    if (status != EFI_SUCCESS)
    {
        return status;
    }

    return hook_enable(&winload_load_pe_image_hook_data);
}

EFI_STATUS winload_place_oslp_build_kernel_memory_map_hook(UINT64 image_base, UINT64 image_size)
{
 //   DebugLogFormatted(L"[*] Starting hook placement for winload image at 0x%llx (size: 0x%llx)\n", image_base, image_size);
 //   DebugLog(L"[*] Searching for OslpBuildKernelMemoryMap function signature...\n");

    // Direct function signature for OslpBuildKernelMemoryMap
#define d_osl_build_kernel_map_pattern "\x48\x89\x5C\x24\x00\x48\x89\x54\x24\x00\x48\x89\x4C\x24\x00\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x00\x4C\x8B\x35"
#define d_osl_build_kernel_map_mask "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxx?xxx"

    CHAR8* oslp_build_kernel_map_subroutine = NULL;
    EFI_STATUS status = scan_image(&oslp_build_kernel_map_subroutine, (CHAR8*)image_base, image_size,
        (UINT8*)d_osl_build_kernel_map_pattern, (UINT8*)d_osl_build_kernel_map_mask);

    if (status != EFI_SUCCESS) {
       // DebugLogFormatted(L"[-] FAILED to find OslpBuildKernelMemoryMap function signature. Status: %r\n", status);
        return status;
    }

  //  DebugLogFormatted(L"[+] Found OslpBuildKernelMemoryMap function at: 0x%p (relative: +0x%llx)\n", 
  //      oslp_build_kernel_map_subroutine, (UINT64)oslp_build_kernel_map_subroutine - image_base);

    // Verify the found address is within the image bounds
    if ((UINT64)oslp_build_kernel_map_subroutine < image_base || 
        (UINT64)oslp_build_kernel_map_subroutine >= (image_base + image_size)) {
    //    DebugLog(L"[-] WARNING: Found function address is outside image bounds!\n");
    //    return EFI_NOT_FOUND;
    }

 //   DebugLogFormatted(L"[*] Creating hook for OslpBuildKernelMemoryMap at 0x%p...\n", oslp_build_kernel_map_subroutine);
    status = hook_create(&oslp_build_kernel_memory_map_hook_data, oslp_build_kernel_map_subroutine, (void*)winload_oslp_build_kernel_memory_map_detour);
    if (status != EFI_SUCCESS) {
   //     DebugLogFormatted(L"[-] FAILED to create hook. Status: %r\n", status);
        return status;
    }
 //   DebugLog(L"[+] Hook creation successful!\n");

//    DebugLog(L"[*] Enabling hook for OslpBuildKernelMemoryMap...\n");
    status = hook_enable(&oslp_build_kernel_memory_map_hook_data);
    if (status != EFI_SUCCESS) {
       // DebugLogFormatted(L"[-] FAILED to enable hook. Status: %r\n", status);
        return status;
    }
  //  DebugLog(L"[+] Hook enabled successfully!\n");
  //  DebugLog(L"[*] OslpBuildKernelMemoryMap hook setup complete. Waiting for function call...\n");
    
    return status;
}

// Global variable to store ntoskrnl base address
UINT64 g_ntoskrnl_base_address = 0;

// Note: Runtime hook functions removed - UEFI applications can't hook post-ExitBootServices functions
// ntoskrnl base address is now captured during PE loading instead

// Utility functions for module base extraction
UINT64 utils_get_module(UINT64 list_entry, const CHAR16* module)
{
    // Validate inputs
    if (!list_entry || !module)
        return 0;
    
    // Add bounds checking to prevent infinite loops    
    UINT32 max_iterations = 1000;
    UINT32 iteration_count = 0;
    
    UINT64 current_entry = *(UINT64*)list_entry;

    while (current_entry && current_entry != list_entry && iteration_count < max_iterations)
    {
        iteration_count++;
        
        // Validate current_entry pointer
        if (current_entry < 0x1000 || (current_entry & 0x7) != 0) // Basic sanity check
            break;
            
        UINT64 module_name_addr = *(UINT64*)((UINT8*)current_entry + 0x58 /* ->BaseDllName */ + 0x8 /* ->Buffer */);

        if (module_name_addr && module_name_addr > 0x1000)
        {
            // Use safer string comparison with length limit
            if (StrnCmp((CHAR16*)module_name_addr, module, 256) == 0)
            {
                return current_entry;
            }
        }

        current_entry = *(UINT64*)current_entry;
    }

    return 0;
}

UINT64 utils_get_module_base(UINT64 list_entry, const CHAR16* module)
{
    UINT64 current_entry = utils_get_module(list_entry, module);

    if (!current_entry)
    {
        return 0;
    }

    // Validate the module entry pointer before dereferencing
    if (current_entry < 0x1000 || (current_entry & 0x7) != 0)
    {
        return 0;
    }

    UINT64 module_base = *(UINT64*)((UINT8*)current_entry + 0x30 /* ->DllBase */);

    // Validate the module base address
    if (module_base < 0x1000)
    {
        return 0;
    }

    return module_base;
}

// Note: OslArchTransferToKernel hook removed - UEFI apps can't hook post-ExitBootServices functions
// We capture ntoskrnl base address during PE loading instead

// Note: OslArchTransferToKernel hook placement function removed - not feasible from UEFI app

EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size)
{
    WriteDebugFile(L"[*] === WINLOAD HOOKS INITIALIZATION ===\r\n");
    CHAR16 buffer[256];
    UnicodeSPrint(buffer, sizeof(buffer), L"[*] Winload image loaded at: 0x%llx\r\n", image_base);
    WriteDebugFile(buffer);
    UnicodeSPrint(buffer, sizeof(buffer), L"[*] Winload image size: 0x%llx\r\n", image_size);
    WriteDebugFile(buffer);
    
    EFI_STATUS status;

    WriteDebugFile(L"[*] Placing load PE image hook...\r\n");
    status = winload_place_load_pe_image_hook(image_base, image_size);
    if (status != EFI_SUCCESS) {
        UnicodeSPrint(buffer, sizeof(buffer), L"[-] Failed to place load PE image hook. Status: 0x%llx\r\n", status);
        WriteDebugFile(buffer);
    } else {
        WriteDebugFile(L"[+] Load PE image hook placed successfully!\r\n");
    }

    WriteDebugFile(L"[*] Placing OslpBuildKernelMemoryMap hook...\r\n");
    status = winload_place_oslp_build_kernel_memory_map_hook(image_base, image_size);
    if (status != EFI_SUCCESS) {
        UnicodeSPrint(buffer, sizeof(buffer), L"[-] Failed to place OslpBuildKernelMemoryMap hook. Status: 0x%llx\r\n", status);
        WriteDebugFile(buffer);
    } else {
        WriteDebugFile(L"[+] OslpBuildKernelMemoryMap hook placed successfully!\r\n");
    }

    // Note: OslArchTransferToKernel hook will be placed later when winload is fully loaded in memory
    WriteDebugFile(L"[*] Skipping OslArchTransferToKernel hook (will be placed later)\r\n");

    UnicodeSPrint(buffer, sizeof(buffer), L"[*] === WINLOAD HOOKS INITIALIZATION COMPLETE === Final Status: 0x%llx\r\n", status);
    WriteDebugFile(buffer);
    return status;
}
