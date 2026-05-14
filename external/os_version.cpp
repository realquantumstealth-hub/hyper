#include "os_version.h"
#include "../memory_manager/memory_manager.h"
#include "../arch/arch.h"
#include "../slat/slat.h"

// KUSER_SHARED_DATA is at a fixed virtual address
constexpr std::uint64_t KUSER_SHARED_DATA_ADDRESS = 0xFFFFF78000000000ULL;
constexpr std::uint32_t NT_BUILD_NUMBER_OFFSET = 0x260;

namespace
{
	os_version::version_info_t cached_version = {};
	bool version_detected = false;
}

std::uint32_t os_version::get_build_number()
{
	// Read from guest kernel memory using memory manager
	cr3 guest_cr3 = arch::get_guest_cr3();
	cr3 slat_cr3 = slat::get_cr3();

	std::uint32_t build_number = 0;
	std::uint64_t kuser_shared_data_addr = KUSER_SHARED_DATA_ADDRESS + NT_BUILD_NUMBER_OFFSET;

	// Read the build number from guest memory
	if (memory_manager::operate_on_guest_virtual_memory(
		slat_cr3,
		&build_number,
		kuser_shared_data_addr,
		guest_cr3,
		sizeof(std::uint32_t),
		memory_operation_t::read_operation) == sizeof(std::uint32_t))
	{
		return build_number & 0xFFFF;  // Lower 16 bits contain build number
	}

	// Fallback: assume pre-24H2 if we can't read
	return 22621;  // Windows 11 22H2 default
}

os_version::version_info_t os_version::detect_version()
{
	if (version_detected)
	{
		return cached_version;
	}

	cached_version.build_number = get_build_number();

	// Determine version enum based on build number
	if (cached_version.build_number >= static_cast<std::uint32_t>(windows_version_t::win11_24h2))
	{
		cached_version.version = windows_version_t::win11_24h2;
		cached_version.is_24h2_or_newer = true;
	}
	else if (cached_version.build_number >= static_cast<std::uint32_t>(windows_version_t::win11_23h2))
	{
		cached_version.version = windows_version_t::win11_23h2;
		cached_version.is_24h2_or_newer = false;
	}
	else if (cached_version.build_number >= static_cast<std::uint32_t>(windows_version_t::win11_22h2))
	{
		cached_version.version = windows_version_t::win11_22h2;
		cached_version.is_24h2_or_newer = false;
	}
	else if (cached_version.build_number >= static_cast<std::uint32_t>(windows_version_t::win11_21h2))
	{
		cached_version.version = windows_version_t::win11_21h2;
		cached_version.is_24h2_or_newer = false;
	}
	else if (cached_version.build_number >= static_cast<std::uint32_t>(windows_version_t::win10_22h2))
	{
		cached_version.version = windows_version_t::win10_22h2;
		cached_version.is_24h2_or_newer = false;
	}
	else
	{
		cached_version.version = windows_version_t::unknown;
		cached_version.is_24h2_or_newer = false;
	}

	version_detected = true;

	return cached_version;
}

os_version::eprocess_offsets_t os_version::get_eprocess_offsets()
{
	auto version = detect_version();

	if (version.is_24h2_or_newer)
	{
		// Windows 11 24H2+ offsets (build 26100+)
		return eprocess_offsets_t{
			.unique_process_id = 0x1D0,
			.active_process_links = 0x1D8,
			.image_file_name = 0x338,
			.peb = 0x2E0,
			.object_table = 0x300,
			.inherited_from_unique_process_id = 0x2D0,
			.section_base_address = 0x2B0,
		};
	}
	else
	{
		// Pre-24H2 offsets (Windows 11 23H2 and below)
		return eprocess_offsets_t{
			.unique_process_id = 0x440,
			.active_process_links = 0x448,
			.image_file_name = 0x5A8,
			.peb = 0x550,
			.object_table = 0x570,
			.inherited_from_unique_process_id = 0x540,
			.section_base_address = 0x520,
		};
	}
}
