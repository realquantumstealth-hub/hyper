#pragma once
#include <cstdint>

namespace os_version
{
	enum class windows_version_t : std::uint32_t
	{
		unknown = 0,
		win10_1507 = 10240,
		win10_1511 = 10586,
		win10_1607 = 14393,
		win10_1703 = 15063,
		win10_1709 = 16299,
		win10_1803 = 17134,
		win10_1809 = 17763,
		win10_1903 = 18362,
		win10_1909 = 18363,
		win10_2004 = 19041,
		win10_20h2 = 19042,
		win10_21h1 = 19043,
		win10_21h2 = 19044,
		win10_22h2 = 19045,
		win11_21h2 = 22000,
		win11_22h2 = 22621,
		win11_23h2 = 22631,
		win11_24h2 = 26100,
	};

	struct version_info_t
	{
		std::uint32_t build_number;
		windows_version_t version;
		bool is_24h2_or_newer;
	};

	// EPROCESS structure offsets
	struct eprocess_offsets_t
	{
		std::uint32_t unique_process_id;
		std::uint32_t active_process_links;
		std::uint32_t image_file_name;
		std::uint32_t peb;
		std::uint32_t object_table;
		std::uint32_t inherited_from_unique_process_id;
		std::uint32_t section_base_address;
	};

	version_info_t detect_version();
	std::uint32_t get_build_number();
	eprocess_offsets_t get_eprocess_offsets();
}
