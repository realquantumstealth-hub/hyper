#include "tbi_discovery.h"
#include "../arch/arch.h"
#include "../slat/slat.h"
#include "../memory_manager/memory_manager.h"
#include "../memory_manager/ntoskrnl_parser.h"
#include "../logs/logs.h"
#include "../crt/crt.h"

extern std::uint64_t g_ntoskrnl_base_address;

namespace tbi_discovery
{
	namespace
	{
		tbi_info_t g_tbi_info = {};
		std::uint64_t g_vmexit_counter = 0;
		bool g_initialized = false;
	}

	void initialize()
	{
		crt::set_memory(&g_tbi_info, 0, sizeof(g_tbi_info));
		g_vmexit_counter = 0;
		g_initialized = true;

		trap_frame_log_t log = {};
		log.r15 = 0x7B10; // TBS discovery initialized
		logs::add_log(log);
	}

	// LDR_DATA_TABLE_ENTRY structure (simplified)
	struct ldr_data_table_entry_t
	{
		std::uint64_t in_load_order_links[2];     // LIST_ENTRY (flink, blink)
		std::uint64_t in_memory_order_links[2];   // LIST_ENTRY
		std::uint64_t in_initialization_order_links[2]; // LIST_ENTRY
		std::uint64_t dll_base;                   // +0x30
		std::uint64_t entry_point;                // +0x38
		std::uint32_t size_of_image;              // +0x40
		std::uint32_t padding;
		std::uint64_t full_dll_name_buffer;       // +0x48 UNICODE_STRING.Buffer
		std::uint16_t full_dll_name_length;       // +0x48 UNICODE_STRING.Length
		std::uint16_t full_dll_name_max_length;   // +0x4A UNICODE_STRING.MaximumLength
		std::uint32_t padding2;
		std::uint64_t base_dll_name_buffer;       // +0x58 UNICODE_STRING.Buffer
		std::uint16_t base_dll_name_length;       // +0x58 UNICODE_STRING.Length
		std::uint16_t base_dll_name_max_length;   // +0x5A UNICODE_STRING.MaximumLength
	};

	// Find PsLoadedModuleList in ntoskrnl.exe
	std::uint64_t find_psloadedmodulelist(std::uint64_t ntoskrnl_base, cr3 kernel_cr3, cr3 slat_cr3)
	{
		trap_frame_log_t log = {};

		log.r15 = 0x7B20; // Finding PsLoadedModuleList
		log.r14 = ntoskrnl_base;
		logs::add_log(log);

		// Try to get PsLoadedModuleList via export (it's exported in some Windows versions)
		NTSTATUS status = ntoskrnl_parser::initialize(
			reinterpret_cast<PVOID>(ntoskrnl_base),
			kernel_cr3,
			slat_cr3);

		if (NT_SUCCESS(status))
		{
			PVOID psloadedmodulelist = ntoskrnl_parser::get_export_by_name(
				"PsLoadedModuleList",
				reinterpret_cast<PVOID>(ntoskrnl_base),
				kernel_cr3,
				slat_cr3);

			if (psloadedmodulelist)
			{
				log.r15 = 0x7B21; // Found via export
				log.r14 = reinterpret_cast<std::uint64_t>(psloadedmodulelist);
				logs::add_log(log);
				return reinterpret_cast<std::uint64_t>(psloadedmodulelist);
			}
		}

		// Pattern scan as fallback
		// PsLoadedModuleList is typically in .data section
		// Pattern: Look for the list head which points to first driver entry
		log.r15 = 0x7B22; // Export not found, trying pattern
		logs::add_log(log);

		return 0;
	}

	// Search PsLoadedModuleList for tbs.dll
	std::uint64_t find_tbi_dll_in_module_list(std::uint64_t psloadedmodulelist, cr3 kernel_cr3, cr3 slat_cr3)
	{
		trap_frame_log_t log = {};

		log.r15 = 0x7B30; // Walking module list
		log.r14 = psloadedmodulelist;
		logs::add_log(log);

		// Read the first link
		std::uint64_t first_entry = 0;
		if (memory_manager::operate_on_guest_virtual_memory(
			slat_cr3, &first_entry, psloadedmodulelist, kernel_cr3,
			sizeof(first_entry), memory_operation_t::read_operation) != sizeof(first_entry))
		{
			log.r15 = 0x7BE0; // Failed to read list head
			logs::add_log(log);
			return 0;
		}

		std::uint64_t current_entry = first_entry;
		std::uint32_t iteration = 0;
		constexpr std::uint32_t max_iterations = 500;

		while (current_entry != 0 && current_entry != psloadedmodulelist && iteration < max_iterations)
		{
			// Read LDR_DATA_TABLE_ENTRY
			ldr_data_table_entry_t ldr_entry = {};
			if (memory_manager::operate_on_guest_virtual_memory(
				slat_cr3, &ldr_entry, current_entry, kernel_cr3,
				sizeof(ldr_entry), memory_operation_t::read_operation) != sizeof(ldr_entry))
			{
				break;
			}

			// Read base DLL name (e.g., "tbs.dll")
			if (ldr_entry.base_dll_name_buffer != 0 && ldr_entry.base_dll_name_length > 0 && ldr_entry.base_dll_name_length < 256)
			{
				wchar_t dll_name[128] = {};
				std::uint64_t read_size = ldr_entry.base_dll_name_length;
				if (read_size > sizeof(dll_name) - 2)
					read_size = sizeof(dll_name) - 2;

				if (memory_manager::operate_on_guest_virtual_memory(
					slat_cr3, dll_name, ldr_entry.base_dll_name_buffer, kernel_cr3,
					read_size, memory_operation_t::read_operation) == read_size)
				{
					// Compare with "tbs.dll" (case insensitive)
					const wchar_t target[] = L"tbs.dll";
					bool match = true;

					for (std::uint32_t i = 0; i < 7 && i < (read_size / 2); i++)
					{
						wchar_t c1 = dll_name[i];
						wchar_t c2 = target[i];

						// Simple case insensitive compare
						if (c1 >= L'A' && c1 <= L'Z') c1 += 32;
						if (c2 >= L'A' && c2 <= L'Z') c2 += 32;

						if (c1 != c2)
						{
							match = false;
							break;
						}
					}

					if (match)
					{
						log.r15 = 0x7B31; // Found tbs.dll
						log.r14 = ldr_entry.dll_base;
						log.r13 = ldr_entry.size_of_image;
						logs::add_log(log);

						return ldr_entry.dll_base;
					}
				}
			}

			// Move to next entry
			current_entry = ldr_entry.in_load_order_links[0]; // flink
			iteration++;
		}

		log.r15 = 0x7B32; // tbs.dll not found in module list
		log.r14 = iteration;
		logs::add_log(log);

		return 0;
	}

	std::uint64_t find_tbi_dll_base(cr3 kernel_cr3, cr3 slat_cr3)
	{
		trap_frame_log_t log = {};

		// Get ntoskrnl base
		std::uint64_t ntoskrnl_base = g_ntoskrnl_base_address;
		if (ntoskrnl_base == 0 || ntoskrnl_base < 0xFFFF000000000000ULL)
		{
			log.r15 = 0x7BE1; // ntoskrnl not found
			logs::add_log(log);
			return 0;
		}

		// Find PsLoadedModuleList
		std::uint64_t psloadedmodulelist = find_psloadedmodulelist(ntoskrnl_base, kernel_cr3, slat_cr3);
		if (psloadedmodulelist == 0)
		{
			log.r15 = 0x7BE2; // PsLoadedModuleList not found
			logs::add_log(log);
			return 0;
		}

		// Search for tbs.dll in module list
		return find_tbi_dll_in_module_list(psloadedmodulelist, kernel_cr3, slat_cr3);
	}

	bool try_discover_tbi()
	{
		if (!g_initialized)
		{
			return false;
		}

		// Already found
		if (g_tbi_info.found)
		{
			return true;
		}

		g_vmexit_counter++;

		// Start searching after 50,000 vmexits to ensure system is stable
		if (g_vmexit_counter < 50000)
		{
			return false;
		}

		// Only try discovery every 10,000 vmexits to reduce performance impact
		if ((g_vmexit_counter % 10000) != 0)
		{
			return false;
		}

		trap_frame_log_t log = {};
		log.r15 = 0x7B40; // Starting TBS discovery attempt
		log.r14 = g_vmexit_counter;
		logs::add_log(log);

		cr3 guest_cr3 = arch::get_guest_cr3();
		cr3 slat_cr3 = slat::get_cr3();

		if (guest_cr3.flags == 0 || slat_cr3.flags == 0)
		{
			return false;
		}

		std::uint64_t tbi_base = find_tbi_dll_base(guest_cr3, slat_cr3);

		if (tbi_base != 0)
		{
			// Read size from PE header
			std::uint32_t e_lfanew = 0;
			memory_manager::operate_on_guest_virtual_memory(
				slat_cr3, &e_lfanew, tbi_base + 0x3C, guest_cr3,
				sizeof(e_lfanew), memory_operation_t::read_operation);

			std::uint32_t size_of_image = 0;
			memory_manager::operate_on_guest_virtual_memory(
				slat_cr3, &size_of_image, tbi_base + e_lfanew + 0x50, guest_cr3,
				sizeof(size_of_image), memory_operation_t::read_operation);

			g_tbi_info.base_address = tbi_base;
			g_tbi_info.size = size_of_image;
			g_tbi_info.discovery_vmexit_count = g_vmexit_counter;
			g_tbi_info.found = 1;

			log.r15 = 0x7B41; // TBS discovery successful
			log.r14 = tbi_base;
			log.r13 = size_of_image;
			logs::add_log(log);

			return true;
		}

		return false;
	}

	tbi_info_t get_tbi_info()
	{
		return g_tbi_info;
	}

	std::uint64_t get_tbi_base()
	{
		return g_tbi_info.base_address;
	}

	bool is_tbi_found()
	{
		return g_tbi_info.found != 0;
	}
}
