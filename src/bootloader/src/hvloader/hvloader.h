#pragma once
#include <Library/UefiLib.h>

EFI_STATUS hvloader_place_hooks(UINT64 image_base, UINT64 image_size);
VOID hvloader_update_ntoskrnl_base(UINT64 ntoskrnl_base_address);
