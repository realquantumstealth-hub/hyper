#pragma once
#include <Library/UefiLib.h>
#include <ia32-doc/ia32_compact.h>
#include "../hvloader/hvloader.h"

extern UINT64 pml4_physical_allocation;
extern UINT64 pdpt_physical_allocation;

// Global variable to store ntoskrnl base address
extern UINT64 g_ntoskrnl_base_address;

typedef enum _BL_MEMORY_TYPE {
	BlLoaderBadMemory = 6,
} BL_MEMORY_TYPE;

typedef struct _LIST_ENTRY LIST_ENTRY, * PLIST_ENTRY;

typedef struct _KLDR_MEMORY_BLOCK {
	LIST_ENTRY      ListEntry;
	UINT32          Unknown1;
	UINT32          Unknown2;
	UINT64          Unknown3;
	UINT32          Type;
	UINT32          Unknown4;
	UINT64          BasePage;
	UINT64          PageCount;
} KLDR_MEMORY_BLOCK, * PKLDR_MEMORY_BLOCK;


EFI_STATUS winload_place_hooks(UINT64 image_base, UINT64 image_size);

// Debug logging function declarations
EFI_STATUS WriteDebugFile(CHAR16 *Message);
EFI_STATUS WriteDebugFileFormatted(CHAR16 *Format, ...);
VOID DebugLog(CHAR16 *Message);
VOID DebugLogFormatted(CHAR16 *Format, ...);
