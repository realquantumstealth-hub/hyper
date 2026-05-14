#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace hook
{
	std::uint8_t set_up();
	void clean_up();

	std::uint8_t add_kernel_hook(std::uint64_t routine_to_hook_virtual, const std::vector<std::uint8_t>& extra_assembled_bytes, const std::vector<uint8_t>& post_original_assembled_bytes);
	std::uint8_t remove_kernel_hook(std::uint64_t hooked_routine_virtual, std::uint8_t do_list_erase);

	struct kernel_hook_info_t
	{
		std::uint64_t original_page_pfn : 36;
		std::uint64_t overflow_original_page_pfn : 36;
		std::uint64_t mapped_shadow_page : 48;
		std::uint64_t detour_holder_shadow_offset : 16;
		std::uint64_t reserved : 56;

		void* get_mapped_shadow_page() const
		{
			return reinterpret_cast<void*>(this->mapped_shadow_page);
		}

		void set_mapped_shadow_page(void* pointer)
		{
			this->mapped_shadow_page = reinterpret_cast<std::uint64_t>(pointer);
		}
	};

	inline std::unordered_map<std::uint64_t, kernel_hook_info_t> kernel_hook_list = { };

	inline std::uint64_t kernel_detour_holder_base = 0;
	inline std::uint64_t kernel_detour_holder_physical_page = 0;
	inline std::uint8_t* kernel_detour_holder_shadow_page_mapped = nullptr;
}
