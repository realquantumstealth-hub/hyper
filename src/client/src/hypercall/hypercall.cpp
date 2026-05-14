#include "hypercall.h"
#include <hypercall/hypercall_def.h>
#include <iostream>

extern "C" std::uint64_t launch_raw_hypercall(hypercall_info_t rcx, std::uint64_t rdx, std::uint64_t r8, std::uint64_t r9);

std::uint64_t make_hypercall(hypercall_type_t call_type, std::uint64_t call_reserved_data, std::uint64_t rdx, std::uint64_t r8, std::uint64_t r9)
{
	hypercall_info_t hypercall_info = { };

	hypercall_info.primary_key = hypercall_primary_key;
	hypercall_info.secondary_key = hypercall_secondary_key;
	hypercall_info.call_type = call_type;
	hypercall_info.call_reserved_data = call_reserved_data;

	// Debug: Print the constructed hypercall_info value
	if (call_type == hypercall_type_t::call_dllmain_silently) {
		std::cout << "[DEBUG] Hypercall info construction:" << std::endl;
		std::cout << "  call_type enum value: " << static_cast<std::uint64_t>(call_type) << std::endl;
		std::cout << "  hypercall_info.call_type after assignment: " << static_cast<std::uint64_t>(hypercall_info.call_type) << std::endl;
		std::cout << "  hypercall_info.value (hex): 0x" << std::hex << hypercall_info.value << std::dec << std::endl;
		std::cout << "  Expected call_type bits (16): " << (16 & 0x1F) << std::endl;
		std::cout << "  Actual call_type bits extracted: " << ((hypercall_info.value >> 16) & 0x1F) << std::endl;
	}

	return launch_raw_hypercall(hypercall_info, rdx, r8, r9);
}

std::uint64_t hypercall::read_guest_physical_memory(void* guest_destination_buffer, std::uint64_t guest_source_physical_address, std::uint64_t size)
{
	return make_hypercall(hypercall_type_t::guest_physical_memory_operation, static_cast<std::uint64_t>(memory_operation_t::read_operation), guest_source_physical_address, reinterpret_cast<std::uint64_t>(guest_destination_buffer), size);
}

std::uint64_t hypercall::write_guest_physical_memory(void* guest_source_buffer, std::uint64_t guest_destination_physical_address, std::uint64_t size)
{
	return make_hypercall(hypercall_type_t::guest_physical_memory_operation, static_cast<std::uint64_t>(memory_operation_t::write_operation), guest_destination_physical_address, reinterpret_cast<std::uint64_t>(guest_source_buffer), size);
}

std::uint64_t hypercall::read_guest_virtual_memory(void* guest_destination_buffer, std::uint64_t guest_source_virtual_address, std::uint64_t source_cr3, std::uint64_t size)
{
	virt_memory_op_hypercall_info_t memory_op_call = { };

	memory_op_call.call_type = hypercall_type_t::guest_virtual_memory_operation;
	memory_op_call.memory_operation = memory_operation_t::read_operation;
	memory_op_call.address_of_page_directory = source_cr3 >> 12;

	hypercall_info_t hypercall_info = { .value = memory_op_call.value };

	std::uint64_t guest_destination_virtual_address = reinterpret_cast<std::uint64_t>(guest_destination_buffer);

	return make_hypercall(hypercall_info.call_type, hypercall_info.call_reserved_data, guest_destination_virtual_address, guest_source_virtual_address, size);
}

std::uint64_t hypercall::write_guest_virtual_memory(void* guest_source_buffer, std::uint64_t guest_destination_virtual_address, std::uint64_t destination_cr3, std::uint64_t size)
{
	virt_memory_op_hypercall_info_t memory_op_call = { };

	memory_op_call.call_type = hypercall_type_t::guest_virtual_memory_operation;
	memory_op_call.memory_operation = memory_operation_t::write_operation;
	memory_op_call.address_of_page_directory = destination_cr3 >> 12;

	hypercall_info_t hypercall_info = { .value = memory_op_call.value };

	std::uint64_t guest_source_virtual_address = reinterpret_cast<std::uint64_t>(guest_source_buffer);

	return make_hypercall(hypercall_info.call_type, hypercall_info.call_reserved_data, guest_source_virtual_address, guest_destination_virtual_address, size);
}

std::uint64_t hypercall::translate_guest_virtual_address(std::uint64_t guest_virtual_address, std::uint64_t guest_cr3)
{
	return make_hypercall(hypercall_type_t::translate_guest_virtual_address, 0, guest_virtual_address, guest_cr3, 0);
}

std::uint64_t hypercall::read_guest_cr3()
{
	return make_hypercall(hypercall_type_t::read_guest_cr3, 0, 0, 0, 0);
}

std::uint64_t hypercall::add_slat_code_hook(std::uint64_t target_guest_physical_address, std::uint64_t shadow_page_guest_physical_address)
{
	return make_hypercall(hypercall_type_t::add_slat_code_hook, 0, target_guest_physical_address, shadow_page_guest_physical_address, 0);
}

std::uint64_t hypercall::remove_slat_code_hook(std::uint64_t target_guest_physical_address)
{
	return make_hypercall(hypercall_type_t::remove_slat_code_hook, 0, target_guest_physical_address, 0, 0);
}

std::uint64_t hypercall::hide_guest_physical_page(std::uint64_t target_guest_physical_address)
{
	return make_hypercall(hypercall_type_t::hide_guest_physical_page, 0, target_guest_physical_address, 0, 0);
}

std::uint64_t hypercall::flush_logs(std::vector<trap_frame_log_t>& logs)
{
	return make_hypercall(hypercall_type_t::flush_logs, 0, reinterpret_cast<std::uint64_t>(logs.data()), logs.size(), 0);
}

std::uint64_t hypercall::get_heap_free_page_count()
{
	return make_hypercall(hypercall_type_t::get_heap_free_page_count, 0, 0, 0, 0);
}

std::uint64_t hypercall::get_process_base(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3)
{
	return make_hypercall(hypercall_type_t::get_process_base_by_pid, 0, target_pid, ps_initial_system_process, kernel_cr3);
}

std::uint64_t hypercall::get_process_cr3(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3)
{
	return make_hypercall(hypercall_type_t::get_process_cr3, 0, target_pid, ps_initial_system_process, kernel_cr3);
}

std::uint64_t hypercall::check_hyperv_attachment_memory_mapping(std::uint64_t kernel_cr3, std::uint64_t memory_map_address, memory_mapping_check_result_t& result)
{
	return make_hypercall(hypercall_type_t::check_hyperv_attachment_memory_mapping, 0, kernel_cr3, memory_map_address, reinterpret_cast<std::uint64_t>(&result));
}
std::uint64_t hypercall::get_eprocess_base(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, std::uint64_t kernel_cr3)
{
	return make_hypercall(hypercall_type_t::get_process_eprocess_base, 0, target_pid, ps_initial_system_process, kernel_cr3);
}
struct allocate_hidden_memory_params_t
{
	std::uint64_t virtual_address; // ADD THIS
	std::uint64_t size;
	std::uint64_t target_pid;
	std::uint64_t ps_initial_system_process;
};

std::uint64_t hypercall::allocate_hidden_memory(
	std::uint64_t virtual_address,
	std::uint64_t size,
	std::uint64_t target_pid,
	std::uint64_t ps_initial_system_process,
	std::uint64_t kernel_cr3)
{
	// Create the parameter struct on the stack.
	// Using a local variable is safer than static in case of multithreading.
	allocate_hidden_memory_params_t params = {};
	params.virtual_address = virtual_address; // Populate the new field
	params.size = size;
	params.target_pid = target_pid;
	params.ps_initial_system_process = ps_initial_system_process;

	// Pass kernel_cr3 in reserved_data and a POINTER to the params struct in rdx.
	// r8 and r9 are now unused for this call.
	return make_hypercall(
		hypercall_type_t::allocate_hidden_memory,
		kernel_cr3,                                   // Goes into call_reserved_data
		reinterpret_cast<std::uint64_t>(&params),     // Goes into rdx
		0,                                            // r8 is now unused
		0                                             // r9 is now unused
	);
}

std::uint64_t hypercall::free_hidden_memory(
	std::uint64_t virtual_address,
	std::uint64_t size,
	std::uint64_t target_pid,
	std::uint64_t ps_initial_system_process,
	std::uint64_t kernel_cr3)
{
	static free_hidden_memory_params_t params = {};
	params.virtual_address = virtual_address;
	params.size = size;
	params.target_pid = target_pid;
	params.ps_initial_system_process = ps_initial_system_process;

	// Pass kernel_cr3 in reserved_data and a pointer to the params in rdx
	return make_hypercall(
		hypercall_type_t::free_hidden_memory,
		kernel_cr3,
		reinterpret_cast<std::uint64_t>(&params),	
		0,
		0
	);
}

std::uint64_t hypercall::call_dllmain_silently(std::uint64_t dllmain_address, std::uint64_t hinstDLL, std::uint64_t fdwReason, std::uint64_t lpvReserved)
{
	std::cout << "[HYPERCALL] Making call_dllmain_silently hypercall" << std::endl;
	std::cout << "[HYPERCALL] Type enum value: " << static_cast<std::uint64_t>(hypercall_type_t::call_dllmain_silently) << std::endl;
	std::cout << "[HYPERCALL] Parameters:" << std::endl;
	std::cout << "  DllMain address: 0x" << std::hex << dllmain_address << std::endl;
	std::cout << "  hinstDLL: 0x" << std::hex << hinstDLL << std::endl;
	std::cout << "  fdwReason: " << std::dec << fdwReason << std::endl;
	std::cout << "  lpvReserved: 0x" << std::hex << lpvReserved << std::endl;

	std::uint64_t result = make_hypercall(hypercall_type_t::call_dllmain_silently, lpvReserved, dllmain_address, hinstDLL, fdwReason);

	std::cout << "[HYPERCALL] Hypercall result: " << result << std::endl;
	return result;
}

std::uint64_t hypercall::dirbase_from_base_address(void* base_address, void* ntoskrnl_base)
{
	return make_hypercall(hypercall_type_t::dirbase_from_base_address, 0, reinterpret_cast<std::uint64_t>(base_address), reinterpret_cast<std::uint64_t>(ntoskrnl_base), 0);
}

std::uint64_t hypercall::hide_hypervisor_memory(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes)
{
	return make_hypercall(hypercall_type_t::hide_hypervisor_memory, 0, hypervisor_base_physical, hypervisor_size_bytes, 0);
}

std::uint64_t hypercall::restore_hypervisor_memory(std::uint64_t physical_address)
{
	return make_hypercall(hypercall_type_t::restore_hypervisor_memory, 0, physical_address, 0, 0);
}

std::uint64_t hypercall::query_hypervisor_pfn_info(std::uint64_t physical_address, hypervisor_pfn_info_t* pfn_info_out)
{
	if (!pfn_info_out)
		return 0; // Invalid parameter
		
	return make_hypercall(hypercall_type_t::query_hypervisor_pfn_info, 0, physical_address, reinterpret_cast<std::uint64_t>(pfn_info_out), 0);
}

std::uint64_t hypercall::get_ntoskrnl_base_address()
{
	return make_hypercall(hypercall_type_t::get_ntoskrnl_base_address, 0, 0, 0, 0);
}

std::uint64_t hypercall::update_ntoskrnl_base_address(std::uint64_t new_ntoskrnl_base)
{
	return make_hypercall(hypercall_type_t::update_ntoskrnl_base_address, 0, new_ntoskrnl_base, 0, 0);
}

std::uint64_t hypercall::set_kernel_cr3(std::uint64_t kernel_cr3)
{
	return make_hypercall(hypercall_type_t::set_kernel_cr3, 0, kernel_cr3, 0, 0);
}

std::uint64_t hypercall::get_ntoskrnl_base_from_kpcr()
{
	return make_hypercall(hypercall_type_t::get_ntoskrnl_base_from_kpcr, 0, 0, 0, 0);
}

std::uint64_t hypercall::get_system_process_cr3_from_kpcr()
{
	return make_hypercall(hypercall_type_t::get_system_process_cr3_from_kpcr, 0, 0, 0, 0);
}

std::uint64_t hypercall::get_hypervisor_memory_info(hypervisor_memory_info_t* memory_info_out)
{
	if (!memory_info_out)
		return 0; // Invalid parameter

	return make_hypercall(hypercall_type_t::get_hypervisor_memory_info, 0, reinterpret_cast<std::uint64_t>(memory_info_out), 0, 0);
}

std::uint64_t hypercall::test_export_discovery(export_discovery_test_result_t* result_out)
{
	if (!result_out)
		return 0; // Invalid parameter

	return make_hypercall(hypercall_type_t::test_export_discovery, 0, reinterpret_cast<std::uint64_t>(result_out), 0, 0);
}

std::uint64_t hypercall::enable_cr3_caching()
{
	return make_hypercall(hypercall_type_t::enable_cr3_caching, 0, 0, 0, 0);
}

std::uint64_t hypercall::disable_cr3_caching()
{
	return make_hypercall(hypercall_type_t::disable_cr3_caching, 0, 0, 0, 0);
}

std::uint64_t hypercall::get_cached_cr3()
{
	return make_hypercall(hypercall_type_t::get_cached_cr3, 0, 0, 0, 0);
}

std::uint64_t hypercall::set_target_pid_for_cr3_caching(std::uint64_t target_pid)
{
	return make_hypercall(hypercall_type_t::set_target_pid_for_cr3_caching, 0, target_pid, 0, 0);
}

std::uint64_t hypercall::get_target_pid_for_cr3_caching()
{
	return make_hypercall(hypercall_type_t::get_target_pid_for_cr3_caching, 0, 0, 0, 0);
}

std::uint64_t hypercall::get_cr3_cache_stats(cr3_stats_t* stats_out)
{
	if (!stats_out)
		return 0; // Invalid parameter

	return make_hypercall(hypercall_type_t::get_cr3_cache_stats, 0, reinterpret_cast<std::uint64_t>(stats_out), 0, 0);
}
