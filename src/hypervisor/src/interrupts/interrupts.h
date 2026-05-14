#pragma once
#include "../apic/apic.h"
#include <cstdint>

namespace interrupts
{
	void set_up();

	void set_all_nmi_ready();
	void set_nmi_ready(uint64_t apic_id);
	void clear_nmi_ready(uint64_t apic_id);

	uint8_t is_nmi_ready(uint64_t apic_id);

	void process_nmi();
	void send_nmi_all_but_self();

	// Injects an exception into the guest on the next VM-entry.
	void inject_guest_exception(std::uint8_t vector, std::uint32_t error_code, bool has_error_code = false);

	inline apic_t* apic = nullptr;
}