#include "ntoskrnl_parser.h"
#include "../crt/crt.h"
#include "../logs/logs.h"
#include "../memory_manager/memory_management.h"
#include "../memory_manager/memory_manager.h"
#include "../arch/arch.h"
#include "../slat/slat.h"
#include "../../shared/structures/memory_operation.h"
#include "heap_manager.h"

// Windows PE constants
#ifndef IMAGE_NT_OPTIONAL_HDR64_MAGIC
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#endif

#ifndef IMAGE_FILE_EXECUTABLE_IMAGE
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#endif

// Access to global ntoskrnl base address captured during boot
extern std::uint64_t g_ntoskrnl_base_address;

namespace ntoskrnl_parser
{
    // Global variables to cache ntoskrnl.exe information
    static PVOID g_ntoskrnl_base = nullptr;
    static bool g_initialized = false;

    // Helper function to read guest virtual memory safely
    bool read_guest_memory(PVOID guest_va, PVOID buffer, SIZE_T size, cr3 guest_cr3, cr3 slat_cr3)
    {
        std::uint64_t guest_source_virtual_address = reinterpret_cast<std::uint64_t>(guest_va);
        std::uint64_t size_left_to_read = size;
        std::uint64_t bytes_copied = 0;

        while (size_left_to_read != 0)
        {
            std::uint64_t size_left_of_source_virtual_page = UINT64_MAX;
            std::uint64_t size_left_of_source_slat_page = UINT64_MAX;

            std::uint64_t guest_source_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = guest_source_virtual_address + bytes_copied }, &size_left_of_source_virtual_page);

            if (size_left_of_source_virtual_page == UINT64_MAX) break;

            std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_source_physical_address, &size_left_of_source_slat_page);

            if (size_left_of_source_slat_page == UINT64_MAX) break;

            std::uint64_t size_left_of_pages = crt::min(size_left_of_source_slat_page, size_left_of_source_virtual_page);
            std::uint64_t copy_size = crt::min(size_left_to_read, size_left_of_pages);

            if (copy_size == 0) break;

            crt::copy_memory(reinterpret_cast<UCHAR*>(buffer) + bytes_copied, reinterpret_cast<const void*>(host_source), copy_size);

            size_left_to_read -= copy_size;
            bytes_copied += copy_size;
        }

        return bytes_copied == size;
    }

    // Dynamic ntoskrnl.exe base discovery using safe, limited scanning
    std::uint64_t find_ntoskrnl_base_dynamically()
    {
        // For safety, temporarily disable dynamic discovery to prevent BSOD
        // The issue is likely unsafe memory access during scanning
        return 0;
        
        /*
        // TODO: Re-implement with safer memory access patterns
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();
        
        // Much more limited search scope to prevent crashes
        // Only check common ntoskrnl locations
        std::uint64_t common_addresses[] = {
            0xFFFFF80000000000ULL,   // Common base
            0xFFFFF80001000000ULL,   // +16MB
            0xFFFFF80002000000ULL,   // +32MB
            0xFFFFF80004000000ULL,   // +64MB
            0xFFFFF80008000000ULL,   // +128MB
            0xFFFFF80010000000ULL,   // +256MB
            0xFFFFF80020000000ULL,   // +512MB
            0xFFFFF80040000000ULL,   // +1GB
        };
        
        for (auto addr : common_addresses)
        {
            if (validate_ntoskrnl_base_safe(addr, guest_cr3, slat_cr3))
            {
                return addr;
            }
        }
        
        return 0;
        */
    }
    
    // Helper function to validate that an address is actually ntoskrnl.exe
    bool validate_ntoskrnl_base(std::uint64_t base_addr, cr3 guest_cr3, cr3 slat_cr3)
    {
        // Read DOS header
        IMAGE_DOS_HEADER dos_header;
        if (!read_guest_memory(reinterpret_cast<PVOID>(base_addr), &dos_header, sizeof(dos_header), guest_cr3, slat_cr3))
            return false;
            
        // Check DOS signature
        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
            return false;
            
        // Validate e_lfanew is reasonable (not too large)
        if (dos_header.e_lfanew > 0x1000 || dos_header.e_lfanew < sizeof(IMAGE_DOS_HEADER))
            return false;
            
        // Read NT headers
        std::uint64_t nt_headers_addr = base_addr + dos_header.e_lfanew;
        IMAGE_NT_HEADERS64 nt_headers;
        if (!read_guest_memory(reinterpret_cast<PVOID>(nt_headers_addr), &nt_headers, sizeof(nt_headers), guest_cr3, slat_cr3))
            return false;
            
        // Check PE signature
        if (nt_headers.Signature != IMAGE_NT_SIGNATURE)
            return false;
            
        // Additional validation: check if it's a kernel image
        if (nt_headers.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;
            
        // Check if it's a system image (typical characteristics of ntoskrnl)
        if (!(nt_headers.FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE))
            return false;
            
        return true;
    }

    NTSTATUS initialize(PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3)
    {
        if (g_initialized && g_ntoskrnl_base)
            return STATUS_SUCCESS;

        if (!ntoskrnl_base)
        {
            // Invalid ntoskrnl.exe base address provided
            return STATUS_INVALID_PARAMETER;
        }

        g_ntoskrnl_base = ntoskrnl_base;
        // Using ntoskrnl.exe at specified base
        g_initialized = true;
        return STATUS_SUCCESS;
    }

    PVOID get_export_by_name(const char* export_name, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3)
    {
        // Log export resolution attempt
        trap_frame_log_t log_entry = {};
        log_entry.r15 = 0xE010; // Export resolution start
        log_entry.r14 = reinterpret_cast<std::uint64_t>(ntoskrnl_base); // ntoskrnl base
        log_entry.r13 = export_name ? (static_cast<std::uint64_t>(export_name[0]) << 24 | 
                                       static_cast<std::uint64_t>(export_name[1]) << 16 |
                                       static_cast<std::uint64_t>(export_name[2]) << 8 |
                                       static_cast<std::uint64_t>(export_name[3])) : 0; // First 4 chars of export name
        log_entry.r12 = guest_cr3.flags; // Guest CR3 being used
        logs::add_log(log_entry);
        
        if (!ntoskrnl_base)
        {
            // Log invalid base
            log_entry = {};
            log_entry.r15 = 0xE011; // Invalid ntoskrnl base
            logs::add_log(log_entry);
            return nullptr;
        }

        // Read DOS header
        IMAGE_DOS_HEADER dos_header;
        if (!read_guest_memory(ntoskrnl_base, &dos_header, sizeof(dos_header), guest_cr3, slat_cr3))
        {
            // Log DOS header read failure
            log_entry = {};
            log_entry.r15 = 0xE012; // DOS header read failed
            logs::add_log(log_entry);
            return nullptr;
        }

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        {
            // Log invalid DOS signature
            log_entry = {};
            log_entry.r15 = 0xE013; // Invalid DOS signature
            log_entry.r14 = dos_header.e_magic;
            logs::add_log(log_entry);
            return nullptr;
        }

        // Read NT headers
        PVOID nt_headers_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + dos_header.e_lfanew);
        IMAGE_NT_HEADERS nt_headers;
        if (!read_guest_memory(nt_headers_va, &nt_headers, sizeof(nt_headers), guest_cr3, slat_cr3))
        {
            // Log NT headers read failure
            log_entry = {};
            log_entry.r15 = 0xE014; // NT headers read failed
            log_entry.r14 = reinterpret_cast<std::uint64_t>(nt_headers_va);
            logs::add_log(log_entry);
            return nullptr;
        }

        if (nt_headers.Signature != IMAGE_NT_SIGNATURE)
        {
            // Log invalid NT signature
            log_entry = {};
            log_entry.r15 = 0xE015; // Invalid NT signature
            log_entry.r14 = nt_headers.Signature;
            logs::add_log(log_entry);
            return nullptr;
        }

        // Get export directory
        IMAGE_DATA_DIRECTORY* export_data_dir = &nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (export_data_dir->VirtualAddress == 0 || export_data_dir->Size == 0)
        {
            // Log no export directory
            log_entry = {};
            log_entry.r15 = 0xE016; // No export directory
            log_entry.r14 = export_data_dir->VirtualAddress;
            log_entry.r13 = export_data_dir->Size;
            logs::add_log(log_entry);
            return nullptr;
        }

        PVOID export_dir_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + export_data_dir->VirtualAddress);
        IMAGE_EXPORT_DIRECTORY export_directory;
        if (!read_guest_memory(export_dir_va, &export_directory, sizeof(export_directory), guest_cr3, slat_cr3))
        {
            // Log export directory read failure
            log_entry = {};
            log_entry.r15 = 0xE017; // Export directory read failed
            log_entry.r14 = reinterpret_cast<std::uint64_t>(export_dir_va);
            logs::add_log(log_entry);
            return nullptr;
        }

        // Get address tables
        PVOID names_table_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + export_directory.AddressOfNames);
        PVOID ordinals_table_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + export_directory.AddressOfNameOrdinals);
        PVOID functions_table_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + export_directory.AddressOfFunctions);

        // Search through the name table
        for (DWORD i = 0; i < export_directory.NumberOfNames; i++)
        {
            // Read name RVA
            DWORD name_rva;
            PVOID name_rva_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(names_table_va) + i * sizeof(DWORD));
            if (!read_guest_memory(name_rva_va, &name_rva, sizeof(name_rva), guest_cr3, slat_cr3))
                continue;

            // Read the actual name
            PVOID name_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + name_rva);
            char current_name[256];
            if (!read_guest_memory(name_va, current_name, sizeof(current_name), guest_cr3, slat_cr3))
                continue;

            current_name[255] = '\0'; // Ensure null termination

            if (memory_management::compare_strings(current_name, export_name) == 0)
            {
                // Found the name, get the ordinal
                WORD ordinal;
                PVOID ordinal_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ordinals_table_va) + i * sizeof(WORD));
                if (!read_guest_memory(ordinal_va, &ordinal, sizeof(ordinal), guest_cr3, slat_cr3))
                    continue;

                // Get the function RVA
                DWORD function_rva;
                PVOID function_rva_va = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(functions_table_va) + ordinal * sizeof(DWORD));
                if (!read_guest_memory(function_rva_va, &function_rva, sizeof(function_rva), guest_cr3, slat_cr3))
                    continue;

                // Return the actual address
                PVOID result = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(ntoskrnl_base) + function_rva);
                
                // Log successful export resolution
                log_entry = {};
                log_entry.r15 = 0xE018; // Export found successfully
                log_entry.r14 = reinterpret_cast<std::uint64_t>(result);
                log_entry.r13 = function_rva;
                logs::add_log(log_entry);
                
                return result;
            }
        }

        // Export not found
        return nullptr;
    }

    PVOID get_export_by_ordinal(DWORD ordinal, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3)
    {
        // Implementation similar to get_export_by_name but using ordinal directly
        // For brevity, returning nullptr for now as it's less commonly used
        return nullptr;
    }

    PVOID get_ntoskrnl_base()
    {
        // If we have a cached base, return it
        if (g_ntoskrnl_base)
            return g_ntoskrnl_base;
            
        // Otherwise, use the captured base address from boot
        if (g_ntoskrnl_base_address)
        {
            g_ntoskrnl_base = reinterpret_cast<PVOID>(g_ntoskrnl_base_address);
            return g_ntoskrnl_base;
        }
        
        // If boot capture failed, try to find ntoskrnl at runtime using dynamic method
        std::uint64_t ntoskrnl_base = find_ntoskrnl_base_dynamically();
        if (ntoskrnl_base)
        {
            g_ntoskrnl_base = reinterpret_cast<PVOID>(ntoskrnl_base);
            // Found ntoskrnl.exe dynamically
            return g_ntoskrnl_base;
        }
        
        // Failed to find ntoskrnl.exe at runtime
        return nullptr;
    }

    PVOID find_pattern_in_ntoskrnl(const void* pattern, SIZE_T pattern_size, PVOID ntoskrnl_base, cr3 guest_cr3, cr3 slat_cr3, SIZE_T max_search_size)
    {
        if (!ntoskrnl_base)
        {
            // Invalid ntoskrnl base address provided
            return nullptr;
        }

        return split_memory(ntoskrnl_base, max_search_size, pattern, pattern_size, guest_cr3, slat_cr3);
    }

    PVOID split_memory(PVOID search_base, SIZE_T search_size, const void* pattern, SIZE_T pattern_size, cr3 guest_cr3, cr3 slat_cr3)
    {
        if (!search_base || !pattern || pattern_size == 0)
            return nullptr;

        // Allocate a page for the buffer
        void* buffer_page = heap_manager::allocate_page();
        if (!buffer_page)
            return nullptr;

        UCHAR* buffer = reinterpret_cast<UCHAR*>(buffer_page);
        const UCHAR* pattern_bytes = reinterpret_cast<const UCHAR*>(pattern);
        const SIZE_T buffer_size = 0x1000; // 4KB page
        PVOID result_address = nullptr;

        for (SIZE_T offset = 0; offset < search_size; offset += buffer_size - pattern_size + 1)
        {
            SIZE_T read_size = crt::min(buffer_size, search_size - offset);
            PVOID read_address = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(search_base) + offset);

            trap_frame_log_t log = {};
            log.r15 = 0xE0D0; // split_memory read attempt
            log.r14 = reinterpret_cast<std::uint64_t>(read_address);
            log.r13 = read_size;
            logs::add_log(log);

            bool read_success = read_guest_memory(read_address, buffer, read_size, guest_cr3, slat_cr3);

            log = {};
            log.r15 = 0xE0D1; // split_memory read result
            log.r14 = read_success ? 1 : 0;
            log.r13 = buffer[0]; // First byte of buffer
            logs::add_log(log);

            if (!read_success)
                continue;

            // Search within this buffer
            for (SIZE_T i = 0; i <= read_size - pattern_size; i++)
            {
                bool found = true;
                for (SIZE_T j = 0; j < pattern_size; j++)
                {
                    if (buffer[i + j] != pattern_bytes[j])
                    {
                        found = false;
                        break;
                    }
                }

                if (found)
                {
                    result_address = reinterpret_cast<PVOID>(reinterpret_cast<UCHAR*>(search_base) + offset + i);
                    goto cleanup;
                }
            }
        }

    cleanup:
        // Free the allocated page
        heap_manager::free_page(buffer_page);
        return result_address;
    }
}