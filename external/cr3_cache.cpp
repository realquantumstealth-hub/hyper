#include "cr3_cache.h"
#include "../arch/arch.h"
#include "../logs/logs.h"
#include "../crt/crt.h"
#include "../memory_manager/memory_manager.h"
#include "../slat/slat.h"
#include <ia32-doc/ia32.hpp>
#include <intrin.h>

namespace cr3_cache
{
    static std::uint64_t g_target_pid = 0;
    static std::uint64_t g_cached_cr3 = 0;
    static cr3_stats_t g_stats = {};

    void initialize()
    {
        g_target_pid = 0;
        g_cached_cr3 = 0;
        crt::set_memory(&g_stats, 0, sizeof(g_stats));
    }

    void set_target_pid(std::uint64_t pid)
    {
        g_target_pid = pid;
        g_cached_cr3 = 0; // Reset cached CR3 when changing target

        trap_frame_log_t log = {};
        log.r15 = 0xC300; // Target PID set
        log.r14 = pid;
        logs::add_log(log);
    }

    std::uint64_t get_current_process_id()
    {
        // Read GS base (TEB pointer in ring 3)
        std::uint64_t gs_base = arch::get_guest_gs_base();

        if (gs_base == 0)
            return 0;

        // The 0x40 aka TEB.ClientId.UniqueProcess
        std::uint64_t pid = 0;

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        std::uint64_t bytes_read = memory_manager::operate_on_guest_virtual_memory(
            slat_cr3, &pid, gs_base + 0x40, guest_cr3, sizeof(pid),
            memory_operation_t::read_operation);

        if (bytes_read != sizeof(pid))
            return 0;

        return pid;
    }

    void try_cache_on_vmexit()
    {
        g_stats.total_samples++;

     

        // Check if we're in ring 3 (usermode)
        std::uint64_t cs_selector = arch::get_guest_cs_selector();
        std::uint8_t cpl = cs_selector & 0x3;

        if (cpl != 3)
            return; // Not in usermode

        g_stats.ring3_samples++;

    

        // No target PID set, nothing to cache
        if (g_target_pid == 0)
            return;

        // Get current process ID
        std::uint64_t current_pid = get_current_process_id();

    

        if (current_pid != g_target_pid)
            return; // Not our target process

        g_stats.target_pid_hits++;

      

        // Read current CR3 from VMCS/VMCB
        cr3 guest_cr3 = arch::get_guest_cr3();

        // Only update if CR3 changed
        if (guest_cr3.flags != g_cached_cr3)
        {
            g_cached_cr3 = guest_cr3.flags;
            g_stats.cr3_updates++;
            g_stats.last_cached_cr3 = guest_cr3.flags;

            // Log CR3 cache update
            trap_frame_log_t log = {};
            log.r15 = 0xC305; // CR3 cached marker
            log.r14 = g_cached_cr3;
            log.r13 = g_stats.cr3_updates;
            logs::add_log(log);
        }
    }

    std::uint64_t get_cached_cr3()
    {
        return g_cached_cr3;
    }

    cr3_stats_t get_statistics()
    {
        return g_stats;
    }
}
