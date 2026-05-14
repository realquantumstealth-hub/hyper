#pragma once
#include <cstdint>

// CR3 caching module to bypass EAC's CR3 shuffling
// Opportunistically captures CR3 during regular VM exits
namespace cr3_cache
{
    // Initialize CR3 caching system
    void initialize();

    // Call this on EVERY vmexit - checks if ring3 + target PID, caches CR3 if match
    void try_cache_on_vmexit();

    // Set which process to monitor (call from usermode hypercall)
    void set_target_pid(std::uint64_t pid);

    // Get current process ID from GS base (ring 3 only)
    std::uint64_t get_current_process_id();

    // Get cached CR3 for target process (for usermode hypercalls)
    std::uint64_t get_cached_cr3();

    // Statistics
    struct cr3_stats_t {
        std::uint64_t total_samples;
        std::uint64_t ring3_samples;
        std::uint64_t target_pid_hits;
        std::uint64_t cr3_updates;
        std::uint64_t last_cached_cr3;
    };

    cr3_stats_t get_statistics();
}
