#include "system.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "../hypercall/hypercall.h"
#include "../hook/hook.h"

#include <portable_executable/image.hpp>

#include <iostream>
#include <print>
#include <vector>
#include <Windows.h>
#include <winternl.h>
#include <intrin.h>
#include <tlhelp32.h>

#undef min

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread, std::uint8_t* previous_enabled_state);

std::vector<std::uint8_t> dump_kernel_module(std::uint64_t module_base_address)
{
	constexpr std::uint64_t headers_size = 0x1000;
	std::vector<std::uint8_t> headers(headers_size);
	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(headers.data(), module_base_address, sys::current_cr3, headers_size);

	if (bytes_read != headers_size)
	{
		std::println("[!] FAILED: Hypercall read_guest_virtual_memory only read {}/{} bytes for module at 0x{:x}.", bytes_read, headers_size, module_base_address);
		return {};
	}

	std::uint16_t magic = *reinterpret_cast<std::uint16_t*>(headers.data());
	if (magic != 0x5a4d) // 'MZ' magic number
	{
		std::println("[!] FAILED: Invalid PE header. Magic number was 0x{:x}, expected 0x5a4d for module at 0x{:x}.", magic, module_base_address);
		return {};
	}

	const portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(headers.data());
	const std::uint32_t image_size = image->nt_headers()->optional_header.size_of_image;

	std::vector<std::uint8_t> image_buffer(image_size);
	memcpy(image_buffer.data(), headers.data(), headers_size);

	// This part is less critical for initial diagnosis but we'll leave it
	for (const auto& current_section : image->sections())
	{
		std::uint64_t read_offset = current_section.virtual_address;
		std::uint64_t read_size = current_section.virtual_size;
		if (read_offset + read_size > image_size) {
		//	std::println("[!] Warning: Section {} is out of bounds. Skipping.", current_section.name);
			continue;
		}
		hypercall::read_guest_virtual_memory(image_buffer.data() + read_offset, module_base_address + read_offset, sys::current_cr3, read_size);
	}

	return image_buffer;
}
std::uint64_t find_kernel_detour_holder_base_address(portable_executable::image_t* ntoskrnl, std::uint64_t ntoskrnl_base_address)
{
	std::println("[DEBUG] Searching for kernel detour holder base address...");
	std::println("[DEBUG] ntoskrnl_base_address: 0x{:x}", ntoskrnl_base_address);

	int section_count = 0;
	// The ntoskrnl->sections() call might be returning a malformed object
	// if the dump is bad.
	try
	{
		for (const auto& current_section : ntoskrnl->sections())
		{
			// ADD THIS LINE: Print BEFORE you try to access the section's members.
			std::println("[DEBUG] Processing section #{}", section_count + 1);

			std::string_view current_section_name(current_section.name);
			section_count++;

			std::println("[DEBUG] Section {}: name='{}', virtual_address=0x{:x}, mem_execute={}",
				section_count, current_section_name, current_section.virtual_address, current_section.characteristics.mem_execute);

			if (current_section_name.contains("Pad") == true && current_section.characteristics.mem_execute == 1)
			{
				std::uint64_t detour_base = ntoskrnl_base_address + current_section.virtual_address;
				std::println("[+] Found kernel detour holder at: 0x{:x}", detour_base);
				return detour_base;
			}
		}
	}
	catch (const std::exception& e)
	{
		// This might catch an error from the PE parsing library itself.
		std::println("[!] CRASH CAUGHT: Exception while parsing sections: {}", e.what());
	}

	std::println("[!] FAILED: No suitable Pad section found for kernel detour holder");
	std::println("[DEBUG] Total sections scanned: {}", section_count);
	return 0;
}
std::unordered_map<std::string, std::uint64_t> parse_module_exports(const portable_executable::image_t* image, const std::string& module_name, const std::uint64_t module_base_address)
{
	std::unordered_map<std::string, std::uint64_t> exports = { };

	for (const auto& current_export : image->exports())
	{
		std::string current_export_name = module_name + "!" + current_export.name;

		std::uint64_t delta = reinterpret_cast<std::uint64_t>(current_export.address) - image->as<std::uint64_t>();

		exports[current_export_name] = module_base_address + delta;
	}

	return exports;
}

bool strings_equal_ignore_case(const std::string& a, const std::string& b) {
	return std::equal(a.begin(), a.end(),
		b.begin(), b.end(),
		[](char a, char b) {
			return tolower(a) == tolower(b);
		});
}

// ========================================================================
// REPLACE the existing get_kernel_export function with this new one
// ========================================================================
std::uint64_t get_kernel_export(const std::vector<std::uint8_t>& module_dump, std::uint64_t module_base_address, const char* export_name)
{
	if (module_dump.empty())
	{
		return 0;
	}

	const portable_executable::image_t* image = reinterpret_cast<const portable_executable::image_t*>(module_dump.data());

	// --- Start of New Diagnostic Code ---
	auto exports = image->exports();

	int exports_to_show = 100;
	// --- End of New Diagnostic Code ---

	for (const auto& current_export : exports)
	{
		// --- More Diagnostic Code ---
		if (exports_to_show > 0) {
			std::println("  - Found export: {}", current_export.name);
			exports_to_show--;
		}
		// --- End ---

		if (strings_equal_ignore_case(current_export.name, export_name))
		{
			// This part is likely correct, but we're not reaching it.
			std::uint64_t rva = reinterpret_cast<std::uint64_t>(current_export.address) - image->as<std::uint64_t>();
			std::println("[+] SUCCESS: Found '{}' at RVA 0x{:x}", export_name, rva);
			return module_base_address + rva;
		}
	}

	printf("[!] FAILED: Scanned all {} exports but did not find '{}'.", exports.end(), export_name);
	return 0; // Export not found
}

std::uint64_t find_pattern(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& pattern, const std::vector<bool>& mask)
{
	for (size_t i = 0; i < data.size() - pattern.size(); ++i)
	{
		bool found = true;
		for (size_t j = 0; j < pattern.size(); ++j)
		{
			if (mask[j] && data[i + j] != pattern[j])
			{
				found = false;
				break;
			}
		}
		if (found)
		{
			return i;
		}
	}
	return -1; // Not found
}

std::uint64_t find_ps_active_process_head_by_signature(const std::vector<std::uint8_t>& ntoskrnl_dump, std::uint64_t ntoskrnl_base)
{
	// This is the correct signature for:
	// LEA r12, PsActiveProcessHead
	// This instruction calculates the ADDRESS of the variable, which is what we need.
	const std::vector<std::uint8_t> pattern = { 0x4C, 0x8D, 0x25 };
	const std::vector<bool> mask = { true, true, true };

	std::println("[+] Scanning ntoskrnl dump for PsActiveProcessHead LEA signature...");

	std::uint64_t offset = find_pattern(ntoskrnl_dump, pattern, mask);

	if (offset == -1)
	{
		std::println("[!] FAILED: PsActiveProcessHead LEA signature not found in dump.");
		return 0;
	}

	std::println("[+] Found LEA signature at dump offset 0x{:x}", offset);

	// The calculation logic remains identical because this is also a 7-byte RIP-relative instruction.
	std::uint64_t instruction_rva = offset;
	std::uint64_t next_instruction_rva = instruction_rva + 7;
	std::int32_t relative_offset = *reinterpret_cast<const std::int32_t*>(&ntoskrnl_dump[offset + 3]);
	std::uint64_t ps_active_process_head_rva = next_instruction_rva + relative_offset;
	std::uint64_t final_address = ntoskrnl_base + ps_active_process_head_rva;

	std::println("[+] Calculated PsActiveProcessHead GVA: 0x{:x}", final_address);
	return final_address;
}

std::uint64_t find_mm_physical_memory_block_by_signature(const std::vector<std::uint8_t>& ntoskrnl_dump, std::uint64_t ntoskrnl_base)
{
	std::println("[+] Scanning ntoskrnl dump for MmPhysicalMemoryBlock using specific signature...");

	// Use the specific signature from your analysis: \x48\x8B\x1D\x04\x97\x9C\x00
	// This is more specific than just "48 8B 1D" to avoid false matches
	const std::vector<std::uint8_t> pattern = { 0x48, 0x8B, 0x1D, 0x04, 0x97, 0x9C, 0x00 };
	const std::vector<bool> mask = { true, true, true, true, true, true, true };

	std::uint64_t offset = find_pattern(ntoskrnl_dump, pattern, mask);
	
	if (offset == -1)
	{
		std::println("[!] FAILED: Specific MmPhysicalMemoryBlock signature not found in ntoskrnl dump");
		return 0;
	}

	// Calculate the target address for this RIP-relative instruction
	std::uint64_t instruction_rva = offset;
	std::uint64_t next_instruction_rva = instruction_rva + 7;  // 3 bytes opcode + 4 bytes offset
	std::int32_t relative_offset = *reinterpret_cast<const std::int32_t*>(&ntoskrnl_dump[offset + 3]);
	std::uint64_t target_rva = next_instruction_rva + relative_offset;
	std::uint64_t target_address = ntoskrnl_base + target_rva;

	std::println("[+] Found specific MOV rbx, cs:MmPhysicalMemoryBlock at offset 0x{:x}, target: 0x{:x}", offset, target_address);
	
	// Validate that this looks like a reasonable kernel data address
	if (target_rva > 0x100000 && target_rva < ntoskrnl_dump.size() - 8)
	{
		std::println("[+] MmPhysicalMemoryBlock variable found at: 0x{:x}", target_address);
		return target_address;
	}
	else
	{
		std::println("[!] Warning: MmPhysicalMemoryBlock target address 0x{:x} seems invalid (RVA: 0x{:x})", 
			target_address, target_rva);
		return 0;
	}
}

void add_module_to_list(const std::string& module_name, const std::vector<std::uint8_t>& module_dump, const std::uint64_t module_base_address, const std::uint32_t module_size)
{
	sys::kernel_module_t kernel_module = { };

	const portable_executable::image_t* image = reinterpret_cast<const portable_executable::image_t*>(module_dump.data());

	kernel_module.exports = parse_module_exports(image, module_name, module_base_address);
	kernel_module.base_address = module_base_address;
	kernel_module.size = module_size;

	sys::kernel::modules_list[module_name] = kernel_module;
}

void erase_unused_modules(const std::unordered_map<std::string, sys::kernel_module_t>& modules_not_found)
{
	for (const auto& [module_name, module_info] : modules_not_found)
	{
		sys::kernel::modules_list.erase(module_name);
	}
}

// requires SeDebugPriviledge, use PsLoadedModulesList instead unless if using before ntoskrnl.exe is parsed
std::vector<rtl_process_module_information_t> get_loaded_modules_priviledged()
{
	std::uint32_t size_of_information = 0;

	sys::user::query_system_information(11, nullptr, 0, &size_of_information);

	if (size_of_information == 0)
	{
		return { };
	}

	std::vector<std::uint8_t> buffer(size_of_information);

	std::uint32_t status = sys::user::query_system_information(11, buffer.data(), size_of_information, &size_of_information);

	if (NT_SUCCESS(status) == false)
	{
		return { };
	}

	rtl_process_modules_t* process_modules = reinterpret_cast<rtl_process_modules_t*>(buffer.data());

	rtl_process_module_information_t* start = &process_modules->modules[0];
	rtl_process_module_information_t* end = start + process_modules->module_count;

	return { start, end };
}

template <class t>
t read_kernel_virtual_memory(std::uint64_t address)
{
	t buffer = t();

	hypercall::read_guest_virtual_memory(&buffer, address, sys::current_cr3, sizeof(t));

	return buffer;
}

std::wstring read_unicode_string(std::uint64_t address)
{
	std::uint16_t length = read_kernel_virtual_memory<std::uint16_t>(address);

	if (length == 0)
	{
		return { };
	}

	std::uint64_t buffer_address = read_kernel_virtual_memory<std::uint64_t>(address + 8);

	std::wstring string(length / 2, L'\0');

	hypercall::read_guest_virtual_memory(string.data(), buffer_address, sys::current_cr3, length);

	return string;
}

std::uint64_t get_ps_loaded_module_list()
{
	const std::string ntoskrnl_name = "ntoskrnl.exe";

	if (sys::kernel::modules_list.contains(ntoskrnl_name) == 0)
	{
		return 0;
	}

	sys::kernel_module_t& ntoskrnl = sys::kernel::modules_list[ntoskrnl_name];

	const std::string ps_loaded_module_list_name = "ntoskrnl.exe!PsLoadedModuleList";

	return ntoskrnl.exports[ps_loaded_module_list_name];
}

std::uint8_t sys::kernel::parse_modules()
{
	const std::uint64_t ps_loaded_module_list = get_ps_loaded_module_list();

	if (ps_loaded_module_list == 0)
	{
		std::println("can't locate PsLoadedModuleList");

		return 0;
	}

	std::unordered_map<std::string, kernel_module_t> modules_not_found = modules_list;

	const std::uint64_t start_entry = ps_loaded_module_list;

	std::uint64_t current_entry = read_kernel_virtual_memory<std::uint64_t>(start_entry); // flink

	while (current_entry != start_entry)
	{
		kernel_module_t kernel_module = { };

		std::uint64_t module_base_address = read_kernel_virtual_memory<std::uint64_t>(current_entry + 0x30); // DllBase
		std::uint32_t module_size = read_kernel_virtual_memory<std::uint32_t>(current_entry + 0x40); // SizeOfImage
		std::string module_name = user::to_string(read_unicode_string(current_entry + 0x58)); // BaseDllName

		// current_entry must not be accessed after this point in this iteration
		current_entry = read_kernel_virtual_memory<std::uint64_t>(current_entry); // flink

		if (modules_list.contains(module_name) == true)
		{
			modules_not_found.erase(module_name);

			const kernel_module_t already_present_module = modules_list[module_name];

			if (already_present_module.base_address == module_base_address && already_present_module.size == module_size)
			{
				continue;
			}
		}

		std::vector<std::uint8_t> module_dump = dump_kernel_module(module_base_address);

		if (module_dump.empty() == true)
		{
			continue;
		}

		add_module_to_list(module_name, module_dump, module_base_address, module_size);
	}

	erase_unused_modules(modules_not_found);

	return 1;
}

void fix_dump(std::vector<std::uint8_t>& buffer)
{
	portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(buffer.data());

	for (auto& current_section : image->sections())
	{
		current_section.pointer_to_raw_data = current_section.virtual_address;
		current_section.size_of_raw_data = current_section.virtual_size;
	}
}

std::uint8_t sys::kernel::dump_module_to_disk(const std::string_view target_module_name, const std::string_view output_directory)
{
	const auto module_info = modules_list[target_module_name.data()];

	const std::uint64_t module_base_address = module_info.base_address;

	if (module_base_address == 0)
	{
		return 0;
	}

	std::vector<std::uint8_t> buffer = dump_kernel_module(module_base_address);

	if (buffer.empty() == 1)
	{
		return 0;
	}

	fix_dump(buffer);

	std::string output_path = std::string(output_directory) + "\\" + "dump_" + std::string(target_module_name);

	return fs::write_to_disk(output_path, buffer);
}

struct ntoskrnl_information_t
{
	std::uint64_t base_address;
	std::uint32_t size;

	std::vector<std::uint8_t> dump;
};

std::optional<ntoskrnl_information_t> load_ntoskrnl_information()
{
	std::uint8_t desired_privilege_state = 1;
	std::uint8_t previous_privilege_state = 0;

	std::println("[+] Attempting to acquire SeDebugPrivilege...");
	if (sys::user::set_debug_privilege(desired_privilege_state, &previous_privilege_state) == 0)
	{
		std::println("[!] FAILED: Unable to acquire necessary privilege. Please run as Administrator.");
		return std::nullopt;
	}
	std::println("[+] SeDebugPrivilege acquired successfully.");

	const std::vector<rtl_process_module_information_t> loaded_modules = get_loaded_modules_priviledged();
	sys::user::set_debug_privilege(previous_privilege_state, &desired_privilege_state); // Revert privilege

	if (loaded_modules.empty()) {
		std::println("[!] FAILED: NtQuerySystemInformation returned an empty module list.");
		return std::nullopt;
	}
	std::println("[+] Received module list with {} entries.", loaded_modules.size());

	for (const rtl_process_module_information_t& current_module : loaded_modules)
	{
		std::string_view current_module_name = reinterpret_cast<const char*>(current_module.full_path_name + current_module.offset_to_file_name);

		// Let's print the first few modules to see what we are getting
		if (loaded_modules.data() == &current_module || &current_module < loaded_modules.data() + 5) {
			std::println("  - Found module: {}", current_module_name);
		}

		if (current_module_name == "ntoskrnl.exe")
		{
			std::println("[+] Found ntoskrnl.exe in module list. Base: 0x{:x}, Size: 0x{:x}", current_module.image_base, current_module.image_size);
			std::vector<std::uint8_t> ntoskrnl_dump = dump_kernel_module(current_module.image_base);

			if (ntoskrnl_dump.empty() == true)
			{
				std::println("[!] FAILED: dump_kernel_module returned empty buffer for ntoskrnl.exe.");
				return std::nullopt;
			}

			ntoskrnl_information_t ntoskrnl_info = { };
			ntoskrnl_info.base_address = current_module.image_base;
			ntoskrnl_info.size = current_module.image_size;
			ntoskrnl_info.dump = ntoskrnl_dump;

			return ntoskrnl_info;
		}
	}

	std::println("[!] FAILED: Scanned entire module list but did not find 'ntoskrnl.exe'.");
	return std::nullopt;
}

std::uint8_t parse_ntoskrnl()
{
	std::optional<ntoskrnl_information_t> ntoskrnl_info = load_ntoskrnl_information();

	if (ntoskrnl_info.has_value() == 0)
	{
		std::println("unable to load ntoskrnl.exe's information");
		return 0;
	}

	std::vector<std::uint8_t>& ntoskrnl_dump = ntoskrnl_info->dump;
	portable_executable::image_t* ntoskrnl_image = reinterpret_cast<portable_executable::image_t*>(ntoskrnl_dump.data());

	// We still add ntoskrnl to the list to get other exports if needed later
	add_module_to_list("ntoskrnl.exe", ntoskrnl_dump, ntoskrnl_info->base_address, ntoskrnl_info->size);

	// FIX: Replace the failing get_kernel_export call with our new signature scan
	sys::ps_active_process_head = find_ps_active_process_head_by_signature(ntoskrnl_dump, ntoskrnl_info->base_address);

	if (sys::ps_active_process_head == 0)
	{
		// The new function already prints the error, so we can simplify this.
		return 0;
	}
	
	// Also get PsInitialSystemProcess and MmPhysicalMemoryBlock
	const std::string ntoskrnl_name = "ntoskrnl.exe";
	if (sys::kernel::modules_list.contains(ntoskrnl_name))
	{
		sys::kernel_module_t& ntoskrnl = sys::kernel::modules_list[ntoskrnl_name];
		
		// Get PsInitialSystemProcess
		const std::string ps_initial_system_process_name = "ntoskrnl.exe!PsInitialSystemProcess";
		sys::ps_initial_system_process = ntoskrnl.exports[ps_initial_system_process_name];
		if (sys::ps_initial_system_process == 0)
		{
			std::println("[!] Failed to find PsInitialSystemProcess export");
		}
		else
		{
			std::println("[+] Found PsInitialSystemProcess at 0x{:x}", sys::ps_initial_system_process);
		}
		
		// Get MmPhysicalMemoryBlock using pattern scanning (it's not exported)
		std::uint64_t mm_physical_memory_block_ptr = find_mm_physical_memory_block_by_signature(ntoskrnl_dump, ntoskrnl_info->base_address);
		if (mm_physical_memory_block_ptr == 0)
		{
			std::println("[!] Failed to find MmPhysicalMemoryBlock using pattern scanning");
		}
		else
		{
			std::println("[+] Found MmPhysicalMemoryBlock variable at 0x{:x}", mm_physical_memory_block_ptr);
			
			// Debug: Let's examine the bytes around this address to see if it looks right
			std::uint64_t debug_buffer[4] = {0};
			hypercall::read_guest_virtual_memory(debug_buffer, mm_physical_memory_block_ptr, sys::current_cr3, sizeof(debug_buffer));
			std::println("[DEBUG] Memory at 0x{:x}: {:016x} {:016x} {:016x} {:016x}", 
				mm_physical_memory_block_ptr, debug_buffer[0], debug_buffer[1], debug_buffer[2], debug_buffer[3]);
			
			// MmPhysicalMemoryBlock is a pointer variable, so we need to read its value
			// to get the address of the PHYSICAL_MEMORY_DESCRIPTOR structure
			sys::mm_physical_memory_block = read_kernel_virtual_memory<std::uint64_t>(mm_physical_memory_block_ptr);
			std::println("[DEBUG] Raw value read: 0x{:x}", sys::mm_physical_memory_block);
			
			if (sys::mm_physical_memory_block == 0)
			{
				std::println("[!] MmPhysicalMemoryBlock contains null pointer");
			}
			else if (sys::mm_physical_memory_block < 0xffff800000000000ULL)
			{
				std::println("[!] MmPhysicalMemoryBlock contains invalid address: 0x{:x} (should be kernel range >= 0xffff800000000000)", sys::mm_physical_memory_block);
				sys::mm_physical_memory_block = 0; // Reset to 0 to indicate failure
			}
			else
			{
				std::println("[+] MmPhysicalMemoryBlock points to PHYSICAL_MEMORY_DESCRIPTOR at: 0x{:x}", sys::mm_physical_memory_block);
			}
		}
	}

	hook::kernel_detour_holder_base = find_kernel_detour_holder_base_address(ntoskrnl_image, ntoskrnl_info->base_address);

	if (hook::kernel_detour_holder_base == 0)
	{
		std::println("unable to locate kernel hook holder");
		return 0;
	}

	return 1;
}

std::uint8_t sys::set_up()
{
	std::println("[DEBUG] Starting sys::set_up()");
	
	// Detect CPU vendor for debugging
	std::println("[DEBUG] Detecting CPU vendor...");
	int cpu_info[4];
	__cpuid(cpu_info, 0);
	char vendor_string[13];
	memcpy(vendor_string, &cpu_info[1], 4);
	memcpy(vendor_string + 4, &cpu_info[3], 4);
	memcpy(vendor_string + 8, &cpu_info[2], 4);
	vendor_string[12] = '\0';
	std::println("[+] CPU Vendor: {}", vendor_string);

	std::println("[DEBUG] About to call hypercall::read_guest_cr3()...");
	current_cr3 = hypercall::read_guest_cr3();
	std::println("[DEBUG] hypercall::read_guest_cr3() returned: 0x{:X}", current_cr3);

	if (current_cr3 == 0)
	{
		std::println("hyperv-attachment doesn't seem to be loaded");

		return 0;
	}

	std::println("[DEBUG] About to parse ntoskrnl...");
	if (parse_ntoskrnl() == 0)
	{
		std::println("unable to parse ntoskrnl.exe");

		return 0;
	}
	std::println("[DEBUG] ntoskrnl parsing completed successfully");

	std::println("[DEBUG] About to parse kernel modules...");
	if (kernel::parse_modules() == 0)
	{
		std::println("unable to parse kernel modules");

		return 0;
	}
	std::println("[DEBUG] kernel modules parsing completed successfully");

	std::println("[DEBUG] About to set up kernel hook helper...");
	if (hook::set_up() == 0)
	{
		std::println("unable to set up kernel hook helper");

		return 0;
	}
	std::println("[DEBUG] kernel hook helper setup completed successfully");

	std::println("[DEBUG] sys::set_up() completed successfully");
	return 1;
}

void sys::clean_up()
{
	hook::clean_up();
}

std::uint32_t sys::user::query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size, std::uint32_t* returned_size)
{
	return NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(information_class), information_out, information_size, reinterpret_cast<ULONG*>(returned_size));
}

std::uint32_t sys::user::adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only, std::uint8_t* previous_enabled_state)
{
	return RtlAdjustPrivilege(privilege, enable, current_thread_only, previous_enabled_state);
}

std::uint8_t sys::user::set_debug_privilege(const std::uint8_t state, std::uint8_t* previous_state)
{
	constexpr std::uint32_t debug_privilege_id = 20;

	std::uint32_t status = adjust_privilege(debug_privilege_id, state, 0, previous_state);

	return NT_SUCCESS(status);
}

void* sys::user::allocate_locked_memory(std::uint64_t size, std::uint32_t protection)
{
	void* allocation_base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protection);

	if (allocation_base == nullptr)
	{
		return nullptr;
	}

	std::int32_t lock_status = VirtualLock(allocation_base, size);

	if (lock_status == 0)
	{
		free_memory(allocation_base);

		return nullptr;
	}

	return allocation_base;
}

std::uint8_t sys::user::free_memory(void* address)
{
	std::int32_t free_status = VirtualFree(address, 0, MEM_RELEASE);

	return free_status != 0;
}

std::string sys::user::to_string(const std::wstring& wstring)
{
	if (wstring.empty() == 1)
	{
		return { };
	}

	std::string converted_string = { };

	std::ranges::transform(wstring,
		std::back_inserter(converted_string), [](wchar_t character)
		{
			return static_cast<char>(character);
		});

	return converted_string;
}

std::uint8_t sys::fs::exists(std::string_view path)
{
	return std::filesystem::exists(path);
}

std::uint8_t sys::fs::write_to_disk(const std::string_view full_path, const std::vector<std::uint8_t>& buffer)
{
	std::ofstream file(full_path.data(),std::ios::binary);

	if (file.is_open() == 0)
	{
		return 0;
	}

	file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

	return file.good();
}

// Process enumeration and scanning functions
std::uint32_t sys::get_pid_from_process_name(const std::string& process_name)
{
	// Use Windows API to enumerate processes
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 process_entry = {};
	process_entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &process_entry)) {
		do {
			// Convert WCHAR to std::string
			std::wstring wide_name = process_entry.szExeFile;
			std::string current_process_name = sys::user::to_string(wide_name);
			if (current_process_name == process_name) {
				CloseHandle(snapshot);
				return process_entry.th32ProcessID;
			}
		} while (Process32Next(snapshot, &process_entry));
	}

	CloseHandle(snapshot);
	return 0; // Process not found
}

std::uint64_t sys::get_process_base_address(std::uint32_t pid)
{
	// Use hypervisor to get process base address
	if (sys::ps_initial_system_process == 0) {
		return 0;
	}

	return hypercall::get_process_base(pid, sys::ps_initial_system_process, sys::current_cr3);
}

std::uint32_t sys::get_image_size(std::uint64_t base_address)
{
	// Read PE headers from process memory to get image size
	if (base_address == 0) {
		return 0;
	}

	// Read DOS header first
	std::vector<std::uint8_t> dos_header(64);
	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(dos_header.data(), base_address, sys::current_cr3, dos_header.size());
	if (bytes_read != dos_header.size()) {
		return 0;
	}

	// Check DOS signature
	std::uint16_t dos_signature = *reinterpret_cast<std::uint16_t*>(dos_header.data());
	if (dos_signature != 0x5A4D) { // 'MZ'
		return 0;
	}

	// Get PE header offset
	std::uint32_t pe_offset = *reinterpret_cast<std::uint32_t*>(dos_header.data() + 60);

	// Read NT headers
	std::vector<std::uint8_t> nt_headers(256); // Should be enough for NT headers
	bytes_read = hypercall::read_guest_virtual_memory(nt_headers.data(), base_address + pe_offset, sys::current_cr3, nt_headers.size());
	if (bytes_read != nt_headers.size()) {
		return 0;
	}

	// Check PE signature
	std::uint32_t pe_signature = *reinterpret_cast<std::uint32_t*>(nt_headers.data());
	if (pe_signature != 0x00004550) { // 'PE\0\0'
		return 0;
	}

	// Get SizeOfImage from Optional Header
	// PE signature (4) + File Header (20) + SizeOfImage offset in Optional Header (56)
	std::uint32_t size_of_image = *reinterpret_cast<std::uint32_t*>(nt_headers.data() + 4 + 20 + 56);

	return size_of_image;
}

constexpr std::uint64_t EPROCESS_PEB_OFFSET = 0x550;

std::uint64_t sys::get_peb_address(std::uint32_t target_pid, std::uint64_t kernel_cr3, std::uint64_t ps_initial_system_process) {
	// FIX: Call the new hypercall to get the EPROCESS base.
	uint64_t eprocess_base = hypercall::get_eprocess_base(target_pid, ps_initial_system_process, kernel_cr3);
	if (eprocess_base == 0) {
		return 0;
	}

	uint64_t peb_address = 0;
	hypercall::read_guest_virtual_memory(&peb_address, eprocess_base + EPROCESS_PEB_OFFSET, kernel_cr3, sizeof(peb_address));
	return peb_address;
}
std::uint64_t sys::find_target_module_base(std::uint32_t target_pid, std::uint64_t target_cr3, const std::string& module_name) {
	// FIX: Provide a minimal, local definition of LDR_DATA_TABLE_ENTRY
	// to avoid compiler/header issues. The offsets are standard for x64.
	struct LDR_DATA_TABLE_ENTRY_MINIMAL {
		LIST_ENTRY InMemoryOrderLinks;          // 0x00
		LIST_ENTRY InLoadOrderLinks;            // 0x10
		LIST_ENTRY InInitializationOrderLinks;  // 0x20
		PVOID DllBase;                          // 0x30
		PVOID EntryPoint;                       // 0x38
		ULONG SizeOfImage;                      // 0x40
		UNICODE_STRING FullDllName;             // 0x48
		UNICODE_STRING BaseDllName;             // 0x58
	};

	PEB peb;
	std::uint64_t peb_address = get_peb_address(target_pid, sys::current_cr3, sys::ps_initial_system_process);
	if (peb_address == 0) return 0;
	if (hypercall::read_guest_virtual_memory(&peb, peb_address, target_cr3, sizeof(peb)) != sizeof(peb)) return 0;

	PEB_LDR_DATA ldr_data;
	if (hypercall::read_guest_virtual_memory(&ldr_data, (uint64_t)peb.Ldr, target_cr3, sizeof(ldr_data)) != sizeof(ldr_data)) return 0;

	LIST_ENTRY* current_entry = ldr_data.InMemoryOrderModuleList.Flink;
	LIST_ENTRY* list_head = (LIST_ENTRY*)((uint64_t)peb.Ldr + offsetof(PEB_LDR_DATA, InMemoryOrderModuleList));

	while (current_entry != list_head) {
		LDR_DATA_TABLE_ENTRY_MINIMAL table_entry; // Use our minimal struct
		uint64_t table_entry_addr = (uint64_t)CONTAINING_RECORD(current_entry, LDR_DATA_TABLE_ENTRY_MINIMAL, InMemoryOrderLinks);

		if (hypercall::read_guest_virtual_memory(&table_entry, table_entry_addr, target_cr3, sizeof(table_entry)) != sizeof(table_entry)) {
			return 0;
		}

		if (table_entry.BaseDllName.Length > 0) {
			std::wstring wide_name(table_entry.BaseDllName.Length / sizeof(wchar_t), L'\0');
			hypercall::read_guest_virtual_memory(wide_name.data(), (uint64_t)table_entry.BaseDllName.Buffer, target_cr3, table_entry.BaseDllName.Length);

			std::string current_module_name = user::to_string(wide_name);
			if (_stricmp(current_module_name.c_str(), module_name.c_str()) == 0) {
				return (uint64_t)table_entry.DllBase;
			}
		}
		current_entry = table_entry.InMemoryOrderLinks.Flink;
	}

	return 0;
}

// Parses the export table of a module in the target process to find a function's address.
std::uint64_t sys::find_export_address_in_target(std::uint64_t target_module_base, std::uint64_t target_cr3, const std::string& function_name) {
    if (target_module_base == 0) return 0;

    IMAGE_DOS_HEADER dos_header;
    hypercall::read_guest_virtual_memory(&dos_header, target_module_base, target_cr3, sizeof(dos_header));
    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 nt_headers;
    hypercall::read_guest_virtual_memory(&nt_headers, target_module_base + dos_header.e_lfanew, target_cr3, sizeof(nt_headers));
    if (nt_headers.Signature != IMAGE_NT_SIGNATURE) return 0;

    IMAGE_DATA_DIRECTORY& export_dir_entry = nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (export_dir_entry.VirtualAddress == 0) return 0;

    IMAGE_EXPORT_DIRECTORY export_dir;
    hypercall::read_guest_virtual_memory(&export_dir, target_module_base + export_dir_entry.VirtualAddress, target_cr3, sizeof(export_dir));

    uint64_t names_rva_ptr = target_module_base + export_dir.AddressOfNames;
    uint64_t ordinals_rva_ptr = target_module_base + export_dir.AddressOfNameOrdinals;
    uint64_t functions_rva_ptr = target_module_base + export_dir.AddressOfFunctions;

    for (DWORD i = 0; i < export_dir.NumberOfNames; i++) {
        uint32_t name_rva;
        hypercall::read_guest_virtual_memory(&name_rva, names_rva_ptr + i * sizeof(uint32_t), target_cr3, sizeof(name_rva));

        char current_func_name[256] = { 0 };
        hypercall::read_guest_virtual_memory(current_func_name, target_module_base + name_rva, target_cr3, sizeof(current_func_name) - 1);

        if (_stricmp(current_func_name, function_name.c_str()) == 0) {
            uint16_t ordinal;
            hypercall::read_guest_virtual_memory(&ordinal, ordinals_rva_ptr + i * sizeof(uint16_t), target_cr3, sizeof(ordinal));

            uint32_t function_rva;
            hypercall::read_guest_virtual_memory(&function_rva, functions_rva_ptr + ordinal * sizeof(uint32_t), target_cr3, sizeof(function_rva));

            return target_module_base + function_rva;
        }
    }

    return 0; // Function not found
}

void* sys::get_ntoskrnl_base()
{
    // Use the existing kernel module infrastructure to get ntoskrnl.exe base address
    const std::string ntoskrnl_name = "ntoskrnl.exe";
    
    // Check if ntoskrnl.exe is already in the modules list
    if (sys::kernel::modules_list.contains(ntoskrnl_name))
    {
        sys::kernel_module_t& ntoskrnl = sys::kernel::modules_list[ntoskrnl_name];
        std::cout << "Found ntoskrnl.exe at: 0x" << std::hex << ntoskrnl.base_address << std::endl;
        return reinterpret_cast<void*>(ntoskrnl.base_address);
    }
    
    // If not found, try to load the ntoskrnl information
    std::optional<ntoskrnl_information_t> ntoskrnl_info = load_ntoskrnl_information();
    if (ntoskrnl_info.has_value())
    {
        std::cout << "Found ntoskrnl.exe at: 0x" << std::hex << ntoskrnl_info->base_address << std::endl;
        return reinterpret_cast<void*>(ntoskrnl_info->base_address);
    }
    
    std::cout << "Failed to find ntoskrnl.exe base address" << std::endl;
    return nullptr;
}

std::uint64_t sys::scan_memory(std::uint64_t base_address, std::uint32_t size, const std::uint8_t* pattern, std::size_t pattern_size)
{
	constexpr std::uint32_t chunk_size = 0x1000; // 4KB chunks
	std::vector<std::uint8_t> buffer(chunk_size);

	for (std::uint32_t offset = 0; offset < size; offset += chunk_size) {
		std::uint32_t read_size = std::min(chunk_size, size - offset);
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			buffer.data(), base_address + offset, sys::current_cr3, read_size);

		if (bytes_read != read_size) {
			continue; // Skip unreadable memory
		}

		// Search for pattern in this chunk
		for (std::uint32_t i = 0; i <= read_size - pattern_size; ++i) {
			bool match = true;
			for (std::size_t j = 0; j < pattern_size; ++j) {
				// 0xCC in pattern means wildcard
				if (pattern[j] != 0xCC && buffer[i + j] != pattern[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				std::println("[+] UWorld pattern found at: 0x{:X}", base_address + offset + i);
				return base_address + offset + i;
			}
		}
	}

	return 0; // Pattern not found
}

std::uint32_t sys::get_image_size(std::uint64_t base_address, std::uint64_t process_cr3)
{
	// Read PE headers from process memory to get image size (using specific process CR3)
	if (base_address == 0) {
		return 0;
	}

	// Read DOS header first
	std::vector<std::uint8_t> dos_header(64);
	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(dos_header.data(), base_address, process_cr3, dos_header.size());
	if (bytes_read != dos_header.size()) {
		return 0;
	}

	// Check DOS signature
	std::uint16_t dos_signature = *reinterpret_cast<std::uint16_t*>(dos_header.data());
	if (dos_signature != 0x5A4D) { // 'MZ'
		return 0;
	}

	// Get PE header offset
	std::uint32_t pe_offset = *reinterpret_cast<std::uint32_t*>(dos_header.data() + 60);

	// Read NT headers
	std::vector<std::uint8_t> nt_headers(256); // Should be enough for NT headers
	bytes_read = hypercall::read_guest_virtual_memory(nt_headers.data(), base_address + pe_offset, process_cr3, nt_headers.size());
	if (bytes_read != nt_headers.size()) {
		return 0;
	}

	// Check PE signature
	std::uint32_t pe_signature = *reinterpret_cast<std::uint32_t*>(nt_headers.data());
	if (pe_signature != 0x00004550) { // 'PE\0\0'
		return 0;
	}

	// Get SizeOfImage from Optional Header
	// PE signature (4) + File Header (20) + SizeOfImage offset in Optional Header (56)
	std::uint32_t size_of_image = *reinterpret_cast<std::uint32_t*>(nt_headers.data() + 4 + 20 + 56);

	return size_of_image;
}

std::uint64_t sys::scan_memory(std::uint64_t base_address, std::uint32_t size, const std::uint8_t* pattern, std::size_t pattern_size, std::uint64_t process_cr3)
{
	constexpr std::uint32_t chunk_size = 0x1000; // 4KB chunks
	std::vector<std::uint8_t> buffer(chunk_size);

	for (std::uint32_t offset = 0; offset < size; offset += chunk_size) {
		std::uint32_t read_size = std::min(chunk_size, size - offset);
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			buffer.data(), base_address + offset, process_cr3, read_size);

		if (bytes_read != read_size) {
			continue; // Skip unreadable memory
		}

		// Search for pattern in this chunk
		for (std::uint32_t i = 0; i <= read_size - pattern_size; ++i) {
			bool match = true;
			for (std::size_t j = 0; j < pattern_size; ++j) {
				// 0xCC in pattern means wildcard
				if (pattern[j] != 0xCC && buffer[i + j] != pattern[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				std::println("[+] UWorld pattern found at: 0x{:X}", base_address + offset + i);
				return base_address + offset + i;
			}
		}
	}

	return 0; // Pattern not found
}
