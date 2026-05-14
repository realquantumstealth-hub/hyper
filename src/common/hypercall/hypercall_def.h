#pragma once
#include <cstdint>
#include <structures/memory_operation.h>

enum class hypercall_type_t : std::uint64_t
{
    guest_physical_memory_operation,
    guest_virtual_memory_operation,
    translate_guest_virtual_address,
    read_guest_cr3,
    add_slat_code_hook,
    remove_slat_code_hook,
    hide_guest_physical_page,
    log_current_state,
    flush_logs,
    get_heap_free_page_count,
    get_process_base_by_pid,
    get_process_cr3,
    check_hyperv_attachment_memory_mapping,
    allocate_hidden_memory,
    free_hidden_memory,
    get_process_eprocess_base,  
    call_dllmain_silently,
    dirbase_from_base_address,
    hide_hypervisor_memory,
    restore_hypervisor_memory,
    get_ntoskrnl_base_address,
    update_ntoskrnl_base_address, // Update hypervisor's ntoskrnl base address
    set_kernel_cr3, // Set the kernel CR3 for hypervisor to use
    get_ntoskrnl_base_from_kpcr, // Discover ntoskrnl base via KPCR traversal
    get_system_process_cr3_from_kpcr, // Discover current system process CR3 using KPCR walk
    query_hypervisor_pfn_info, // Query PFN information for a physical address
    get_hypervisor_memory_info,  // Get hypervisor memory layout
    test_export_discovery, // Test if hypervisor can find MmGetVirtualForPhysical and MmPfnDatabase
    enable_cr3_caching, // Enable CR3 shuffling bypass via MTF
    disable_cr3_caching, // Disable CR3 monitoring
    get_cached_cr3, // Get cached CR3 for a PID
    get_cr3_cache_stats, // Get CR3 cache statistics
    set_target_pid_for_cr3_caching, // Set the target PID to monitor for CR3 caching (0 = all processes)
    get_target_pid_for_cr3_caching // Get the current target PID for CR3 caching
};

#pragma warning(push)
#pragma warning(disable: 4201)

constexpr std::uint64_t hypercall_primary_key = 0x4E47;
constexpr std::uint64_t hypercall_secondary_key = 0x7F;

union hypercall_info_t
{
    std::uint64_t value;

    struct
    {
        std::uint64_t primary_key : 16;
        hypercall_type_t call_type : 6;  // Increased to 6 bits to support up to 64 hypercall types (0-63)
        std::uint64_t secondary_key : 7;
        std::uint64_t call_reserved_data : 35;  // Reduced from 36 to 35 bits to compensate
    };
};

union virt_memory_op_hypercall_info_t
{
    std::uint64_t value;

    struct
    {
        std::uint64_t primary_key : 16;
        hypercall_type_t call_type : 6;  // Increased to 6 bits to match the main struct
        std::uint64_t secondary_key : 7;
        memory_operation_t memory_operation : 1;
        std::uint64_t address_of_page_directory : 34; // Reduced from 35 to 34 bits to compensate
    };
};



#pragma warning(pop)

