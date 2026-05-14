#include "memory_management.h"
#include "ntoskrnl_parser.h"
#include "../crt/crt.h"
#include "../logs/logs.h"
#include "../memory_manager/memory_manager.h"
#include "../slat/slat.h"
#include "../arch/arch.h"
#include "../hypercall/hypercall.h"

// Access to global variables defined in main.cpp
extern std::uint64_t g_ntoskrnl_base_address;
extern std::uint64_t g_hyperv_attachment_physical_base;
extern std::uint64_t g_hyperv_attachment_page_count;
extern std::uint64_t heap_physical_initial_base;
extern std::uint64_t heap_total_size_to_hide;
extern std::uint64_t uefi_boot_physical_base_address;
extern std::uint64_t uefi_boot_image_size;

namespace memory_management
{
    // Global variables
    PVOID g_mmonp_MmPfnDatabase = nullptr;
    MmGetVirtualForPhysical_t g_MmGetVirtualForPhysical = nullptr;
    MmGetPhysicalMemoryRanges_t g_MmGetPhysicalMemoryRanges = nullptr;
    bool g_initialized = false;
    cr3 g_kernel_cr3 = {}; // Kernel CR3 for reading kernel virtual addresses

    // Auto-initialization function that uses the global ntoskrnl base
    NTSTATUS auto_initialize_kernel_functions()
    {
        if (g_initialized)
            return STATUS_SUCCESS;
            
        if (!g_ntoskrnl_base_address)
        {
            // ntoskrnl base address not captured during boot
            return STATUS_INVALID_PARAMETER;
        }
        
        return initialize_kernel_functions(reinterpret_cast<PVOID>(g_ntoskrnl_base_address));
    }

    NTSTATUS initialize_kernel_functions(PVOID ntoskrnl_base)
    {
        if (g_initialized)
            return STATUS_SUCCESS;

        if (!ntoskrnl_base)
        {
            // Invalid ntoskrnl base address provided
            return STATUS_INVALID_PARAMETER;
        }

        // Get CR3s for memory operations
        // Use current guest CR3 - kernel memory is mapped in all processes on Windows
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        // Initialize ntoskrnl parser using guest CR3
        NTSTATUS status = ntoskrnl_parser::initialize(ntoskrnl_base, guest_cr3, slat_cr3);
        if (!NT_SUCCESS(status))
        {
            // Failed to initialize ntoskrnl parser
            return status;
        }

        // Get MmGetVirtualForPhysical function using guest CR3
        g_MmGetVirtualForPhysical = reinterpret_cast<MmGetVirtualForPhysical_t>(
            ntoskrnl_parser::get_export_by_name("MmGetVirtualForPhysical", ntoskrnl_base, guest_cr3, slat_cr3));

        if (!g_MmGetVirtualForPhysical)
        {
            // Failed to find MmGetVirtualForPhysical export
            return STATUS_PROCEDURE_NOT_FOUND;
        }

        // Get MmGetPhysicalMemoryRanges function using guest CR3
        g_MmGetPhysicalMemoryRanges = reinterpret_cast<MmGetPhysicalMemoryRanges_t>(
            ntoskrnl_parser::get_export_by_name("MmGetPhysicalMemoryRanges", ntoskrnl_base, guest_cr3, slat_cr3));

        if (!g_MmGetPhysicalMemoryRanges)
        {
            // Failed to find MmGetPhysicalMemoryRanges export
            return STATUS_PROCEDURE_NOT_FOUND;
        }

        // Successfully resolved kernel functions

        // Initialize MmPfnDatabase
        status = initialize_mmpfn_database(ntoskrnl_base);
        if (!NT_SUCCESS(status))
        {
            // Failed to initialize MmPfnDatabase
            return status;
        }

        g_initialized = true;
        return STATUS_SUCCESS;
    }

    NTSTATUS initialize_mmpfn_database(PVOID ntoskrnl_base)
    {
        cr3 slat_cr3 = slat::get_cr3();

        // Use current guest CR3 - kernel memory is mapped in all processes on Windows
        cr3 guest_cr3 = arch::get_guest_cr3();

        struct MmPfnDatabaseSearchPattern
        {
            const UCHAR* bytes;
            SIZE_T bytes_size;
            bool hard_coded;
        };

        MmPfnDatabaseSearchPattern patterns;

        // Windows 10 x64 Build 14332+
        static const UCHAR kPatternWin10x64[] = {
            0x48, 0x8B, 0xC1,        // mov     rax, rcx
            0x48, 0xC1, 0xE8, 0x0C,  // shr     rax, 0Ch
            0x48, 0x8D, 0x14, 0x40,  // lea     rdx, [rax + rax * 2]
            0x48, 0x03, 0xD2,        // add     rdx, rdx
            0x48, 0xB8,              // mov     rax, 0FFFFFA8000000008h
        };

        patterns.bytes = kPatternWin10x64;
        patterns.bytes_size = sizeof(kPatternWin10x64);
        patterns.hard_coded = true;

        const auto p_MmGetVirtualForPhysical = reinterpret_cast<UCHAR*>(g_MmGetVirtualForPhysical);

        if (!p_MmGetVirtualForPhysical) {
            // p_MmGetVirtualForPhysical not found
            return STATUS_PROCEDURE_NOT_FOUND;
        }

        auto found = reinterpret_cast<UCHAR*>(ntoskrnl_parser::split_memory(
            p_MmGetVirtualForPhysical, 0x200, patterns.bytes, patterns.bytes_size, guest_cr3, slat_cr3));

        if (!found) {
            // MmPfnDatabase pattern not found
            return STATUS_UNSUCCESSFUL;
        }

        found += patterns.bytes_size;

        // Read the MmPfnDatabase pointer from kernel memory using guest CR3
        void* database_ptr;
        if (patterns.hard_coded) {
            if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &database_ptr,
                reinterpret_cast<std::uint64_t>(found), guest_cr3, sizeof(database_ptr), memory_operation_t::read_operation))
            {
                // Failed to read MmPfnDatabase pointer
                return STATUS_UNSUCCESSFUL;
            }
            g_mmonp_MmPfnDatabase = database_ptr;
        }
        else {
            ULONG_PTR mmpfn_address;
            if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &mmpfn_address,
                reinterpret_cast<std::uint64_t>(found), guest_cr3, sizeof(mmpfn_address), memory_operation_t::read_operation))
            {
                // Failed to read MmPfnDatabase address
                return STATUS_UNSUCCESSFUL;
            }

            if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &database_ptr,
                mmpfn_address, guest_cr3, sizeof(database_ptr), memory_operation_t::read_operation))
            {
                // Failed to read MmPfnDatabase from address
                return STATUS_UNSUCCESSFUL;
            }
            g_mmonp_MmPfnDatabase = database_ptr;
        }

        g_mmonp_MmPfnDatabase = PAGE_ALIGN(g_mmonp_MmPfnDatabase);

        // Successfully initialized MmPfnDatabase
        return STATUS_SUCCESS;
    }

    NTSTATUS read_physical_memory(PVOID physical_address, PVOID buffer, SIZE_T size, SIZE_T* bytes_read)
    {
        if (!buffer || !physical_address || size == 0)
            return STATUS_INVALID_PARAMETER;

        if (bytes_read)
            *bytes_read = 0;

        cr3 slat_cr3 = slat::get_cr3();
        std::uint64_t guest_physical_address = reinterpret_cast<std::uint64_t>(physical_address);
        
        SIZE_T bytes_copied = 0;
        SIZE_T remaining_size = size;
        UCHAR* dest_buffer = reinterpret_cast<UCHAR*>(buffer);

        while (remaining_size > 0)
        {
            // Map the guest physical address to host virtual address
            std::uint64_t size_left_of_ept_page = 0;
            std::uint64_t host_virtual_address = memory_manager::map_guest_physical(
                slat_cr3, guest_physical_address + bytes_copied, &size_left_of_ept_page);
            
            if (host_virtual_address == 0)
            {
                // Failed to map guest physical address
                break;
            }

            // Calculate how much we can copy from this page
            SIZE_T copy_size = crt::min(remaining_size, size_left_of_ept_page);
            
            // Copy the data
            crt::copy_memory(dest_buffer + bytes_copied, 
                           reinterpret_cast<void*>(host_virtual_address), 
                           copy_size);

            bytes_copied += copy_size;
            remaining_size -= copy_size;
        }

        if (bytes_read)
            *bytes_read = bytes_copied;

        return (bytes_copied == size) ? STATUS_SUCCESS : STATUS_PARTIAL_COPY;
    }

    std::uint64_t dirbase_from_base_address(void* base, PVOID ntoskrnl_base)
    {
        if (!base)
            return 0;

        // Auto-initialize only once
        if (!g_initialized)
        {
            NTSTATUS status = auto_initialize_kernel_functions();
            if (!NT_SUCCESS(status))
                return 0;
        }

        // Ensure MmPfnDatabase is available
        if (!g_mmonp_MmPfnDatabase)
            return 0;

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        virt_addr_t virt_base{};
        virt_base.value = base;

        // Scan physical memory using MmPfnDatabase to find page directories
        // We scan a reasonable range (first 16GB of physical memory)
        const std::uint64_t max_physical_address = 0x400000000ULL; // 16GB
        const std::uint64_t max_pages_to_scan = 10000; // Safety limit
        std::uint64_t pages_scanned = 0;

        for (std::uint64_t current_phys = 0x1000; current_phys < max_physical_address && pages_scanned < max_pages_to_scan; current_phys += 0x1000)
        {
            std::uint64_t pfn = current_phys >> 12;

            // Get PFN entry from MmPfnDatabase
            std::uint64_t pfn_entry_address = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase) + (pfn * sizeof(MMPFN));

            MMPFN pfn_entry = {};
            if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
                pfn_entry_address, guest_cr3, sizeof(MMPFN), memory_operation_t::read_operation))
                continue;

            pages_scanned++;

            // Check if this is a page table: PteFrame should equal its own PFN
            if (pfn_entry.u4.PteFrame != pfn)
                continue;

            // Check if page exists and is valid
            if (!pfn_entry.u4.PfnExists)
                continue;

            // This looks like a page directory - test if it can translate our virtual address
            if (test_virtual_address_in_cr3(base, current_phys, slat_cr3))
            {
                return current_phys; // Found the CR3!
            }
        }

        return 0; // CR3 not found
    }

    // Helper function to test if a virtual address is mapped in a specific CR3
    bool test_virtual_address_in_cr3(void* virtual_address, std::uint64_t test_cr3, cr3 slat_cr3)
    {
        if (!virtual_address || test_cr3 == 0)
            return false;

        virt_addr_t virt_addr{};
        virt_addr.value = virtual_address;
        
        size_t bytes_read = 0;
        
        // Read PML4E
        MMPTE pml4e{};
        std::uint64_t pml4_address = (test_cr3 & ~0xFFF) + (virt_addr.pml4_index * 8);
        if (!NT_SUCCESS(read_physical_memory(PVOID(pml4_address), &pml4e, sizeof(pml4e), &bytes_read)) || 
            bytes_read != sizeof(pml4e))
            return false;
            
        if (!pml4e.u.Hard.Valid)
            return false;

        // Read PDPTE
        MMPTE pdpte{};
        std::uint64_t pdpt_address = (pml4e.u.Hard.PageFrameNumber << 12) + (virt_addr.pdpt_index * 8);
        if (!NT_SUCCESS(read_physical_memory(PVOID(pdpt_address), &pdpte, sizeof(pdpte), &bytes_read)) || 
            bytes_read != sizeof(pdpte))
            return false;
            
        if (!pdpte.u.Hard.Valid)
            return false;

        // Check for 1GB page
        if (pdpte.u.Hard.LargePage)
        {
            return true; // Found valid 1GB page mapping
        }

        // Read PDE
        MMPTE pde{};
        std::uint64_t pd_address = (pdpte.u.Hard.PageFrameNumber << 12) + (virt_addr.pd_index * 8);
        if (!NT_SUCCESS(read_physical_memory(PVOID(pd_address), &pde, sizeof(pde), &bytes_read)) || 
            bytes_read != sizeof(pde))
            return false;
            
        if (!pde.u.Hard.Valid)
            return false;

        // Check for 2MB page
        if (pde.u.Hard.LargePage)
        {
            return true; // Found valid 2MB page mapping
        }

        // Read PTE
        MMPTE pte{};
        std::uint64_t pt_address = (pde.u.Hard.PageFrameNumber << 12) + (virt_addr.pt_index * 8);
        if (!NT_SUCCESS(read_physical_memory(PVOID(pt_address), &pte, sizeof(pte), &bytes_read)) || 
            bytes_read != sizeof(pte))
            return false;
            
        return pte.u.Hard.Valid; // Return true if final PTE is valid
    }

    // Fallback method: try to find CR3 by scanning common page directory locations
    std::uint64_t find_cr3_by_page_table_walk(void* virtual_address, cr3 guest_cr3, cr3 slat_cr3)
    {
        if (!virtual_address)
            return 0;

        // Try some common CR3 locations based on typical Windows memory layout
        std::uint64_t candidate_cr3_list[] = {
            guest_cr3.flags,  // Current guest CR3 as fallback
            0x1AA000,         // Common system CR3
            0x187000,         // Another common system CR3
            0x1AB000,         // Common kernel CR3
            0x1A9000,         // Alternative kernel CR3
        };

        for (size_t i = 0; i < sizeof(candidate_cr3_list) / sizeof(candidate_cr3_list[0]); i++)
        {
            std::uint64_t candidate_cr3 = candidate_cr3_list[i];
            if (candidate_cr3 != 0 && (candidate_cr3 & 0xFFF) == 0) // Must be page-aligned
            {
                if (test_virtual_address_in_cr3(virtual_address, candidate_cr3, slat_cr3))
                {
                    return candidate_cr3;
                }
            }
        }

        // If all else fails, try scanning some physical memory for valid page directories
        // This is a last resort and should be used sparingly
        for (std::uint64_t phys_addr = 0x100000; phys_addr < 0x10000000; phys_addr += 0x1000)
        {
            // Skip if not page-aligned or in reserved ranges
            if ((phys_addr & 0xFFF) != 0 || phys_addr < 0x1000)
                continue;
                
            // Quick sanity check: read first few entries of potential page directory
            MMPTE test_entries[4];
            size_t bytes_read = 0;
            if (NT_SUCCESS(read_physical_memory(PVOID(phys_addr), test_entries, sizeof(test_entries), &bytes_read)) &&
                bytes_read == sizeof(test_entries))
            {
                // Check if this looks like a valid page directory
                int valid_entries = 0;
                for (int j = 0; j < 4; j++)
                {
                    if (test_entries[j].u.Hard.Valid && 
                        test_entries[j].u.Hard.PageFrameNumber != 0 &&
                        test_entries[j].u.Hard.PageFrameNumber < 0x100000) // Reasonable PFN
                    {
                        valid_entries++;
                    }
                }
                
                // If we found some valid-looking entries, test this as a CR3
                if (valid_entries >= 1)
                {
                    if (test_virtual_address_in_cr3(virtual_address, phys_addr, slat_cr3))
                    {
                        return phys_addr;
                    }
                }
            }
            
            // Don't scan too much to avoid performance issues
            static int scan_count = 0;
            if (++scan_count > 1000)
                break;
        }

        return 0; // Failed to find CR3
    }

    // Utility functions for pattern searching and PE parsing
    PVOID find_pattern_in_memory(PVOID search_base, SIZE_T search_size, const void* pattern, SIZE_T pattern_size)
    {
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();
        return ntoskrnl_parser::split_memory(search_base, search_size, pattern, pattern_size, guest_cr3, slat_cr3);
    }

    PVOID get_module_base(const char* module_name)
    {
        // This would typically walk the loaded module list
        // For now, if it's ntoskrnl, return the captured base
        if (compare_strings(module_name, "ntoskrnl.exe") == 0)
        {
            // Use captured ntoskrnl base address if available
            if (g_ntoskrnl_base_address)
                return reinterpret_cast<PVOID>(g_ntoskrnl_base_address);
            
            // Fallback to parser cache
            return ntoskrnl_parser::get_ntoskrnl_base();
        }
        return nullptr;
    }

    PVOID get_procedure_address(PVOID module_base, const char* procedure_name)
    {
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();
        
        // For ntoskrnl.exe, use our parser with captured base
        PVOID ntoskrnl_base = g_ntoskrnl_base_address ? 
            reinterpret_cast<PVOID>(g_ntoskrnl_base_address) : 
            ntoskrnl_parser::get_ntoskrnl_base();
            
        if (module_base == ntoskrnl_base || module_base == nullptr)
        {
            return ntoskrnl_parser::get_export_by_name(procedure_name, ntoskrnl_base, guest_cr3, slat_cr3);
        }
        return nullptr;
    }

    PVOID parse_imports_and_find_function(PVOID module_base, const char* function_name)
    {
        // Simplified implementation
        return get_procedure_address(module_base, function_name);
    }

    NTSTATUS find_mmpfn_database_address(PVOID ntoskrnl_base, PVOID* mmpfn_database_out)
    {
        if (!ntoskrnl_base || !mmpfn_database_out)
            return STATUS_INVALID_PARAMETER;

        *mmpfn_database_out = nullptr;

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        // Pattern for MmPfnDatabase access in MmGetVirtualForPhysical
        // This pattern is more reliable than the previous one
        static const UCHAR kMmPfnDatabasePattern[] = {
            0x48, 0x8B, 0xC1,        // mov rax, rcx
            0x48, 0xC1, 0xE8, 0x0C,  // shr rax, 0Ch (divide by page size)
            0x48, 0x8D, 0x14, 0x40,  // lea rdx, [rax + rax*2] (multiply by 3)
            0x48, 0x03, 0xD2,        // add rdx, rdx (multiply by 2, total *6)
            0x48, 0xB8               // mov rax, immediate64 (MmPfnDatabase)
        };

        // Find MmGetVirtualForPhysical export first using guest CR3
        PVOID mm_get_virtual_for_physical = ntoskrnl_parser::get_export_by_name(
            "MmGetVirtualForPhysical", ntoskrnl_base, guest_cr3, slat_cr3);

        if (!mm_get_virtual_for_physical)
            return STATUS_PROCEDURE_NOT_FOUND;

        // Search for the pattern within the function
        PVOID pattern_location = ntoskrnl_parser::split_memory(
            mm_get_virtual_for_physical, 0x100, kMmPfnDatabasePattern, 
            sizeof(kMmPfnDatabasePattern), guest_cr3, slat_cr3);

        if (!pattern_location)
            return STATUS_NOT_FOUND;

        // The MmPfnDatabase address follows immediately after the pattern
        UCHAR* pattern_ptr = reinterpret_cast<UCHAR*>(pattern_location);
        PVOID* database_ptr_location = reinterpret_cast<PVOID*>(pattern_ptr + sizeof(kMmPfnDatabasePattern));

        // Read the MmPfnDatabase address from guest memory
        PVOID database_address;
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &database_address,
            reinterpret_cast<std::uint64_t>(database_ptr_location), guest_cr3, 
            sizeof(database_address), memory_operation_t::read_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        *mmpfn_database_out = database_address;
        return STATUS_SUCCESS;
    }

    NTSTATUS hide_physical_page_from_pfn_database(std::uint64_t physical_address)
    {
        if (!g_mmonp_MmPfnDatabase)
            return STATUS_NOT_FOUND;

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        // Calculate PFN index (physical address / page size)
        std::uint64_t pfn_index = physical_address >> 12;
        
        // Calculate PFN entry address in MmPfnDatabase
        std::uint64_t pfn_entry_address = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase) + 
                                         (pfn_index * sizeof(_MMPFN));

        // Read the current PFN entry
        _MMPFN pfn_entry;
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
            pfn_entry_address, guest_cr3, sizeof(pfn_entry), memory_operation_t::read_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        // Store original PFN entry for potential restoration
        // For now, we'll modify it to appear as a free/bad page

        // Method 1: Mark as bad page
        pfn_entry.u3.e1.PageLocation = 6; // BadPageList
        pfn_entry.u4.PfnExists = 0;       // Mark as non-existent
        pfn_entry.u3.e4.EntireField = 0;  // Clear reference count

        // Method 2: Unlink from any lists by zeroing list entries
        crt::set_memory(&pfn_entry.ListEntry, 0, sizeof(pfn_entry.ListEntry));
        crt::set_memory(&pfn_entry.u1, 0, sizeof(pfn_entry.u1));

        // Write the modified PFN entry back
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
            pfn_entry_address, guest_cr3, sizeof(pfn_entry), memory_operation_t::write_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS restore_physical_page_in_pfn_database(std::uint64_t physical_address)
    {
        // This would restore a previously hidden page
        // For now, we'll implement a basic restoration that marks the page as active
        
        if (!g_mmonp_MmPfnDatabase)
            return STATUS_NOT_FOUND;

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        std::uint64_t pfn_index = physical_address >> 12;
        std::uint64_t pfn_entry_address = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase) + 
                                         (pfn_index * sizeof(_MMPFN));

        _MMPFN pfn_entry;
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
            pfn_entry_address, guest_cr3, sizeof(pfn_entry), memory_operation_t::read_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        // Restore to a reasonable active state
        pfn_entry.u3.e1.PageLocation = 2; // ActiveAndValid
        pfn_entry.u4.PfnExists = 1;
        pfn_entry.u4.PteFrame = pfn_index;
        pfn_entry.u3.e2.ReferenceCount = 1;

        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
            pfn_entry_address, guest_cr3, sizeof(pfn_entry), memory_operation_t::write_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS unlink_hypervisor_memory_from_pfn_database(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes)
    {
        if (!g_mmonp_MmPfnDatabase)
        {
            // Try to initialize if not already done
            NTSTATUS status = initialize_mmpfn_database(ntoskrnl_parser::get_ntoskrnl_base());
            if (!NT_SUCCESS(status))
                return status;
        }

        // Calculate the number of pages to hide
        std::uint64_t page_count = (hypervisor_size_bytes + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;
        std::uint64_t current_physical = hypervisor_base_physical;

        NTSTATUS overall_status = STATUS_SUCCESS;

        // Hide each page of the hypervisor memory
        for (std::uint64_t i = 0; i < page_count; i++)
        {
            NTSTATUS status = hide_physical_page_from_pfn_database(current_physical);
            if (!NT_SUCCESS(status))
            {
                // Continue trying to hide other pages even if one fails
                overall_status = status;
            }
            current_physical += PAGE_SIZE_4KB;
        }

        return overall_status;
    }

    // Get PFN information for a specific physical address
    NTSTATUS get_pfn_information(std::uint64_t physical_address, _MMPFN* pfn_info_out)
    {
        if (!pfn_info_out || !g_mmonp_MmPfnDatabase)
            return STATUS_INVALID_PARAMETER;

        if (!g_initialized)
        {
            // Try to auto-initialize using captured ntoskrnl base
            NTSTATUS status = auto_initialize_kernel_functions();
            if (!NT_SUCCESS(status))
                return status;
        }

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        // Calculate PFN index (physical address / page size)
        std::uint64_t pfn_index = physical_address >> 12;
        
        // Calculate PFN entry address in MmPfnDatabase
        std::uint64_t pfn_entry_address = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase) + 
                                         (pfn_index * sizeof(_MMPFN));

        // Read the PFN entry from guest memory
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, pfn_info_out,
            pfn_entry_address, guest_cr3, sizeof(_MMPFN), memory_operation_t::read_operation))
        {
            return STATUS_UNSUCCESSFUL;
        }

        return STATUS_SUCCESS;
    }

    // Get detailed page information from MmPfnDatabase
    NTSTATUS get_page_information(std::uint64_t physical_address, page_info_t* page_info_out)
    {
        if (!page_info_out || physical_address == 0)
            return STATUS_INVALID_PARAMETER;

        crt::set_memory(page_info_out, 0, sizeof(page_info_t));

        _MMPFN pfn_entry;
        NTSTATUS status = get_pfn_information(physical_address, &pfn_entry);
        if (!NT_SUCCESS(status))
            return status;

        // Fill in the page information structure
        page_info_out->physical_address = physical_address;
        page_info_out->pfn_number = physical_address >> 12;
        
        // Extract information from PFN entry
        page_info_out->reference_count = pfn_entry.u3.e2.ReferenceCount;
        page_info_out->page_location = pfn_entry.u3.e1.PageLocation;
        page_info_out->write_in_progress = pfn_entry.u3.e1.WriteInProgress;
        page_info_out->modified = pfn_entry.u3.e1.Modified;
        page_info_out->read_in_progress = pfn_entry.u3.e1.ReadInProgress;
        page_info_out->cache_attribute = pfn_entry.u3.e1.CacheAttribute;
        
        page_info_out->pte_frame = pfn_entry.u4.PteFrame;
        page_info_out->resident_page = pfn_entry.u4.ResidentPage;
        page_info_out->pfn_exists = pfn_entry.u4.PfnExists;
        page_info_out->file_only = pfn_entry.u4.FileOnly;
        page_info_out->prototype_pte = pfn_entry.u4.PrototypePte;
        
        // Get PTE address if available
        if (pfn_entry.PteAddress)
        {
            page_info_out->pte_address = reinterpret_cast<std::uint64_t>(pfn_entry.PteAddress);
            page_info_out->has_pte_address = true;
        }
        else
        {
            page_info_out->pte_address = 0;
            page_info_out->has_pte_address = false;
        }

        // Interpret page location
        switch (page_info_out->page_location)
        {
        case 0: page_info_out->location_name = "ZeroedPageList"; break;
        case 1: page_info_out->location_name = "FreePageList"; break;
        case 2: page_info_out->location_name = "StandbyPageList"; break;
        case 3: page_info_out->location_name = "ModifiedPageList"; break;
        case 4: page_info_out->location_name = "ModifiedNoWritePageList"; break;
        case 5: page_info_out->location_name = "BadPageList"; break;
        case 6: page_info_out->location_name = "ActiveAndValid"; break;
        case 7: page_info_out->location_name = "TransitionPage"; break;
        default: page_info_out->location_name = "Unknown"; break;
        }

        return STATUS_SUCCESS;
    }

    // Enumerate physical pages and their properties
    NTSTATUS enumerate_physical_pages(std::uint64_t start_physical_address, 
                                    std::uint64_t end_physical_address, 
                                    page_enumeration_callback_t callback, 
                                    void* context)
    {
        if (!callback || start_physical_address >= end_physical_address)
            return STATUS_INVALID_PARAMETER;

        if (!g_initialized)
        {
            NTSTATUS status = auto_initialize_kernel_functions();
            if (!NT_SUCCESS(status))
                return status;
        }

        // Align addresses to page boundaries
        std::uint64_t current_address = start_physical_address & ~(PAGE_SIZE_4KB - 1);
        std::uint64_t end_address = end_physical_address & ~(PAGE_SIZE_4KB - 1);

        NTSTATUS overall_status = STATUS_SUCCESS;
        std::uint64_t pages_processed = 0;
        const std::uint64_t max_pages = 10000; // Safety limit

        while (current_address <= end_address && pages_processed < max_pages)
        {
            page_info_t page_info;
            NTSTATUS status = get_page_information(current_address, &page_info);
            
            if (NT_SUCCESS(status))
            {
                // Call the user-provided callback
                if (!callback(&page_info, context))
                {
                    // Callback requested to stop enumeration
                    break;
                }
            }
            else
            {
                // Continue with next page even if current one failed
                overall_status = status;
            }

            current_address += PAGE_SIZE_4KB;
            pages_processed++;
        }

        return overall_status;
    }

    // Check if a PFN entry is valid and accessible
    bool is_pfn_valid(std::uint64_t physical_address)
    {
        _MMPFN pfn_entry;
        NTSTATUS status = get_pfn_information(physical_address, &pfn_entry);
        
        if (!NT_SUCCESS(status))
            return false;

        // Check if PFN exists and is in a valid state
        return (pfn_entry.u4.PfnExists == 1) && 
               (pfn_entry.u3.e1.PageLocation < 8) && // Valid page location
               (pfn_entry.u3.e2.ReferenceCount > 0 || pfn_entry.u3.e1.PageLocation <= 1); // Either referenced or free/zeroed
    }

    // Get physical memory statistics from MmPfnDatabase
    NTSTATUS get_memory_statistics(memory_statistics_t* stats_out)
    {
        if (!stats_out)
            return STATUS_INVALID_PARAMETER;

        crt::set_memory(stats_out, 0, sizeof(memory_statistics_t));

        if (!g_initialized)
        {
            NTSTATUS status = auto_initialize_kernel_functions();
            if (!NT_SUCCESS(status))
                return status;
        }

        // Enumerate a sample of physical memory to get statistics
        // We'll check the first 1GB to get a representative sample
        std::uint64_t sample_end = 0x40000000; // 1GB
        std::uint64_t pages_checked = 0;
        const std::uint64_t max_sample_pages = 1000; // Limit sample size for performance

        for (std::uint64_t phys_addr = 0x1000; phys_addr < sample_end && pages_checked < max_sample_pages; phys_addr += PAGE_SIZE_4KB)
        {
            _MMPFN pfn_entry;
            if (NT_SUCCESS(get_pfn_information(phys_addr, &pfn_entry)))
            {
                pages_checked++;
                
                if (pfn_entry.u4.PfnExists)
                {
                    stats_out->total_pages++;
                    
                    switch (pfn_entry.u3.e1.PageLocation)
                    {
                    case 0: stats_out->zeroed_pages++; break;
                    case 1: stats_out->free_pages++; break;
                    case 2: stats_out->standby_pages++; break;
                    case 3: stats_out->modified_pages++; break;
                    case 4: stats_out->modified_no_write_pages++; break;
                    case 5: stats_out->bad_pages++; break;
                    case 6: stats_out->active_pages++; break;
                    case 7: stats_out->transition_pages++; break;
                    }
                    
                    if (pfn_entry.u3.e2.ReferenceCount > 0)
                        stats_out->referenced_pages++;
                    
                    if (pfn_entry.u3.e1.Modified)
                        stats_out->dirty_pages++;
                }
            }
        }

        stats_out->sample_size = pages_checked;
        return STATUS_SUCCESS;
    }

    // Detailed hypervisor PFN query function
    NTSTATUS query_hypervisor_pfn_detailed(std::uint64_t physical_address, HYPERVISOR_PFN_INFO* pfn_info_out)
    {
        if (!pfn_info_out)
            return STATUS_INVALID_PARAMETER;

        // Zero out the structure first
        crt::set_memory(pfn_info_out, 0, sizeof(HYPERVISOR_PFN_INFO));

        pfn_info_out->physical_address = physical_address;
        pfn_info_out->pfn_index = physical_address >> 12;

        trap_frame_log_t log = {};
        log.r15 = 0xF000; // PFN query start
        log.r14 = physical_address;
        log.r13 = pfn_info_out->pfn_index;
        logs::add_log(log);

        // Get the ntoskrnl base using KPCR method if not already cached
        if (g_ntoskrnl_base_address == 0)
        {
            log = {};
            log.r15 = 0xF001; // Need to discover ntoskrnl
            logs::add_log(log);

            std::uint64_t ntoskrnl_base = get_ntoskrnl_from_kpcr();
            if (ntoskrnl_base == 0)
            {
                log = {};
                log.r15 = 0xF0E1; // Failed to find ntoskrnl
                logs::add_log(log);

                pfn_info_out->query_status = STATUS_NOT_FOUND;
                return STATUS_NOT_FOUND;
            }

            g_ntoskrnl_base_address = ntoskrnl_base;

            log = {};
            log.r15 = 0xF002; // Found ntoskrnl
            log.r14 = ntoskrnl_base;
            logs::add_log(log);
        }

        // Auto-initialize if needed
        if (!g_initialized)
        {
            log = {};
            log.r15 = 0xF003; // Starting initialization
            log.r14 = g_ntoskrnl_base_address;
            logs::add_log(log);

            NTSTATUS status = auto_initialize_kernel_functions();
            if (!NT_SUCCESS(status))
            {
                log = {};
                log.r15 = 0xF0E2; // Initialization failed
                log.r14 = status;
                log.r13 = g_ntoskrnl_base_address;
                logs::add_log(log);

                pfn_info_out->query_status = status;
                return status;
            }

            log = {};
            log.r15 = 0xF004; // Initialization success
            log.r14 = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase);
            logs::add_log(log);
        }

        if (!g_mmonp_MmPfnDatabase)
        {
            log = {};
            log.r15 = 0xF0E3; // MmPfnDatabase not found
            logs::add_log(log);

            pfn_info_out->query_status = STATUS_NOT_FOUND;
            return STATUS_NOT_FOUND;
        }

        // Use current guest CR3 - kernel memory is mapped in all processes on Windows
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        log = {};
        log.r15 = 0xF005; // Using guest CR3
        log.r14 = guest_cr3.flags;
        logs::add_log(log);

        // Calculate PFN entry address in MmPfnDatabase
        std::uint64_t pfn_entry_address = reinterpret_cast<std::uint64_t>(g_mmonp_MmPfnDatabase) +
                                         (pfn_info_out->pfn_index * sizeof(MMPFN));

        log = {};
        log.r15 = 0xF007; // Reading PFN entry
        log.r14 = pfn_entry_address;
        log.r13 = guest_cr3.flags;
        logs::add_log(log);

        // Read the full PFN entry using guest CR3
        MMPFN pfn_entry = {};
        if (!memory_manager::operate_on_guest_virtual_memory(slat_cr3, &pfn_entry,
            pfn_entry_address, guest_cr3, sizeof(MMPFN), memory_operation_t::read_operation))
        {
            log = {};
            log.r15 = 0xF0E5; // Failed to read PFN entry
            log.r14 = pfn_entry_address;
            log.r13 = guest_cr3.flags;
            logs::add_log(log);

            pfn_info_out->query_status = STATUS_UNSUCCESSFUL;
            return STATUS_UNSUCCESSFUL;
        }

        log = {};
        log.r15 = 0xF008; // Successfully read PFN entry
        log.r14 = pfn_entry.u3.e2.ReferenceCount;
        logs::add_log(log);

        // Copy raw PFN data for debugging
        crt::copy_memory(pfn_info_out->raw_pfn_data, &pfn_entry, sizeof(MMPFN));

        // Extract list entry information
        pfn_info_out->list_entry.flink = pfn_entry.u1.Flink;
        pfn_info_out->list_entry.blink = pfn_entry.u2.Blink;

        // Extract reference and share counts
        pfn_info_out->reference_count = pfn_entry.u3.e2.ReferenceCount;
        pfn_info_out->share_count = 0; // ShortFlags doesn't exist in this structure

        // Extract PTE information
        pfn_info_out->pte_address = reinterpret_cast<std::uint64_t>(pfn_entry.PteAddress);
        pfn_info_out->pte_frame = pfn_entry.u4.PteFrame;

        // Extract e1 flags (MMPFNENTRY1)
        pfn_info_out->e1_flags.page_location = pfn_entry.u3.e1.PageLocation;
        pfn_info_out->e1_flags.write_in_progress = pfn_entry.u3.e1.WriteInProgress;
        pfn_info_out->e1_flags.modified = pfn_entry.u3.e1.Modified;
        pfn_info_out->e1_flags.read_in_progress = pfn_entry.u3.e1.ReadInProgress;
        pfn_info_out->e1_flags.cache_attribute = pfn_entry.u3.e1.CacheAttribute;

        // Extract e3 flags (MMPFNENTRY3)
        pfn_info_out->e3_flags.priority = pfn_entry.u3.e3.Priority;
        pfn_info_out->e3_flags.on_prototype_pte = pfn_entry.u3.e3.OnProtectedStandby;
        pfn_info_out->e3_flags.inactive_list_head = pfn_entry.u3.e3.InPageError;
        pfn_info_out->e3_flags.active_flink = pfn_entry.u3.e3.SystemChargedPage;
        pfn_info_out->e3_flags.removal_requested = pfn_entry.u3.e3.RemovalRequested;
        pfn_info_out->e3_flags.parity_error = pfn_entry.u3.e3.ParityError;

        // Extract color and node information
        pfn_info_out->page_color = 0; // PageColor not available in this structure
        pfn_info_out->node_blink = static_cast<std::uint32_t>(pfn_entry.u4.NodeFlinkHigh);

        // Determine if this is likely a hypervisor page
        // Check if it's in a typical hypervisor memory range or has specific characteristics
        
        if (g_hyperv_attachment_physical_base != 0 && g_hyperv_attachment_page_count != 0)
        {
            std::uint64_t hypervisor_start = g_hyperv_attachment_physical_base;
            std::uint64_t hypervisor_end = hypervisor_start + (g_hyperv_attachment_page_count * PAGE_SIZE_4KB);
            
            if (physical_address >= hypervisor_start && physical_address < hypervisor_end)
            {
                pfn_info_out->is_hypervisor_page = 1;
            }
        }

        // Check if the PFN entry looks valid
        if (pfn_info_out->reference_count > 0 || pfn_info_out->e1_flags.page_location != 0)
        {
            pfn_info_out->is_valid = 1;
        }

        pfn_info_out->query_status = STATUS_SUCCESS;
        return STATUS_SUCCESS;
    }

    // Enumerate multiple hypervisor PFNs
    NTSTATUS enumerate_hypervisor_pfns(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes, 
                                      HYPERVISOR_PFN_INFO* pfn_info_array, std::uint32_t max_entries, std::uint32_t* entries_filled)
    {
        if (!pfn_info_array || !entries_filled || max_entries == 0)
            return STATUS_INVALID_PARAMETER;

        *entries_filled = 0;

        std::uint64_t pages_to_check = (hypervisor_size_bytes + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;
        std::uint32_t entries_to_fill = static_cast<std::uint32_t>(crt::min(static_cast<std::uint64_t>(max_entries), pages_to_check));

        for (std::uint32_t i = 0; i < entries_to_fill; i++)
        {
            std::uint64_t current_physical = hypervisor_base_physical + (i * PAGE_SIZE_4KB);
            
            NTSTATUS status = query_hypervisor_pfn_detailed(current_physical, &pfn_info_array[i]);
            if (NT_SUCCESS(status))
            {
                (*entries_filled)++;
            }
            else
            {
                // Still fill the entry but mark it with the error status
                crt::set_memory(&pfn_info_array[i], 0, sizeof(HYPERVISOR_PFN_INFO));
                pfn_info_array[i].physical_address = current_physical;
                pfn_info_array[i].pfn_index = current_physical >> 12;
                pfn_info_array[i].query_status = status;
                (*entries_filled)++;
            }
        }

        return (*entries_filled > 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    }

    // Get hypervisor memory layout information
    NTSTATUS get_hypervisor_memory_info(HYPERVISOR_MEMORY_INFO* memory_info_out)
    {
        if (!memory_info_out)
            return STATUS_INVALID_PARAMETER;

        // Zero out the structure first
        crt::set_memory(memory_info_out, 0, sizeof(HYPERVISOR_MEMORY_INFO));

        // Get hypervisor attachment memory info
        
        if (g_hyperv_attachment_physical_base != 0 && g_hyperv_attachment_page_count != 0)
        {
            memory_info_out->physical_base = g_hyperv_attachment_physical_base;
            memory_info_out->page_count = g_hyperv_attachment_page_count;
            memory_info_out->size_bytes = g_hyperv_attachment_page_count * PAGE_SIZE_4KB;
            memory_info_out->is_valid = 1;
        }

        // Get heap memory info from main.cpp globals
        
        if (heap_physical_initial_base != 0 && heap_total_size_to_hide != 0)
        {
            memory_info_out->heap_physical_base = heap_physical_initial_base;
            memory_info_out->heap_size_bytes = heap_total_size_to_hide;
            memory_info_out->heap_page_count = heap_total_size_to_hide / PAGE_SIZE_4KB;
        }

        // Get UEFI boot memory info
        
        if (uefi_boot_physical_base_address != 0 && uefi_boot_image_size != 0)
        {
            memory_info_out->uefi_boot_base = uefi_boot_physical_base_address;
            memory_info_out->uefi_boot_size = uefi_boot_image_size;
        }

        return STATUS_SUCCESS;
    }
}
