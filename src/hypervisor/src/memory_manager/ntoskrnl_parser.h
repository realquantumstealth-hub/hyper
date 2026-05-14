#pragma once
#include <cstdint>

#include "memory_management.h"
#include <ia32-doc/ia32.hpp>

namespace ntoskrnl_parser
{
    // Structure to hold parsed export information
    struct export_info_t
    {
        PVOID address;
        DWORD ordinal;
        const char* name;
    };

    // Initialize the ntoskrnl.exe parser with base address from usermode
    NTSTATUS initialize(PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3);

    // Find an export by name
    PVOID get_export_by_name(const char* export_name, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3);

    // Find an export by ordinal
    PVOID get_export_by_ordinal(DWORD ordinal, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3);

    // Get cached ntoskrnl.exe base address
    PVOID get_ntoskrnl_base();

    // Dynamic ntoskrnl discovery functions
    std::uint64_t find_ntoskrnl_base_dynamically();
    bool validate_ntoskrnl_base(std::uint64_t base_addr, cr3 guest_cr3, cr3 slat_cr3);

    // Pattern search within ntoskrnl.exe
    PVOID find_pattern_in_ntoskrnl(const void* pattern, SIZE_T pattern_size, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3, SIZE_T max_search_size = 0x100000);

    // Utility function to search memory for patterns using guest memory access
    PVOID split_memory(PVOID search_base, SIZE_T search_size, const void* pattern, SIZE_T pattern_size, cr3 guest_cr3, cr3 slat_cr3);
}
