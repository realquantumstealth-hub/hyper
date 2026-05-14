#pragma once
#include <cstdint>

namespace heap_manager
{
	void set_up(std::uint64_t heap_base, std::uint64_t heap_size);

	void* allocate_page();
	void free_page(void* pointer);

	std::uint64_t get_free_page_count();

	struct heap_entry_t
	{
		heap_entry_t* next = nullptr;
	};
}
