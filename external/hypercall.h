#pragma once

#include <cstdint>
#include <structures/trap_frame.h>
#include <ia32-doc/ia32.hpp>

union hypercall_info_t;

// Structure to hold memory mapping verification results
struct memory_mapping_check_result_t
{
    std::uint64_t is_mapped_by_os;
    std::uint64_t physical_address_checked;
    std::uint64_t page_count_checked;
    std::uint64_t verification_status;
};

// Defines an entry for tracking a hidden memory allocation.
struct hidden_memory_entry_t
{
    std::uint64_t gpa_pfn;
    std::uint64_t hidden_hpa_pfn;
    hidden_memory_entry_t* next;
};

// Global variables for system state
extern std::uint64_t g_hyperv_attachment_physical_base;
extern std::uint64_t g_hyperv_attachment_page_count;
extern volatile std::uint64_t g_process_in_hooked_view_cr3;

// Forward declaration for get_ntoskrnl_from_kpcr (defined in hypercall.cpp)
std::uint64_t get_ntoskrnl_from_kpcr();

namespace hypercall
{
    void process(hypercall_info_t hypercall_info, trap_frame_t* trap_frame, trap_frame_t* original_trap_frame = nullptr);
    std::uint64_t dirbase_from_base_address(void* base_address);
}