#include "image.h"
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS load_image(EFI_HANDLE* loaded_image_handle_out, BOOLEAN boot_policy, EFI_HANDLE parent_image_handle, EFI_DEVICE_PATH* device_path)
{
    return gBS->LoadImage(boot_policy, parent_image_handle, device_path, NULL, 0, loaded_image_handle_out);
}

EFI_STATUS unload_image(EFI_HANDLE image_handle)
{
    return gBS->UnloadImage(image_handle);
}

EFI_STATUS start_image(EFI_HANDLE image_handle)
{
    return gBS->StartImage(image_handle, NULL, NULL);
}

EFI_STATUS get_image_info(EFI_LOADED_IMAGE** image_info_out, EFI_HANDLE image_handle)
{
    EFI_GUID loaded_image_protocol_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

    return gBS->HandleProtocol(image_handle, &loaded_image_protocol_guid, image_info_out);
}

EFI_STATUS scan_image(CHAR8** location_out, CHAR8* scan_base, UINT64 scan_max_size, UINT8* pattern, UINT8* mask)
{
    if (location_out == NULL || scan_base == NULL || scan_max_size == 0 || pattern == NULL || mask == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    UINT64 mask_size = AsciiStrLen(mask);

    CHAR8* scan_limit = scan_base + scan_max_size - mask_size;

    for (CHAR8* current_byte = scan_base; current_byte <= scan_limit; current_byte++)
    {
        BOOLEAN was_pattern_found = 1;

        for (UINT64 i = 0; i < mask_size; i++)
        {
            CHAR8 current_mask_byte = mask[i];

            if (current_mask_byte == '?')
            {
                continue;
            }

            CHAR8 current_pattern_byte = pattern[i];

            if (current_pattern_byte != current_byte[i])
            {
                was_pattern_found = 0;

                break;
            }
        }

        if (was_pattern_found == 1)
        {
            *location_out = current_byte;

            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS scan_image_section(CHAR8** location_out, CHAR8* image_base, CHAR8* section_name, UINT8* pattern, UINT64 pattern_size)
{
    if (location_out == NULL || image_base == NULL || section_name == NULL || pattern == NULL || pattern_size == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get DOS header
    EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER*)image_base;
    if (dos_header->e_magic != EFI_IMAGE_DOS_SIGNATURE)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get PE header
    EFI_IMAGE_NT_HEADERS64* nt_header = (EFI_IMAGE_NT_HEADERS64*)(image_base + dos_header->e_lfanew);
    if (nt_header->Signature != EFI_IMAGE_NT_SIGNATURE)
    {
        return EFI_INVALID_PARAMETER;
    }

    // Get first section header
    EFI_IMAGE_SECTION_HEADER* section_header = (EFI_IMAGE_SECTION_HEADER*)((CHAR8*)nt_header + 
        sizeof(UINT32) + sizeof(EFI_IMAGE_FILE_HEADER) + nt_header->FileHeader.SizeOfOptionalHeader);

    BOOLEAN section_found = FALSE;
    // Search for the requested section
    for (UINT16 i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        // Compare section name (up to 8 characters)
        BOOLEAN name_match = TRUE;
        for (UINT32 j = 0; j < 8 && section_name[j] != 0; j++)
        {
            if (section_header[i].Name[j] != section_name[j])
            {
                name_match = FALSE;
                break;
            }
        }

        if (name_match)
        {
            section_found = TRUE;
            // Found the section, now scan it for the pattern
            CHAR8* section_start = image_base + section_header[i].VirtualAddress;
            UINT32 section_size = section_header[i].Misc.VirtualSize;

            // Validate section bounds
            if (section_size == 0 || section_start < image_base)
            {
                return EFI_NOT_FOUND;
            }

            // Scan the section for the pattern
            for (UINT32 offset = 0; offset <= section_size - pattern_size; offset++)
            {
                BOOLEAN pattern_match = TRUE;
                for (UINT64 k = 0; k < pattern_size; k++)
                {
                    if (section_start[offset + k] != pattern[k])
                    {
                        pattern_match = FALSE;
                        break;
                    }
                }

                if (pattern_match)
                {
                    *location_out = section_start + offset;
                    return EFI_SUCCESS;
                }
            }

            // Pattern not found in this section
            return EFI_NOT_FOUND;
        }
    }

    // Section not found
    return section_found ? EFI_NOT_FOUND : EFI_NOT_FOUND;
}

EFI_IMAGE_NT_HEADERS64* image_get_nt_headers(UINT8* image_base)
{
    EFI_IMAGE_DOS_HEADER* dos_header = (EFI_IMAGE_DOS_HEADER*)image_base;

    EFI_IMAGE_NT_HEADERS64* nt_headers = (EFI_IMAGE_NT_HEADERS64*)(image_base + dos_header->e_lfanew);

    return nt_headers;
}
