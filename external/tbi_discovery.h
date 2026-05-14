#pragma once
#include <cstdint>
#include <C:\Users\Home\Desktop\hyper\shared\structures/tbi_info.h>
#include <ia32-doc/ia32.hpp>

namespace tbi_discovery
{

	// Initialize the TBS discovery system
	void initialize();

	// Try to discover tbs.dll in the system - called on each vmexit
	bool try_discover_tbi();

	// Get the current tbs.dll info
	tbi_info_t get_tbi_info();

	// Get tbs.dll base address (0 if not found)
	std::uint64_t get_tbi_base();

	// Check if tbs.dll has been found
	bool is_tbi_found();
}
