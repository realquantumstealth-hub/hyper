#pragma once
#include <cstdint>
#include <vector>
#include <structures/trap_frame.h>

// Structure to hold memory mapping verification results
struct memory_mapping_check_result_t
{
	std::uint64_t is_mapped_by_os;
	std::uint64_t physical_address_checked;
	std::uint64_t page_count_checked;
	std::uint64_t verification_status;
};

// Structure to return detailed PFN information from hypervisor
struct hypervisor_pfn_info_t
{
    std::uint64_t physical_address;
    std::uint64_t pfn_index;
    
    // PFN Entry information
    struct {
        std::uint64_t flink;
        std::uint64_t blink;
    } list_entry;
    
    std::uint64_t reference_count;
    std::uint64_t share_count;
    
    // PTE information
    std::uint64_t pte_address;
    std::uint64_t pte_frame;
    
    // Flags from MMPFNENTRY1
    struct {
        std::uint8_t page_location : 3;
        std::uint8_t write_in_progress : 1;
        std::uint8_t modified : 1;
        std::uint8_t read_in_progress : 1;
        std::uint8_t cache_attribute : 2;
    } e1_flags;
    
    // Flags from MMPFNENTRY3  
    struct {
        std::uint8_t priority : 3;
        std::uint8_t on_prototype_pte : 1;
        std::uint8_t inactive_list_head : 1;
        std::uint8_t active_flink : 1;
        std::uint8_t removal_requested : 1;
        std::uint8_t parity_error : 1;
    } e3_flags;
    
    // Color and other info
    std::uint32_t page_color;
    std::uint32_t node_blink;
    
    // Raw data for debugging
    std::uint8_t raw_pfn_data[0x30]; // Full MMPFN structure
    
    // Status information
    std::int32_t query_status; // NTSTATUS
    std::uint8_t is_hypervisor_page;
    std::uint8_t is_valid;
    std::uint8_t reserved[6];
};

// Structure to return hypervisor memory layout information
struct hypervisor_memory_info_t
{
    std::uint64_t physical_base;
    std::uint64_t size_bytes;
    std::uint64_t page_count;
    std::uint64_t heap_physical_base;
    std::uint64_t heap_size_bytes;
    std::uint64_t heap_page_count;
    std::uint64_t uefi_boot_base;
    std::uint64_t uefi_boot_size;
    std::uint8_t is_valid;
    std::uint8_t reserved[7];
};

// Export discovery test results from hypervisor
struct export_discovery_test_result_t
{
    // Status flags
    std::uint8_t ntoskrnl_found;
    std::uint8_t mm_get_virtual_for_physical_found;
    std::uint8_t pattern_found;
    std::uint8_t mmpfn_database_found;

    // Addresses
    std::uint64_t ntoskrnl_base;
    std::uint64_t mm_get_virtual_for_physical_address;
    std::uint64_t pattern_offset;
    std::uint64_t mmpfn_database_address;

    // First 128 bytes of MmGetVirtualForPhysical for inspection
    std::uint8_t mm_get_virtual_bytes[128];

    // NTSTATUS codes
    std::uint64_t ntoskrnl_status;
    std::uint64_t export_status;
    std::uint64_t pattern_status;
    std::uint64_t mmpfn_status;
};

namespace hypercall
{
	std::uint64_t read_guest_physical_memory(void* guest_destination_buffer, std::uint64_t guest_source_physical_address, std::uint64_t size);
	std::uint64_t write_guest_physical_memory(void* guest_source_buffer, std::uint64_t guest_destination_physical_address, std::uint64_t size);

	std::uint64_t read_guest_virtual_memory(void* guest_destination_buffer, std::uint64_t guest_source_virtual_address, std::uint64_t source_cr3, std::uint64_t size);
	std::uint64_t write_guest_virtual_memory(void* guest_source_buffer, std::uint64_t guest_destination_virtual_address, std::uint64_t destination_cr3, std::uint64_t size);

	std::uint64_t translate_guest_virtual_address(std::uint64_t guest_virtual_address, std::uint64_t guest_cr3);

	std::uint64_t read_guest_cr3();

	std::uint64_t add_slat_code_hook(std::uint64_t target_guest_physical_address, std::uint64_t shadow_page_guest_physical_address);
	std::uint64_t remove_slat_code_hook(std::uint64_t target_guest_physical_address);
	std::uint64_t hide_guest_physical_page(std::uint64_t target_guest_physical_address);

	std::uint64_t flush_logs(std::vector<trap_frame_log_t>& logs);

	std::uint64_t get_heap_free_page_count();

	std::uint64_t get_process_base(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3);

	std::uint64_t get_process_cr3(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3);

	std::uint64_t check_hyperv_attachment_memory_mapping(std::uint64_t kernel_cr3, std::uint64_t memory_map_address, memory_mapping_check_result_t& result);
	std::uint64_t get_eprocess_base(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3);

	// Hidden memory functions for stealth DLL injection
	std::uint64_t allocate_hidden_memory(
		std::uint64_t virtual_address,
		std::uint64_t size,
		std::uint64_t target_pid,
		std::uint64_t ps_initial_system_process,
		std::uint64_t kernel_cr3 // Add this parameter
	);
	struct free_hidden_memory_params_t
	{
		std::uint64_t virtual_address;
		std::uint64_t size;
		std::uint64_t target_pid;
		std::uint64_t ps_initial_system_process;
	};

	std::uint64_t free_hidden_memory(
		std::uint64_t virtual_address,
		std::uint64_t size,
		std::uint64_t target_pid,
		std::uint64_t ps_initial_system_process,
		std::uint64_t kernel_cr3
	);


	std::uint64_t call_dllmain_silently(std::uint64_t dllmain_address, std::uint64_t hinstDLL, std::uint64_t fdwReason, std::uint64_t lpvReserved);

	std::uint64_t dirbase_from_base_address(void* base_address, void* ntoskrnl_base);
	
	// MmPfnDatabase manipulation for stealth
	std::uint64_t hide_hypervisor_memory(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes);
	std::uint64_t restore_hypervisor_memory(std::uint64_t physical_address);
	
	// Query hypervisor memory in MmPfnDatabase
	std::uint64_t query_hypervisor_pfn_info(std::uint64_t physical_address, hypervisor_pfn_info_t* pfn_info_out);
	std::uint64_t set_kernel_cr3(std::uint64_t kernel_cr3);
	std::uint64_t get_ntoskrnl_base_from_kpcr();
	std::uint64_t get_system_process_cr3_from_kpcr();
	std::uint64_t get_ntoskrnl_base_address();
	std::uint64_t update_ntoskrnl_base_address(std::uint64_t new_ntoskrnl_base);
	std::uint64_t get_hypervisor_memory_info(hypervisor_memory_info_t* memory_info_out);
	std::uint64_t test_export_discovery(export_discovery_test_result_t* result_out);

	// CR3 cache functions for bypassing CR3 shuffling
	std::uint64_t enable_cr3_caching();
	std::uint64_t disable_cr3_caching();
	std::uint64_t get_cached_cr3();
	std::uint64_t set_target_pid_for_cr3_caching(std::uint64_t target_pid);
	std::uint64_t get_target_pid_for_cr3_caching();

	struct cr3_stats_t {
		std::uint64_t total_samples;
		std::uint64_t ring3_samples;
		std::uint64_t target_pid_hits;
		std::uint64_t cr3_updates;
		std::uint64_t last_cached_cr3;
	};
	std::uint64_t get_cr3_cache_stats(cr3_stats_t* stats_out);
}
