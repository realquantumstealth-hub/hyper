#include "logs.h"

#include "../crt/crt.h"
#include "../memory_manager/heap_manager.h"
#include "../memory_manager/memory_manager.h"

namespace logs
{
	crt::mutex_t log_mutex = { };
}

void logs::set_up()
{
	constexpr std::uint64_t stored_logs_pages = 64; // Increased from 4 to 64 pages for more logs
	constexpr std::uint64_t stored_logs_size = stored_logs_pages * 0x1000;

	// will be done in initialization, so will be contiguous
	stored_logs = static_cast<trap_frame_log_t*>(heap_manager::allocate_page());

	// reserve those other pages (will be contiguous)
	for (std::uint64_t i = 0; i < stored_logs_pages - 1; i++)
	{
		heap_manager::allocate_page();
	}

	stored_log_max = stored_logs_size / sizeof(trap_frame_log_t);
}

void logs::add_log(trap_frame_log_t trap_frame)
{
	log_mutex.lock();

	std::uint16_t index = stored_log_index;

	if (index < stored_log_max)
	{
		stored_logs[index] = trap_frame;

		stored_log_index++;
	}

	log_mutex.release();
}

std::uint8_t logs::flush(cr3 slat_cr3, std::uint64_t guest_virtual_buffer, cr3 guest_cr3, std::uint16_t count)
{
	log_mutex.lock();

	std::uint16_t actual_count = crt::min(count, stored_log_index);

	std::uint16_t copy_start_index = stored_log_index - actual_count;
	std::uint64_t write_size = sizeof(trap_frame_log_t) * actual_count;

	std::uint64_t bytes_written = memory_manager::operate_on_guest_virtual_memory(slat_cr3, &stored_logs[copy_start_index], guest_virtual_buffer, guest_cr3, write_size, memory_operation_t::write_operation);

	stored_log_index = copy_start_index;

	log_mutex.release();

	return bytes_written == write_size;
}
