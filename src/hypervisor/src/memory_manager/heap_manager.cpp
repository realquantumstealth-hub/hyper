#include "heap_manager.h"
#include "../crt/crt.h"
#include <intrin.h>

namespace
{
	constexpr std::uint64_t heap_block_size = 0x1000;

	heap_manager::heap_entry_t* free_block_list_head = nullptr;

	crt::mutex_t allocation_mutex = { };
}

void heap_manager::set_up(std::uint64_t heap_base, std::uint64_t heap_size)
{
	free_block_list_head = reinterpret_cast<heap_entry_t*>(heap_base);

	std::uint64_t heap_entries = heap_size / heap_block_size;

	heap_entry_t* entry = free_block_list_head;

	for (std::uint64_t i = 1; i < heap_entries - 1; i++)
	{
		entry->next = reinterpret_cast<heap_entry_t*>(reinterpret_cast<std::uint8_t*>(entry) + heap_block_size);

		entry = entry->next;
	}

	entry->next = nullptr;
}

void* heap_manager::allocate_page()
{
	allocation_mutex.lock();

	heap_entry_t* entry = free_block_list_head;

	if (entry == nullptr)
	{
		allocation_mutex.release();

		return nullptr;
	}

	free_block_list_head = entry->next;

	allocation_mutex.release();

	return entry;
}

void heap_manager::free_page(void* allocation_base)
{
	if (allocation_base == nullptr)
	{
		return;
	}

	allocation_mutex.lock();

	heap_entry_t* entry = static_cast<heap_entry_t*>(allocation_base);

	entry->next = free_block_list_head;
	free_block_list_head = entry;

	allocation_mutex.release();
}

std::uint64_t heap_manager::get_free_page_count()
{
	allocation_mutex.lock();

	std::uint64_t count = 0;

	heap_entry_t* entry = free_block_list_head;

	while (entry != nullptr)
	{
		count++;

		entry = entry->next;
	}

	allocation_mutex.release();

	return count;
}
