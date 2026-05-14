#pragma once
#include <unordered_map>
#include <optional>
#include <string>
#include "system_def.h"

namespace sys
{
	struct kernel_module_t;

	std::uint8_t set_up();
	void clean_up();

	namespace kernel
	{
		std::uint8_t parse_modules();
		std::uint8_t dump_module_to_disk(std::string_view target_module_name, const std::string_view output_directory);

		inline std::unordered_map<std::string, kernel_module_t> modules_list = { };
	}

	namespace user
	{
		std::uint32_t query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size);

		std::uint32_t adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state);
		std::uint8_t set_debug_privilege(std::uint8_t state, std::uint8_t* previous_state);

		void* allocate_locked_memory(std::uint64_t size, std::uint32_t protection);
		std::uint8_t free_memory(void* address);

		std::string to_string(const std::wstring& wstring);
	}

	namespace fs
	{
		std::uint8_t exists(std::string_view path);

		std::uint8_t write_to_disk(std::string_view full_path, const std::vector<std::uint8_t>& buffer);
	}

	struct kernel_module_t
	{
		std::unordered_map<std::string, std::uint64_t> exports;
		
		std::uint64_t base_address;
		std::uint32_t size;
	};

	// Process enumeration functions
	std::uint32_t get_pid_from_process_name(const std::string& process_name);
	std::uint64_t get_process_base_address(std::uint32_t pid);
	std::uint32_t get_image_size(std::uint64_t base_address);
	std::uint32_t get_image_size(std::uint64_t base_address, std::uint64_t process_cr3);
	std::uint64_t scan_memory(std::uint64_t base_address, std::uint32_t size, const std::uint8_t* pattern, std::size_t pattern_size);
	std::uint64_t scan_memory(std::uint64_t base_address, std::uint32_t size, const std::uint8_t* pattern, std::size_t pattern_size, std::uint64_t process_cr3);
	std::uint64_t get_peb_address(std::uint32_t target_pid, std::uint64_t kernel_cr3, std::uint64_t ps_initial_system_process);
	std::uint64_t find_target_module_base(std::uint32_t target_pid, std::uint64_t target_cr3, const std::string& module_name);
	std::uint64_t find_export_address_in_target(std::uint64_t target_module_base, std::uint64_t target_cr3, const std::string& function_name);

	inline std::uint64_t current_cr3 = 0;
	inline std::uint64_t ps_active_process_head = 0;
	inline std::uint64_t ps_initial_system_process = 0;
	inline std::uint64_t mm_physical_memory_block = 0;
	inline std::uint64_t ps_loaded_module_list = 0;

	// Get ntoskrnl.exe base address
	void* get_ntoskrnl_base();
}
