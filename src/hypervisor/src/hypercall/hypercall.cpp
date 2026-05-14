#include "hypercall.h"

#include <hypercall/hypercall_def.h>

#include "../memory_manager/memory_manager.h"

#include "../memory_manager/memory_management.h"

#include "../memory_manager/ntoskrnl_parser.h"

#include "../slat/slat.h"

#include "../arch/arch.h"

#include "../logs/logs.h"

#include "../crt/crt.h"

#include "../memory_manager/heap_manager.h"

#include "../apic/apic.h"

#include "../cr3_cache/cr3_cache.h"

#include <ia32-doc/ia32.hpp>

#include <intrin.h>



// Access to global variables defined in main.cpp

extern std::uint64_t g_ntoskrnl_base_address;

#include "../core/definitions.h"



// Function to get ntoskrnl base using KPCR IDT inspection

namespace utils

{

namespace memory

{

    std::uint64_t get_kernel_base()

    {

        trap_frame_log_t log_entry = {};

        // 1. Read IDT base directly from IDTR register
        const std::uint64_t idt_base = arch::get_guest_idtr_base();

        log_entry.r15 = 0xDDD2;
        log_entry.r14 = idt_base;
        logs::add_log(log_entry);

        constexpr std::uint64_t minimum_kernel_address = 0xFFFF000000000000ULL;

        if (idt_base == 0 || idt_base < minimum_kernel_address)
        {
            log_entry.r15 = 0xDDD3;
            logs::add_log(log_entry);
            return 0;
        }

        const cr3 guest_cr3 = arch::get_guest_cr3();
        const cr3 slat_cr3 = slat::get_cr3();

        if (guest_cr3.flags == 0 || slat_cr3.flags == 0)
        {
            log_entry.r15 = 0xDDD8;
            logs::add_log(log_entry);
            return 0;
        }

        // 2. Get the Page Fault handler (int 0x0E) from the IDT
        constexpr std::uint64_t PAGE_FAULT_IDT_INDEX = 0x0E;
        constexpr std::uint64_t IDT_ENTRY_SIZE = 16;
        const std::uint64_t idt_entry_addr = idt_base + (PAGE_FAULT_IDT_INDEX * IDT_ENTRY_SIZE);

        // Read the IDT entry (64-bit _KIDTENTRY64 structure)
        std::uint16_t offset_low = 0;
        std::uint16_t offset_middle = 0;
        std::uint32_t offset_high = 0;

        if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &offset_low, idt_entry_addr, guest_cr3, sizeof(offset_low), memory_operation_t::read_operation) != sizeof(offset_low))
            return 0;

        if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &offset_middle, idt_entry_addr + 6, guest_cr3, sizeof(offset_middle), memory_operation_t::read_operation) != sizeof(offset_middle))
            return 0;

        if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &offset_high, idt_entry_addr + 8, guest_cr3, sizeof(offset_high), memory_operation_t::read_operation) != sizeof(offset_high))
            return 0;

        // 3. Rebuild the full 64-bit handler address
        const std::uint64_t handler_address = (static_cast<std::uint64_t>(offset_high) << 32) |
                                               (static_cast<std::uint64_t>(offset_middle) << 16) |
                                               static_cast<std::uint64_t>(offset_low);

        log_entry.r15 = 0xDDDC;
        log_entry.r14 = handler_address;
        logs::add_log(log_entry);

        if (handler_address == 0 || handler_address < minimum_kernel_address)
        {
            log_entry.r15 = 0xDDDD;
            logs::add_log(log_entry);
            return 0;
        }

        // 4. Align to 2MB boundary (ntoskrnl is loaded at 2MB boundaries)
        constexpr std::uint64_t NTOSKRNL_ALIGNMENT = 0x200000; // 2MB
        std::uint64_t current_addr = handler_address & ~(NTOSKRNL_ALIGNMENT - 1);

        constexpr std::uint16_t MZ_SIGNATURE = 0x5A4D;
        constexpr std::uint32_t PE_SIGNATURE = 0x00004550;
        constexpr std::uint64_t max_search_iterations = 0x10; // Search up to 32MB back
        std::uint64_t iterations = 0;

        log_entry.r15 = 0xDDE0;
        log_entry.r14 = current_addr;
        logs::add_log(log_entry);

        while (current_addr >= minimum_kernel_address && iterations < max_search_iterations)
        {
            // 4.1. Check for MZ signature at 2MB boundary
            std::uint16_t signature = 0;
            if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &signature, current_addr, guest_cr3, sizeof(signature), memory_operation_t::read_operation) == sizeof(signature))
            {
                if (signature == MZ_SIGNATURE)
                {
                    // 5. Validate PE header
                    std::uint32_t e_lfanew = 0;
                    if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &e_lfanew, current_addr + 0x3C, guest_cr3, sizeof(e_lfanew), memory_operation_t::read_operation) == sizeof(e_lfanew))
                    {
                        const std::uint64_t nt_header_address = current_addr + e_lfanew;
                        std::uint32_t nt_signature = 0;

                        if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &nt_signature, nt_header_address, guest_cr3, sizeof(nt_signature), memory_operation_t::read_operation) == sizeof(nt_signature))
                        {
                            if (nt_signature == PE_SIGNATURE)
                            {
                                // Valid PE found at 2MB boundary!
                                log_entry.r15 = 0xDDD5;
                                log_entry.r14 = current_addr;
                                logs::add_log(log_entry);
                                return current_addr;
                            }
                        }
                    }
                }
            }

            // Move to previous 2MB boundary
            current_addr -= NTOSKRNL_ALIGNMENT;
            iterations++;
        }

        log_entry.r15 = 0xDDE1;
        log_entry.r14 = iterations;
        logs::add_log(log_entry);

        return 0;

    }



}

}



std::uint64_t get_ntoskrnl_from_kpcr()

{

    trap_frame_log_t log_entry = {};

    log_entry.r15 = 0xDDD1;

    logs::add_log(log_entry);



    const std::uint64_t kernel_base = utils::memory::get_kernel_base();

    if (kernel_base != 0)

    {

        trap_frame_log_t success_log = {};

        success_log.r15 = 0xDDD5;

        success_log.r14 = kernel_base;

        logs::add_log(success_log);



        return kernel_base;

    }



    trap_frame_log_t failure_log = {};

    failure_log.r15 = 0xDDD6;

    failure_log.r14 = 0;

    logs::add_log(failure_log);



    return 0;

}



// Ensure the structures are properly recognized

using HYPERVISOR_PFN_INFO = ::HYPERVISOR_PFN_INFO;

using HYPERVISOR_MEMORY_INFO = ::HYPERVISOR_MEMORY_INFO;



// Offsets for Windows EPROCESS structure (x64) - verify against your target OS version

namespace eprocess_offsets

{

    constexpr std::uint64_t UniqueProcessId = 0x440;

    constexpr std::uint64_t ActiveProcessLinks = 0x448;

    constexpr std::uint64_t SectionBaseAddress = 0x520;

    constexpr std::uint64_t DirectoryTableBase = 0x28;

}



// Global list to track hidden memory allocations

hidden_memory_entry_t* g_hidden_memory_list = nullptr;

crt::mutex_t g_hidden_memory_mutex = {};



// A tracker for a single page allocation within the allocate_hidden_memory hypercall.

struct allocation_tracker_t

{

    std::uint64_t gva;

    std::uint64_t gpa;

    void* hva;

    std::uint64_t hpa;

    allocation_tracker_t* next;

};



// Global GPA allocator (simple bump allocator)

class gpa_allocator {

private:

    std::uint64_t next_gpa;

    crt::mutex_t allocation_mutex;



    static std::uint64_t align_up(std::uint64_t value, std::uint64_t alignment) {

        return (value + alignment - 1) & ~(alignment - 1);

    }



public:

    gpa_allocator() : next_gpa(0x200000000ULL) {}



    std::uint64_t allocate_range(std::uint64_t size) {

        allocation_mutex.lock();

        std::uint64_t allocated = next_gpa;

        next_gpa += align_up(size, PAGE_SIZE_4KB);

        allocation_mutex.release();

        return allocated;

    }



    std::uint64_t allocate_page() {

        return allocate_range(PAGE_SIZE_4KB);

    }



    void free_range(std::uint64_t gpa, std::uint64_t size) {

        // No-op for simple bump allocator

    }

};

static gpa_allocator g_gpa_allocator;





std::uint64_t operate_on_guest_physical_memory(trap_frame_t* trap_frame, memory_operation_t operation)

{

    cr3 guest_cr3 = arch::get_guest_cr3();

    cr3 slat_cr3 = slat::get_cr3();



    std::uint64_t guest_buffer_virtual_address = trap_frame->r8;

    std::uint64_t guest_physical_address = trap_frame->rdx;

    std::uint64_t size_left_to_copy = trap_frame->r9;

    std::uint64_t bytes_copied = 0;



    while (size_left_to_copy > 0)

    {

        std::uint64_t guest_buffer_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = guest_buffer_virtual_address + bytes_copied });

        if (guest_buffer_physical_address == 0) break;



        std::uint64_t size_left_on_dest_slat_page = 0;

        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_buffer_physical_address, &size_left_on_dest_slat_page);

        if (host_destination == 0) break;



        std::uint64_t size_left_on_src_slat_page = 0;

        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_physical_address + bytes_copied, &size_left_on_src_slat_page);

        if (host_source == 0) break;



        void* source_ptr = reinterpret_cast<void*>(host_source);

        void* dest_ptr = reinterpret_cast<void*>(host_destination);



        if (operation == memory_operation_t::write_operation) {

            crt::swap(source_ptr, dest_ptr);

        }



        std::uint64_t copy_size = crt::min(size_left_to_copy, crt::min(size_left_on_dest_slat_page, size_left_on_src_slat_page));

        if (copy_size == 0) break;



        crt::copy_memory(dest_ptr, source_ptr, copy_size);



        size_left_to_copy -= copy_size;

        bytes_copied += copy_size;

    }



    return bytes_copied;

}

std::uint64_t operate_on_guest_virtual_memory(trap_frame_t* trap_frame, memory_operation_t operation, std::uint64_t address_of_page_directory)

{

    cr3 guest_source_cr3 = { .address_of_page_directory = address_of_page_directory };

    cr3 guest_destination_cr3 = arch::get_guest_cr3();

    cr3 slat_cr3 = slat::get_cr3();



    std::uint64_t guest_destination_virtual_address = trap_frame->rdx;

    std::uint64_t guest_source_virtual_address = trap_frame->r8;

    std::uint64_t size_left_to_read = trap_frame->r9;

    std::uint64_t bytes_copied = 0;



    while (size_left_to_read != 0)

    {

        std::uint64_t size_left_of_destination_virtual_page = UINT64_MAX;

        std::uint64_t size_left_of_destination_slat_page = UINT64_MAX;

        std::uint64_t size_left_of_source_virtual_page = UINT64_MAX;

        std::uint64_t size_left_of_source_slat_page = UINT64_MAX;



        std::uint64_t guest_source_physical_address = memory_manager::translate_guest_virtual_address(guest_source_cr3, slat_cr3, { .address = guest_source_virtual_address + bytes_copied }, &size_left_of_source_virtual_page);

        std::uint64_t guest_destination_physical_address = memory_manager::translate_guest_virtual_address(guest_destination_cr3, slat_cr3, { .address = guest_destination_virtual_address + bytes_copied }, &size_left_of_destination_virtual_page);



        if (size_left_of_destination_virtual_page == UINT64_MAX || size_left_of_source_virtual_page == UINT64_MAX) break;



        std::uint64_t host_destination = memory_manager::map_guest_physical(slat_cr3, guest_destination_physical_address, &size_left_of_destination_slat_page);

        std::uint64_t host_source = memory_manager::map_guest_physical(slat_cr3, guest_source_physical_address, &size_left_of_source_slat_page);



        if (size_left_of_destination_slat_page == UINT64_MAX || size_left_of_source_slat_page == UINT64_MAX) break;



        if (operation == memory_operation_t::write_operation)

        {

            crt::swap(host_source, host_destination);

        }



        std::uint64_t size_left_of_slat_pages = crt::min(size_left_of_source_slat_page, size_left_of_destination_slat_page);

        std::uint64_t size_left_of_virtual_pages = crt::min(size_left_of_source_virtual_page, size_left_of_destination_virtual_page);

        std::uint64_t size_left_of_pages = crt::min(size_left_of_slat_pages, size_left_of_virtual_pages);

        std::uint64_t copy_size = crt::min(size_left_to_read, size_left_of_pages);



        if (copy_size == 0) break;



        crt::copy_memory(reinterpret_cast<void*>(host_destination), reinterpret_cast<const void*>(host_source), copy_size);



        size_left_to_read -= copy_size;

        bytes_copied += copy_size;

    }



    return bytes_copied;

}





struct allocate_hidden_memory_params_t

{

    std::uint64_t virtual_address;

    std::uint64_t size;

    std::uint64_t target_pid;

    std::uint64_t ps_initial_system_process;

};



// Helper function for get_process_cr3_by_pid that takes parameters directly

std::uint64_t get_process_cr3_by_pid_helper(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, cr3 kernel_cr3)

{

    cr3 slat_cr3 = slat::get_cr3();



    if (ps_initial_system_process == 0 || kernel_cr3.address_of_page_directory == 0) {

        return 0;

    }



    auto read_guest_virtual = [&](std::uint64_t gva, void* buffer, std::uint64_t size) -> bool {

        return memory_manager::operate_on_guest_virtual_memory(slat_cr3, buffer, gva, kernel_cr3, size, memory_operation_t::read_operation) == size;

        };



    std::uint64_t system_eprocess = 0;

    if (!read_guest_virtual(ps_initial_system_process, &system_eprocess, sizeof(system_eprocess))) {

        return 0;

    }



    std::uint64_t current = system_eprocess;

    int safety_counter = 0;

    do {

        std::uint64_t pid = 0;

        if (!read_guest_virtual(current + eprocess_offsets::UniqueProcessId, &pid, sizeof(pid))) return 0;



        if (pid == target_pid) {

            std::uint64_t cr3_value = 0;

            if (read_guest_virtual(current + eprocess_offsets::DirectoryTableBase, &cr3_value, sizeof(cr3_value))) {

                return cr3_value;

            }

            return 0;

        }



        std::uint64_t next_link = 0;

        if (!read_guest_virtual(current + eprocess_offsets::ActiveProcessLinks, &next_link, sizeof(next_link))) return 0;



        current = next_link - eprocess_offsets::ActiveProcessLinks;

        safety_counter++;



        if (current == system_eprocess) break;



    } while (safety_counter < 2000);



    return 0;

}



std::uint64_t get_process_base_by_pid_from_initial(std::uint64_t target_pid, std::uint64_t ps_initial_system_process, cr3 kernel_cr3) {

    cr3 slat_cr3 = slat::get_cr3();



    if (ps_initial_system_process == 0 || kernel_cr3.address_of_page_directory == 0) {

        return 0;

    }



    auto read_guest_virtual = [&](std::uint64_t gva, void* buffer, std::uint64_t size) -> bool {

        return memory_manager::operate_on_guest_virtual_memory(slat_cr3, buffer, gva, kernel_cr3, size, memory_operation_t::read_operation) == size;

        };



    std::uint64_t system_eprocess = 0;

    if (!read_guest_virtual(ps_initial_system_process, &system_eprocess, sizeof(system_eprocess))) {

        return 0;

    }



    std::uint64_t current = system_eprocess;

    int safety_counter = 0;

    do {

        std::uint64_t pid = 0;

        if (!read_guest_virtual(current + eprocess_offsets::UniqueProcessId, &pid, sizeof(pid))) return 0;



        if (pid == target_pid) {

            std::uint64_t section_base_address = 0;

            if (read_guest_virtual(current + eprocess_offsets::SectionBaseAddress, &section_base_address, sizeof(section_base_address))) {

                return section_base_address;

            }

            return 0;

        }



        std::uint64_t next_link = 0;

        if (!read_guest_virtual(current + eprocess_offsets::ActiveProcessLinks, &next_link, sizeof(next_link))) return 0;



        current = next_link - eprocess_offsets::ActiveProcessLinks;

        safety_counter++;



        if (current == system_eprocess) break;



    } while (safety_counter < 2000);



    return 0;

}



std::uint64_t get_process_cr3_by_pid(trap_frame_t* trap_frame)

{

    const std::uint64_t target_pid = trap_frame->rdx;

    const std::uint64_t ps_initial_system_process = trap_frame->r8;

    const cr3 kernel_cr3 = { .flags = trap_frame->r9 };



    return get_process_cr3_by_pid_helper(target_pid, ps_initial_system_process, kernel_cr3);

}



std::uint64_t get_eprocess_base_by_pid(trap_frame_t* trap_frame)

{

    cr3 slat_cr3 = slat::get_cr3();

    const std::uint64_t target_pid = trap_frame->rdx;

    const std::uint64_t ps_initial_system_process = trap_frame->r8;

    const cr3 kernel_cr3 = { .flags = trap_frame->r9 };



    auto read_guest_virtual = [&](std::uint64_t gva, void* buffer, std::uint64_t size) -> bool {

        return memory_manager::operate_on_guest_virtual_memory(slat_cr3, buffer, gva, kernel_cr3, size, memory_operation_t::read_operation) == size;

        };

    std::uint64_t system_eprocess = 0;

    if (!read_guest_virtual(ps_initial_system_process, &system_eprocess, sizeof(system_eprocess))) {

        return 0;

    }



    std::uint64_t current = system_eprocess;

    do {

        std::uint64_t pid = 0;

        if (!read_guest_virtual(current + eprocess_offsets::UniqueProcessId, &pid, sizeof(pid))) return 0;



        if (pid == target_pid) {

            return current;

        }



        std::uint64_t next_link = 0;

        if (!read_guest_virtual(current + eprocess_offsets::ActiveProcessLinks, &next_link, sizeof(next_link))) return 0;



        current = next_link - eprocess_offsets::ActiveProcessLinks;

        if (current == system_eprocess) break;

    } while (true);



    return 0;

}



bool unmap_gva_from_guest(cr3 target_cr3, cr3 slat_cr3, std::uint64_t gva)

{

    virtual_address_t va = { .address = gva };

    pml4e_64* pml4 = reinterpret_cast<pml4e_64*>(memory_manager::map_guest_physical(slat_cr3, target_cr3.address_of_page_directory << 12));

    if (!pml4 || !pml4[va.pml4_idx].present) return true;



    pdpte_64* pdpt = reinterpret_cast<pdpte_64*>(memory_manager::map_guest_physical(slat_cr3, pml4[va.pml4_idx].page_frame_number << 12));

    if (!pdpt || !pdpt[va.pdpt_idx].present) return true;



    if (pdpt[va.pdpt_idx].large_page) return false;



    pde_64* pd = reinterpret_cast<pde_64*>(memory_manager::map_guest_physical(slat_cr3, pdpt[va.pdpt_idx].page_frame_number << 12));

    if (!pd || !pd[va.pd_idx].present) return true;



    if (pd[va.pd_idx].large_page) return false;



    pte_64* pt = reinterpret_cast<pte_64*>(memory_manager::map_guest_physical(slat_cr3, pd[va.pd_idx].page_frame_number << 12));

    if (!pt) return false;



    pte_64* pte = &pt[va.pt_idx];

    pte->flags = 0;



    arch::invlpg(gva);



    return true;

}



bool map_gva_to_gpa_in_guest(cr3 target_cr3, cr3 slat_cr3, std::uint64_t gva, std::uint64_t gpa_to_map)

{

    virtual_address_t va = { .address = gva };



    pml4e_64* pml4 = reinterpret_cast<pml4e_64*>(

        memory_manager::map_guest_physical(slat_cr3, target_cr3.address_of_page_directory << 12));

    if (!pml4) return false;



    pml4e_64* pml4e = &pml4[va.pml4_idx];

    pdpte_64* pdpt;



    if (!pml4e->present) {

        void* new_pdpt_hva = heap_manager::allocate_page();

        if (!new_pdpt_hva) return false;

        crt::set_memory(new_pdpt_hva, 0, PAGE_SIZE_4KB);



        // FIX: Get HPA, not GPA, then map GPA to this HPA

        std::uint64_t new_pdpt_hpa = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pdpt_hva));

        std::uint64_t new_pdpt_gpa = g_gpa_allocator.allocate_page();



        if (!slat::map_guest_physical_to_host_physical(new_pdpt_gpa, new_pdpt_hpa, true, true, false)) {

            heap_manager::free_page(new_pdpt_hva);

            return false;

        }



        pml4e->present = 1;

        pml4e->write = 1;

        pml4e->supervisor = 0;

        pml4e->page_frame_number = new_pdpt_gpa >> 12;



        pdpt = reinterpret_cast<pdpte_64*>(new_pdpt_hva);

    }

    else {

        pdpt = reinterpret_cast<pdpte_64*>(memory_manager::map_guest_physical(slat_cr3, pml4e->page_frame_number << 12));

    }



    if (!pdpt) return false;



    pdpte_64* pdpte = &pdpt[va.pdpt_idx];

    pde_64* pd;



    if (!pdpte->present) {

        void* new_pd_hva = heap_manager::allocate_page();

        if (!new_pd_hva) return false;

        crt::set_memory(new_pd_hva, 0, PAGE_SIZE_4KB);



     

        std::uint64_t new_pd_hpa = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pd_hva));

        std::uint64_t new_pd_gpa = g_gpa_allocator.allocate_page();



        if (!slat::map_guest_physical_to_host_physical(new_pd_gpa, new_pd_hpa, true, true, false)) {

            heap_manager::free_page(new_pd_hva);

            return false;

        }



        pdpte->present = 1;

        pdpte->write = 1;

        pdpte->supervisor = 0;



        pdpte->page_frame_number = new_pd_gpa >> 12;



        pd = reinterpret_cast<pde_64*>(new_pd_hva);

    }

    else {

        pd = reinterpret_cast<pde_64*>(memory_manager::map_guest_physical(slat_cr3, pdpte->page_frame_number << 12));

    }



    if (!pd) return false;



    pde_64* pde = &pd[va.pd_idx];

    pte_64* pt;



    if (!pde->present) {

        void* new_pt_hva = heap_manager::allocate_page();

        if (!new_pt_hva) return false;

        crt::set_memory(new_pt_hva, 0, PAGE_SIZE_4KB);



        // FIX: Get HPA, then map GPA to this HPA

        std::uint64_t new_pt_hpa = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pt_hva));

        std::uint64_t new_pt_gpa = g_gpa_allocator.allocate_page();



        if (!slat::map_guest_physical_to_host_physical(new_pt_gpa, new_pt_hpa, true, true, false)) {

            heap_manager::free_page(new_pt_hva);

            return false;

        }



        pde->present = 1;

        pde->write = 1;

        pde->supervisor = 0;

        // FIX: Use GPA for page frame number in guest page tables

        pde->page_frame_number = new_pt_gpa >> 12;



        pt = reinterpret_cast<pte_64*>(new_pt_hva);

    }

    else {

        pt = reinterpret_cast<pte_64*>(memory_manager::map_guest_physical(slat_cr3, pde->page_frame_number << 12));

    }



    if (!pt) return false;



    pte_64* pte = &pt[va.pt_idx];

    pte->present = 1;

    pte->write = 1;

    pte->supervisor = 0;

    pte->execute_disable = 0;

    // FIX: Use GPA for page frame number in guest page tables

    pte->page_frame_number = gpa_to_map >> 12;



    return true;

}



std::uint64_t allocate_hidden_memory(trap_frame_t* trap_frame, hypercall_info_t hypercall_info)

{

    trap_frame_log_t log_entry = {};



    // 1. Input validation

    if (!trap_frame) return 0;



    cr3 kernel_cr3 = { .flags = hypercall_info.call_reserved_data };

    std::uint64_t params_gva = trap_frame->rdx;



    log_entry.r15 = 1;

    log_entry.r14 = params_gva;

    log_entry.r13 = kernel_cr3.flags;

    logs::add_log(log_entry);



    if (kernel_cr3.flags == 0 || params_gva == 0) return 0;



    allocate_hidden_memory_params_t params = {};

    cr3 guest_cr3 = arch::get_guest_cr3();

    cr3 slat_cr3 = slat::get_cr3();



    if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &params, params_gva, guest_cr3, sizeof(params), memory_operation_t::read_operation) != sizeof(params)) return 0;



    if (params.size == 0 || params.size > 0x10000000 || (params.virtual_address & (PAGE_SIZE_4KB - 1)) != 0) return 0;



    // 2. Get target process CR3

    cr3 target_cr3 = { .flags = get_process_cr3_by_pid_helper(params.target_pid, params.ps_initial_system_process, kernel_cr3) };



    log_entry.r15 = 2;

    log_entry.r14 = target_cr3.flags;

    logs::add_log(log_entry);



    if (target_cr3.flags == 0) return 0;



    std::uint64_t num_pages = (params.size + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;

    std::uint64_t final_virtual_address = params.virtual_address;



    // 3. Pre-flight Check

    for (std::uint64_t i = 0; i < num_pages; ++i) {

        std::uint64_t check_gva = final_virtual_address + (i * PAGE_SIZE_4KB);

        if (memory_manager::translate_guest_virtual_address(target_cr3, slat_cr3, { .address = check_gva }) != 0) {

            log_entry.r15 = 3;

            log_entry.r14 = check_gva;

            logs::add_log(log_entry);

            return 0;

        }

    }

    allocation_tracker_t* allocation_head = nullptr;

    bool success = true;

    std::uint64_t allocation_start_gpa = g_gpa_allocator.allocate_range(num_pages * PAGE_SIZE_4KB);



    if (allocation_start_gpa == 0) return 0;



    for (std::uint64_t i = 0; i < num_pages; ++i) {

        std::uint64_t current_gva = final_virtual_address + (i * PAGE_SIZE_4KB);

        std::uint64_t current_gpa = allocation_start_gpa + (i * PAGE_SIZE_4KB);



        // Per-page allocation logging removed to prevent spam



        void* hidden_page_hva = heap_manager::allocate_page();

        if (!hidden_page_hva) {

            log_entry.r15 = 0x30;

            log_entry.r14 = i;

            logs::add_log(log_entry);

            success = false;

            break;

        }



        crt::set_memory(hidden_page_hva, 0, PAGE_SIZE_4KB);

        std::uint64_t hidden_hpa = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(hidden_page_hva));



        if (hidden_hpa == 0) {

            heap_manager::free_page(hidden_page_hva);

            log_entry.r15 = 0x31;

            log_entry.r14 = i;

            logs::add_log(log_entry);

            success = false;

            break;

        }



        allocation_tracker_t* tracker = reinterpret_cast<allocation_tracker_t*>(heap_manager::allocate_page());

        if (!tracker) {

            heap_manager::free_page(hidden_page_hva);

            log_entry.r15 = 0x32;

            log_entry.r14 = i;

            logs::add_log(log_entry);

            success = false;

            break;

        }



        tracker->gva = current_gva;

        tracker->gpa = current_gpa;

        tracker->hva = hidden_page_hva;

        tracker->hpa = hidden_hpa;

        tracker->next = allocation_head;

        allocation_head = tracker;



        // Create SLAT mapping first

        if (!slat::map_guest_physical_to_host_physical(current_gpa, hidden_hpa, true, true, true)) {

            log_entry.r15 = 0x33;

            log_entry.r14 = i;

            logs::add_log(log_entry);

            success = false;

            break;

        }

    }





    if (success) {

        log_entry.r15 = 0x10;

        logs::add_log(log_entry);





        slat::flush_all_logical_processors_slat_cache();



        // Then do guest page table updates  

        for (allocation_tracker_t* current = allocation_head; current; current = current->next) {

            if (!map_gva_to_gpa_in_guest(target_cr3, slat_cr3, current->gva, current->gpa)) {

                success = false;

                break;

            }

        }



        if (success) {

            // Conservative single flush only to avoid issues

            arch::flush_target_process_tlb(target_cr3, final_virtual_address);



            g_hidden_memory_mutex.lock();

            for (allocation_tracker_t* current = allocation_head; current; current = current->next) {

                hidden_memory_entry_t* entry = reinterpret_cast<hidden_memory_entry_t*>(heap_manager::allocate_page());

                if (entry) {

                    entry->gpa_pfn = current->gpa >> 12;

                    entry->hidden_hpa_pfn = current->hpa >> 12;

                    entry->next = g_hidden_memory_list;

                    g_hidden_memory_list = entry;

                }

            }

            g_hidden_memory_mutex.release();

        }

    }



    if (!success) {

        log_entry.r15 = 0x11;

        logs::add_log(log_entry);



        // Rollback in reverse order of allocation

        while (allocation_head) {

            allocation_tracker_t* current = allocation_head;

            allocation_head = allocation_head->next;



            // Cleanup the allocations but don't double-free the tracker

            if (memory_manager::translate_guest_virtual_address(target_cr3, slat_cr3, { .address = current->gva }) == current->gpa) {

                unmap_gva_from_guest(target_cr3, slat_cr3, current->gva);

            }

            slat::unmap_guest_physical(current->gpa);

            heap_manager::free_page(current->hva);

            heap_manager::free_page(current);  // Free tracker only once

        }

    }

    return final_virtual_address;

}





struct free_hidden_memory_params_t

{

    std::uint64_t virtual_address;

    std::uint64_t size;

    std::uint64_t target_pid;

    std::uint64_t ps_initial_system_process;

};



std::uint64_t free_hidden_memory(trap_frame_t* trap_frame, hypercall_info_t hypercall_info)

{

    cr3 kernel_cr3 = { .flags = hypercall_info.call_reserved_data };

    free_hidden_memory_params_t params = {};

    cr3 guest_cr3 = arch::get_guest_cr3();

    cr3 slat_cr3 = slat::get_cr3();



    if (memory_manager::operate_on_guest_virtual_memory(slat_cr3, &params, trap_frame->rdx, guest_cr3, sizeof(params), memory_operation_t::read_operation) != sizeof(params)) return 0;



    cr3 target_cr3 = { .flags = get_process_cr3_by_pid_helper(params.target_pid, params.ps_initial_system_process, kernel_cr3) };

    if (target_cr3.flags == 0) return 0;



    std::uint64_t num_pages = (params.size + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;

    std::uint64_t freed_pages = 0;

    for (std::uint64_t i = 0; i < num_pages; ++i)

    {

        std::uint64_t current_gva = params.virtual_address + (i * PAGE_SIZE_4KB);

        std::uint64_t current_gpa = memory_manager::translate_guest_virtual_address(target_cr3, slat_cr3, { .address = current_gva });

        if (current_gpa == 0) continue;



        unmap_gva_from_guest(target_cr3, slat_cr3, current_gva);

        slat::unmap_guest_physical(current_gpa);



        g_hidden_memory_mutex.lock();

        hidden_memory_entry_t* prev = nullptr;

        for (hidden_memory_entry_t* current = g_hidden_memory_list; current; ) {

            if (current->gpa_pfn == (current_gpa >> 12)) {

                if (prev) prev->next = current->next;

                else g_hidden_memory_list = current->next;

                void* page_to_free = reinterpret_cast<void*>(memory_manager::map_host_physical(current->hidden_hpa_pfn << 12));

                heap_manager::free_page(page_to_free);

                heap_manager::free_page(current);

                freed_pages++;

                break;

            }

            prev = current;

            current = current->next;

        }

        g_hidden_memory_mutex.release();

    }



    if (freed_pages > 0) {

        slat::flush_all_logical_processors_slat_cache();

    }



    return freed_pages;

}



std::uint64_t call_dllmain_silently(trap_frame_t* trap_frame, hypercall_info_t hypercall_info)

{

    trap_frame_log_t log_entry = {};



    // 1. Extract parameters

    const std::uint64_t dllmain_address = trap_frame->rdx;

    const std::uint64_t hinstDLL = trap_frame->r8;

    const std::uint64_t fdwReason = trap_frame->r9;

    const std::uint64_t lpvReserved = hypercall_info.call_reserved_data;



    // Input validation

    if (dllmain_address == 0) {

        log_entry.r15 = 0xDE;

        log_entry.r14 = 0x1;

        logs::add_log(log_entry);

        return 0;

    }



    // 2. Get current guest state

    const std::uint64_t original_rip = arch::get_guest_rip();

    const std::uint64_t original_rsp = arch::get_guest_rsp();

    const cr3 guest_cr3 = arch::get_guest_cr3();

    const cr3 slat_cr3 = slat::get_cr3();



    // 3. Find safe address range for shellcode (avoid NTFS memory regions)

    std::uint64_t shellcode_gva = 0x7FFF00000000ULL; // Safe high user space



    // Validate address is not in use

    bool found_safe_address = false;

    for (int attempts = 0; attempts < 10; attempts++) {

        if (memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = shellcode_gva }) == 0) {

            found_safe_address = true;

            break;

        }

        shellcode_gva += 0x100000; // Try next 1MB boundary

    }



    if (!found_safe_address) {

        log_entry.r15 = 0xD2;

        log_entry.r14 = 0x3;

        logs::add_log(log_entry);

        return 0;

    }



    // 4. Allocate shellcode page with proper error handling

    void* shellcode_hva = heap_manager::allocate_page();

    if (!shellcode_hva) {

        return 0;

    }



    std::uint64_t shellcode_hpa = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(shellcode_hva));

    std::uint64_t shellcode_gpa = g_gpa_allocator.allocate_page();



    // 5. Create SLAT mapping with minimal permissions

    if (!slat::map_guest_physical_to_host_physical(shellcode_gpa, shellcode_hpa, true, true, true)) {

        heap_manager::free_page(shellcode_hva);

        return 0;

    }



    // 6. SAFE page table mapping with validation

    if (!map_gva_to_gpa_in_guest(guest_cr3, slat_cr3, shellcode_gva, shellcode_gpa)) {

        slat::unmap_guest_physical(shellcode_gpa);

        heap_manager::free_page(shellcode_hva);

        return 0;

    }



    // 7. Write FIXED shellcode that returns properly

    std::uint8_t* shellcode = reinterpret_cast<std::uint8_t*>(shellcode_hva);

    std::uint32_t offset = 0;



    // Save critical registers

    shellcode[offset++] = 0x50; // push rax

    shellcode[offset++] = 0x51; // push rcx

    shellcode[offset++] = 0x52; // push rdx

    shellcode[offset++] = 0x41; shellcode[offset++] = 0x50; // push r8

    shellcode[offset++] = 0x41; shellcode[offset++] = 0x51; // push r9

    shellcode[offset++] = 0x9C; // pushfq (save flags)



    // Align stack for Windows calling convention

    shellcode[offset++] = 0x48; shellcode[offset++] = 0x83;

    shellcode[offset++] = 0xEC; shellcode[offset++] = 0x28; // sub rsp, 0x28



    // Set up DllMain parameters

    // mov rcx, hinstDLL

    shellcode[offset++] = 0x48; shellcode[offset++] = 0xB9;

    *reinterpret_cast<std::uint64_t*>(&shellcode[offset]) = hinstDLL;

    offset += 8;



    // mov rdx, fdwReason

    shellcode[offset++] = 0x48; shellcode[offset++] = 0xBA;

    *reinterpret_cast<std::uint64_t*>(&shellcode[offset]) = fdwReason;

    offset += 8;



    // mov r8, lpvReserved

    shellcode[offset++] = 0x49; shellcode[offset++] = 0xB8;

    *reinterpret_cast<std::uint64_t*>(&shellcode[offset]) = lpvReserved;

    offset += 8;



    // mov rax, dllmain_address

    shellcode[offset++] = 0x48; shellcode[offset++] = 0xB8;

    *reinterpret_cast<std::uint64_t*>(&shellcode[offset]) = dllmain_address;

    offset += 8;



    // call rax

    shellcode[offset++] = 0xFF; shellcode[offset++] = 0xD0;



    // Restore stack

    shellcode[offset++] = 0x48; shellcode[offset++] = 0x83;

    shellcode[offset++] = 0xC4; shellcode[offset++] = 0x28; // add rsp, 0x28



    // Restore registers

    shellcode[offset++] = 0x9D; // popfq

    shellcode[offset++] = 0x41; shellcode[offset++] = 0x59; // pop r9

    shellcode[offset++] = 0x41; shellcode[offset++] = 0x58; // pop r8

    shellcode[offset++] = 0x5A; // pop rdx

    shellcode[offset++] = 0x59; // pop rcx

    shellcode[offset++] = 0x58; // pop rax



    // CRITICAL FIX: Jump back to original RIP instead of infinite loop

    shellcode[offset++] = 0x48; shellcode[offset++] = 0xB8; // mov rax, original_rip

    *reinterpret_cast<std::uint64_t*>(&shellcode[offset]) = original_rip;

    offset += 8;

    shellcode[offset++] = 0xFF; shellcode[offset++] = 0xE0; // jmp rax



    // 8. Conservative cache flushing (avoid system-wide flush)

    arch::flush_target_process_tlb(guest_cr3, shellcode_gva);



    // 9. Set guest execution state

    arch::set_guest_rip(shellcode_gva);



    log_entry.r15 = 0xDF;

    log_entry.r14 = shellcode_gva;

    logs::add_log(log_entry);



    return 1;

}



std::uint8_t copy_stack_data_from_log_exit(std::uint64_t* stack_data, std::uint64_t stack_data_count, cr3 guest_cr3, std::uint64_t rsp)

{

    if (rsp == 0) return 0;



    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t bytes_read = 0;

    std::uint64_t bytes_remaining = stack_data_count * sizeof(std::uint64_t);



    while (bytes_remaining != 0)

    {

        std::uint64_t virtual_size_left = 0;

        std::uint64_t rsp_guest_physical_address = memory_manager::translate_guest_virtual_address(guest_cr3, slat_cr3, { .address = rsp + bytes_read }, &virtual_size_left);

        if (rsp_guest_physical_address == 0) return 0;



        std::uint64_t physical_size_left = 0;

        std::uint64_t* rsp_mapped = reinterpret_cast<std::uint64_t*>(memory_manager::map_guest_physical(slat_cr3, rsp_guest_physical_address, &physical_size_left));

        if (!rsp_mapped) return 0;



        std::uint64_t size_left_of_page = crt::min(physical_size_left, virtual_size_left);

        std::uint64_t size_to_read = crt::min(bytes_remaining, size_left_of_page);

        if (size_to_read == 0) return 0;



        crt::copy_memory(reinterpret_cast<std::uint8_t*>(stack_data) + bytes_read, reinterpret_cast<std::uint8_t*>(rsp_mapped), size_to_read);



        bytes_remaining -= size_to_read;

        bytes_read += size_to_read;

    }

    return 1;

}



void do_stack_data_copy(trap_frame_log_t& trap_frame, cr3 guest_cr3)

{

    constexpr std::uint64_t stack_data_count = trap_frame_log_stack_data_count + 1;

    std::uint64_t stack_data[stack_data_count] = { };

    if (copy_stack_data_from_log_exit(&stack_data[0], stack_data_count, guest_cr3, trap_frame.rsp))

    {

        crt::copy_memory(&trap_frame.stack_data, &stack_data[1], sizeof(trap_frame.stack_data));

        trap_frame.rcx = stack_data[0];

        trap_frame.rsp += 8;

    }

}



void log_current_state(trap_frame_log_t& trap_frame)

{

    cr3 guest_cr3 = arch::get_guest_cr3();

    do_stack_data_copy(trap_frame, guest_cr3);

    trap_frame.cr3 = guest_cr3.flags;

    trap_frame.rip = arch::get_guest_rip();

    logs::add_log(trap_frame);

}



std::uint64_t flush_logs(trap_frame_t* trap_frame)

{

    std::uint64_t stored_logs_count = logs::stored_log_index;

    cr3 guest_cr3 = arch::get_guest_cr3();

    cr3 slat_cr3 = slat::get_cr3();

    std::uint64_t guest_virtual_address = trap_frame->rdx;

    auto count = static_cast<std::uint16_t>(trap_frame->r8);



    if (logs::flush(slat_cr3, guest_virtual_address, guest_cr3, count) == 0)

    {

        return -1;

    }

    return stored_logs_count;

}



void hypercall::process(hypercall_info_t hypercall_info, trap_frame_t* trap_frame, trap_frame_t* original_trap_frame)

{

    cr3 guest_cr3 = arch::get_guest_cr3();

    bool advance_rip = true; // Assume we advance RIP by default



    switch (hypercall_info.call_type)

    {

    case hypercall_type_t::guest_physical_memory_operation:

    {

        memory_operation_t memory_operation = static_cast<memory_operation_t>(hypercall_info.call_reserved_data);

        trap_frame->rax = operate_on_guest_physical_memory(trap_frame, memory_operation);

        break;

    }

    case hypercall_type_t::guest_virtual_memory_operation:

    {

        virt_memory_op_hypercall_info_t virt_memory_op_info = { .value = hypercall_info.value };

        memory_operation_t memory_operation = virt_memory_op_info.memory_operation;

        std::uint64_t address_of_page_directory = virt_memory_op_info.address_of_page_directory;

        trap_frame->rax = operate_on_guest_virtual_memory(trap_frame, memory_operation, address_of_page_directory);

        break;

    }

    case hypercall_type_t::translate_guest_virtual_address:

    {

        virtual_address_t guest_virtual_address = { .address = trap_frame->rdx };

        cr3 target_guest_cr3 = { .flags = trap_frame->r8 };

        cr3 slat_cr3 = slat::get_cr3();

        trap_frame->rax = memory_manager::translate_guest_virtual_address(target_guest_cr3, slat_cr3, guest_virtual_address);

        break;

    }

    case hypercall_type_t::read_guest_cr3:

    {

        cr3 guest_cr3_val = arch::get_guest_cr3();

        trap_frame->rax = guest_cr3_val.flags;

        break;

    }

    case hypercall_type_t::add_slat_code_hook:

    {

        virtual_address_t target_gpa = { .address = trap_frame->rdx };

        virtual_address_t shadow_gpa = { .address = trap_frame->r8 };

        cr3 current_slat_root = slat::get_cr3();

        trap_frame->rax = slat::add_slat_code_hook(current_slat_root, target_gpa, shadow_gpa);

        break;

    }

    case hypercall_type_t::log_current_state:

    {

        trap_frame_log_t trap_frame_log = { };

        // Use the original trap frame if available, otherwise fall back to current

        trap_frame_t* source_frame = (original_trap_frame != nullptr) ? original_trap_frame : trap_frame;

        crt::copy_memory(&trap_frame_log, source_frame, sizeof(trap_frame_t));

        log_current_state(trap_frame_log);

        break;

    }

    case hypercall_type_t::flush_logs:

    {

        trap_frame->rax = flush_logs(trap_frame);

        break;

    }

    case hypercall_type_t::remove_slat_code_hook:

    {

        virtual_address_t target_gpa = { .address = trap_frame->rdx };

        cr3 current_slat_root = slat::get_cr3();

        trap_frame->rax = slat::remove_slat_code_hook(current_slat_root, target_gpa);

        break;

    }

    case hypercall_type_t::hide_guest_physical_page:

    {

        virtual_address_t target_gpa = { .address = trap_frame->rdx };

        cr3 current_slat_root = slat::get_cr3();

        trap_frame->rax = slat::hide_physical_page_from_guest(current_slat_root, target_gpa);

        break;

    }

    case hypercall_type_t::get_process_base_by_pid:

    {

        uint64_t target_pid = trap_frame->rdx;

        uint64_t ps_initial_system_process = trap_frame->r8;

        cr3 kernel_cr3 = { .flags = trap_frame->r9 };

        trap_frame->rax = get_process_base_by_pid_from_initial(target_pid, ps_initial_system_process, kernel_cr3);

        break;

    }

    case hypercall_type_t::get_process_cr3:

    {

        trap_frame->rax = get_process_cr3_by_pid(trap_frame);

        break;

    }

    case hypercall_type_t::allocate_hidden_memory:

    {

        trap_frame->rax = allocate_hidden_memory(trap_frame, hypercall_info);

        break;

    }

    case hypercall_type_t::free_hidden_memory:

    {

        trap_frame->rax = free_hidden_memory(trap_frame, hypercall_info);

        break;

    }

    case hypercall_type_t::get_process_eprocess_base:

    {

        trap_frame->rax = get_eprocess_base_by_pid(trap_frame);

        break;

    }

    case hypercall_type_t::dirbase_from_base_address:

    {

        void* base_address = reinterpret_cast<void*>(trap_frame->rdx);

        // Function will auto-initialize with captured ntoskrnl base address

        trap_frame->rax = memory_management::dirbase_from_base_address(base_address, nullptr);

        break;

    }

    case hypercall_type_t::hide_hypervisor_memory:

    {

        std::uint64_t hypervisor_base_physical = trap_frame->rdx;

        std::uint64_t hypervisor_size_bytes = trap_frame->r8;

        trap_frame->rax = memory_management::unlink_hypervisor_memory_from_pfn_database(hypervisor_base_physical, hypervisor_size_bytes);

        break;

    }

    case hypercall_type_t::restore_hypervisor_memory:

    {

        std::uint64_t physical_address = trap_frame->rdx;

        trap_frame->rax = memory_management::restore_physical_page_in_pfn_database(physical_address);

        break;

    }

    case hypercall_type_t::call_dllmain_silently:

    {

        trap_frame->rax = call_dllmain_silently(trap_frame, hypercall_info);

        advance_rip = false;

        break;

    }

    case hypercall_type_t::get_ntoskrnl_base_address:

    {

        // Log hypercall entry

        trap_frame_log_t log_entry = {};

        log_entry.r15 = 0xEEE1; // get_ntoskrnl_base_address hypercall marker

        log_entry.r14 = g_ntoskrnl_base_address; // current cached value

        logs::add_log(log_entry);

        

        // First try the captured boot address

        if (g_ntoskrnl_base_address != 0)

        {

            // Log boot address success

            log_entry = {};

            log_entry.r15 = 0xEEE2; // boot address success

            log_entry.r14 = g_ntoskrnl_base_address;

            logs::add_log(log_entry);

            

            trap_frame->rax = g_ntoskrnl_base_address;

            break;

        }

        

        // Log that boot capture failed, trying dynamic discovery

        log_entry = {};

        log_entry.r15 = 0xEEE3; // boot failed, trying dynamic

        logs::add_log(log_entry);

        

        // If boot capture failed, use KPCR to find ntoskrnl base

        std::uint64_t ntoskrnl_base = get_ntoskrnl_from_kpcr();

        if (ntoskrnl_base != 0)

        {

            // Log dynamic discovery success

            log_entry = {};

            log_entry.r15 = 0xEEE4; // dynamic success

            log_entry.r14 = ntoskrnl_base;

            logs::add_log(log_entry);

            

            // Cache the result for future calls

            g_ntoskrnl_base_address = ntoskrnl_base;

            trap_frame->rax = ntoskrnl_base;

        }

        else

        {

            // Log complete failure

            log_entry = {};

            log_entry.r15 = 0xEEE5; // complete failure

            log_entry.r14 = 0;

            logs::add_log(log_entry);

            

            trap_frame->rax = 0;

        }

        break;

    }

    case hypercall_type_t::update_ntoskrnl_base_address:

    {

        std::uint64_t new_ntoskrnl_base = trap_frame->rdx;

        

        // Log update request

        trap_frame_log_t log_entry = {};

        log_entry.r15 = 0xEEE6; // update_ntoskrnl_base_address hypercall marker

        log_entry.r14 = g_ntoskrnl_base_address; // old value

        log_entry.r13 = new_ntoskrnl_base; // new value

        logs::add_log(log_entry);

        

        if (new_ntoskrnl_base != 0)

        {

            // Update the global ntoskrnl base address

            g_ntoskrnl_base_address = new_ntoskrnl_base;

            

            // Reset the memory management initialization to use the new base

            memory_management::g_initialized = false;

            memory_management::g_mmonp_MmPfnDatabase = nullptr;

            

            // Log successful update

            log_entry = {};

            log_entry.r15 = 0xEEE7; // update success marker

            log_entry.r14 = new_ntoskrnl_base;

            logs::add_log(log_entry);

            

            trap_frame->rax = STATUS_SUCCESS;

        }

        else

        {

            // Log invalid parameter

            log_entry = {};

            log_entry.r15 = 0xEEE8; // update failure marker

            log_entry.r14 = 0;

            logs::add_log(log_entry);

            

            trap_frame->rax = STATUS_INVALID_PARAMETER;

        }

        break;

    }

    case hypercall_type_t::set_kernel_cr3:

    {

        std::uint64_t new_kernel_cr3 = trap_frame->rdx;

        

        // Log kernel CR3 update request

        trap_frame_log_t log_entry = {};

        log_entry.r15 = 0xEEE9; // set_kernel_cr3 hypercall marker

        log_entry.r14 = memory_management::g_kernel_cr3.flags; // old CR3

        log_entry.r13 = new_kernel_cr3; // new CR3

        logs::add_log(log_entry);

        

        if (new_kernel_cr3 != 0)

        {

            // Update the kernel CR3

            memory_management::g_kernel_cr3.flags = new_kernel_cr3;

            

            // Reset initialization to use new CR3

            memory_management::g_initialized = false;

            memory_management::g_mmonp_MmPfnDatabase = nullptr;

            

            // Log successful update

            log_entry = {};

            log_entry.r15 = 0xEEEA; // CR3 update success marker

            log_entry.r14 = new_kernel_cr3;

            logs::add_log(log_entry);

            

            trap_frame->rax = STATUS_SUCCESS;

        }

        else

        {

            // Log invalid parameter

            log_entry = {};

            log_entry.r15 = 0xEEEB; // CR3 update failure marker

            logs::add_log(log_entry);

            

            trap_frame->rax = STATUS_INVALID_PARAMETER;

        }

        break;

    }

    case hypercall_type_t::get_ntoskrnl_base_from_kpcr:

    {

        // Log KPCR discovery request

        trap_frame_log_t log_entry = {};

        log_entry.r15 = 0xEEEC; // get_ntoskrnl_base_from_kpcr hypercall marker

        logs::add_log(log_entry);



        // Actually call the KPCR-based ntoskrnl discovery function

        std::uint64_t ntoskrnl_base = get_ntoskrnl_from_kpcr();



        if (ntoskrnl_base != 0)

        {

            // Log successful discovery

            log_entry = {};

            log_entry.r15 = 0xEEED; // KPCR discovery success marker

            log_entry.r14 = ntoskrnl_base;

            logs::add_log(log_entry);



            // Cache the result for future use

            if (g_ntoskrnl_base_address == 0)

            {

                g_ntoskrnl_base_address = ntoskrnl_base;

            }



            trap_frame->rax = ntoskrnl_base;

        }

        else

        {

            // Log failure

            log_entry = {};

            log_entry.r15 = 0xEEEE; // KPCR discovery failure marker

            logs::add_log(log_entry);



            trap_frame->rax = 0;

        }

        break;

    }

    case hypercall_type_t::get_system_process_cr3_from_kpcr:

    {

        auto log_marker = [](std::uint64_t marker, std::uint64_t r14 = 0, std::uint64_t r13 = 0)

        {

            trap_frame_log_t entry = {};

            entry.r15 = marker;

            entry.r14 = r14;

            entry.r13 = r13;

            logs::add_log(entry);

        };



        const cr3 guest_cr3 = arch::get_guest_cr3();



        cr3 kernel_cr3 = memory_management::g_kernel_cr3;

        if (kernel_cr3.flags == 0)

        {

            kernel_cr3 = guest_cr3;

        }



        cr3 slat_cr3 = slat::get_cr3();



        log_marker(0xF100, kernel_cr3.flags, slat_cr3.flags);



        if (kernel_cr3.flags == 0 || slat_cr3.flags == 0)

        {

            log_marker(0xF1E0);

            trap_frame->rax = 0;

            break;

        }



        std::uint64_t kpcr_base = arch::get_guest_gs_base();



        if (kpcr_base == 0)

        {

            log_marker(0xF1E2);

            trap_frame->rax = 0;

            break;

        }



        log_marker(0xF101, kpcr_base);



        const std::uint64_t current_thread_ptr_address = kpcr_base + 0x188;

        log_marker(0xF102, current_thread_ptr_address);



        auto read_kernel_qword = [&](std::uint64_t address, std::uint64_t& value) -> bool

        {

            log_marker(0xF200, address, kernel_cr3.flags);

            const std::uint64_t bytes = memory_manager::operate_on_guest_virtual_memory(

                slat_cr3,

                &value,

                address,

                kernel_cr3,

                sizeof(value),

                memory_operation_t::read_operation);

            log_marker(0xF201, bytes, value);

            return bytes == sizeof(value);

        };



        std::uint64_t current_thread = 0;

        if (!read_kernel_qword(current_thread_ptr_address, current_thread))

        {

            log_marker(0xF1E3, current_thread_ptr_address);

            trap_frame->rax = 0;

            break;

        }



        if (current_thread == 0)

        {

            log_marker(0xF1E4);

            trap_frame->rax = 0;

            break;

        }



        log_marker(0xF103, current_thread);



        const std::uint64_t eprocess_ptr_address = current_thread + 0x220;

        log_marker(0xF104, eprocess_ptr_address);



        std::uint64_t current_eprocess = 0;

        if (!read_kernel_qword(eprocess_ptr_address, current_eprocess))

        {

            log_marker(0xF1E5, eprocess_ptr_address);

            trap_frame->rax = 0;

            break;

        }



        if (current_eprocess == 0)

        {

            log_marker(0xF1E6);

            trap_frame->rax = 0;

            break;

        }



        log_marker(0xF105, current_eprocess);

        trap_frame->rax = current_eprocess;

        break;

    }

case hypercall_type_t::query_hypervisor_pfn_info:

    {

        std::uint64_t physical_address = trap_frame->rdx;

        std::uint64_t output_buffer_gva = trap_frame->r8;

        

        if (!output_buffer_gva)

        {

            trap_frame->rax = STATUS_INVALID_PARAMETER;

            break;

        }

        

        // Query the PFN information

        HYPERVISOR_PFN_INFO pfn_info = {};

        NTSTATUS status = memory_management::query_hypervisor_pfn_detailed(physical_address, &pfn_info);

        

        if (NT_SUCCESS(status))

        {

            // Copy the result to usermode buffer

            cr3 guest_cr3 = arch::get_guest_cr3();

            cr3 slat_cr3 = slat::get_cr3();

            

            std::uint64_t bytes_written = memory_manager::operate_on_guest_virtual_memory(

                slat_cr3, &pfn_info, output_buffer_gva, guest_cr3, 

                sizeof(HYPERVISOR_PFN_INFO), memory_operation_t::write_operation);

            

            if (bytes_written == sizeof(HYPERVISOR_PFN_INFO))

            {

                trap_frame->rax = STATUS_SUCCESS;

            }

            else

            {

                trap_frame->rax = STATUS_UNSUCCESSFUL;

            }

        }

        else

        {

            trap_frame->rax = status;

        }

        break;

    }

    case hypercall_type_t::get_hypervisor_memory_info:

    {

        std::uint64_t output_buffer_gva = trap_frame->rdx;

        

        if (!output_buffer_gva)

        {

            trap_frame->rax = STATUS_INVALID_PARAMETER;

            break;

        }

        

        // Get the hypervisor memory information

        HYPERVISOR_MEMORY_INFO memory_info = {};

        NTSTATUS status = memory_management::get_hypervisor_memory_info(&memory_info);

        

        if (NT_SUCCESS(status))

        {

            // Copy the result to usermode buffer

            cr3 guest_cr3 = arch::get_guest_cr3();

            cr3 slat_cr3 = slat::get_cr3();

            

            std::uint64_t bytes_written = memory_manager::operate_on_guest_virtual_memory(

                slat_cr3, &memory_info, output_buffer_gva, guest_cr3, 

                sizeof(HYPERVISOR_MEMORY_INFO), memory_operation_t::write_operation);

            

            if (bytes_written == sizeof(HYPERVISOR_MEMORY_INFO))

            {

                trap_frame->rax = STATUS_SUCCESS;

            }

            else

            {

                trap_frame->rax = STATUS_UNSUCCESSFUL;

            }

        }

        else

        {

            trap_frame->rax = status;

        }

        break;

    }

    case hypercall_type_t::test_export_discovery:
    {
        trap_frame_log_t log = {};
        log.r15 = 0xE000; // test_export_discovery start
        logs::add_log(log);

        std::uint64_t output_buffer_gva = trap_frame->rdx;

        if (!output_buffer_gva)
        {
            log.r15 = 0xE0E0; // invalid parameter
            logs::add_log(log);
            trap_frame->rax = STATUS_INVALID_PARAMETER;
            break;
        }

        log.r15 = 0xE001; // buffer valid
        log.r14 = output_buffer_gva;
        logs::add_log(log);

        EXPORT_DISCOVERY_TEST_RESULT result = {};

        // Try to find ntoskrnl (cached or via KPCR)
        std::uint64_t ntoskrnl_base = 0;

        if (g_ntoskrnl_base_address != 0 && g_ntoskrnl_base_address >= 0xFFFF000000000000ULL)
        {
            // Use cached
            ntoskrnl_base = g_ntoskrnl_base_address;
            log.r15 = 0xE002; // using cached
            log.r14 = ntoskrnl_base;
            logs::add_log(log);
        }
        else
        {
            // Discover via KPCR
            log.r15 = 0xE003; // about to discover
            logs::add_log(log);

            ntoskrnl_base = get_ntoskrnl_from_kpcr();

            log.r15 = 0xE004; // discovery completed
            log.r14 = ntoskrnl_base;
            logs::add_log(log);

            if (ntoskrnl_base != 0 && ntoskrnl_base >= 0xFFFF000000000000ULL)
            {
                g_ntoskrnl_base_address = ntoskrnl_base; // Cache it
            }
        }

        // Set result
        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        if (ntoskrnl_base != 0 && ntoskrnl_base >= 0xFFFF000000000000ULL)
        {
            result.ntoskrnl_found = 1;
            result.ntoskrnl_base = ntoskrnl_base;
            result.ntoskrnl_status = STATUS_SUCCESS;
        }
        else
        {
            result.ntoskrnl_found = 0;
            result.ntoskrnl_status = STATUS_NOT_FOUND;

            // Early exit
            memory_manager::operate_on_guest_virtual_memory(
                slat_cr3, &result, output_buffer_gva, guest_cr3,
                sizeof(EXPORT_DISCOVERY_TEST_RESULT), memory_operation_t::write_operation);
            trap_frame->rax = STATUS_SUCCESS;
            break;
        }

        // Initialize parser
        log.r15 = 0xE006; // about to init parser
        logs::add_log(log);

        NTSTATUS parser_status = ntoskrnl_parser::initialize(
            reinterpret_cast<PVOID>(ntoskrnl_base),
            guest_cr3,
            slat_cr3);

        log.r15 = 0xE007; // parser completed
        log.r14 = parser_status;
        logs::add_log(log);

        if (!NT_SUCCESS(parser_status))
        {
            result.export_status = parser_status;
            memory_manager::operate_on_guest_virtual_memory(
                slat_cr3, &result, output_buffer_gva, guest_cr3,
                sizeof(EXPORT_DISCOVERY_TEST_RESULT), memory_operation_t::write_operation);
            trap_frame->rax = STATUS_SUCCESS;
            break;
        }

        // Find MmGetVirtualForPhysical export
        log.r15 = 0xE008; // about to find export
        logs::add_log(log);

        PVOID mm_get_virtual = ntoskrnl_parser::get_export_by_name(
            "MmGetVirtualForPhysical",
            reinterpret_cast<PVOID>(ntoskrnl_base),
            guest_cr3,
            slat_cr3);

        log.r15 = 0xE009; // export search completed
        log.r14 = reinterpret_cast<std::uint64_t>(mm_get_virtual);
        logs::add_log(log);

        if (!mm_get_virtual)
        {
            result.mm_get_virtual_for_physical_found = 0;
            result.export_status = STATUS_PROCEDURE_NOT_FOUND;

            memory_manager::operate_on_guest_virtual_memory(
                slat_cr3, &result, output_buffer_gva, guest_cr3,
                sizeof(EXPORT_DISCOVERY_TEST_RESULT), memory_operation_t::write_operation);
            trap_frame->rax = STATUS_SUCCESS;
            break;
        }

        result.mm_get_virtual_for_physical_found = 1;
        result.mm_get_virtual_for_physical_address = reinterpret_cast<std::uint64_t>(mm_get_virtual);
        result.export_status = STATUS_SUCCESS;

        // Read the first 128 bytes to capture the full pattern + MmPfnDatabase pointer
        log.r15 = 0xE00A; // about to test read
        logs::add_log(log);

        std::uint64_t test_bytes_read = memory_manager::operate_on_guest_virtual_memory(
            slat_cr3, result.mm_get_virtual_bytes,
            reinterpret_cast<std::uint64_t>(mm_get_virtual),
            guest_cr3, sizeof(result.mm_get_virtual_bytes),
            memory_operation_t::read_operation);

        log.r15 = 0xE00A1; // test read completed
        log.r14 = test_bytes_read;
        log.r13 = (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[0]) << 56) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[1]) << 48) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[2]) << 40) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[3]) << 32) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[4]) << 24) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[5]) << 16) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[6]) << 8) |
                  (static_cast<std::uint64_t>(result.mm_get_virtual_bytes[7]));
        logs::add_log(log);

        // Search for MmPfnDatabase pattern (split_memory reads internally)
        log.r15 = 0xE00B; // about to search pattern
        logs::add_log(log);

        static const UCHAR pattern[] = {
            0x48, 0x8B, 0xC1,        // mov rax, rcx
            0x48, 0xC1, 0xE8, 0x0C,  // shr rax, 0Ch
            0x48, 0x8D, 0x14, 0x40,  // lea rdx, [rax + rax * 2]
            0x48, 0x03, 0xD2,        // add rdx, rdx
            0x48, 0xB8               // mov rax, <imm64>
        };

        PVOID pattern_location = ntoskrnl_parser::split_memory(
            mm_get_virtual, 0x200, pattern, sizeof(pattern),
            guest_cr3, slat_cr3);

        log.r15 = 0xE00C; // pattern search completed
        log.r14 = reinterpret_cast<std::uint64_t>(pattern_location);
        logs::add_log(log);

        if (pattern_location)
        {
            result.pattern_found = 1;
            result.pattern_offset = reinterpret_cast<std::uint64_t>(pattern_location) -
                                   reinterpret_cast<std::uint64_t>(mm_get_virtual);
            result.pattern_status = STATUS_SUCCESS;

            // The MmPfnDatabase pointer is the 8-byte immediate after the pattern
            // Pattern is 15 bytes: 48 8B C1 48 C1 E8 0C 48 8D 14 40 48 03 D2 48 B8
            // The next 8 bytes after "48 B8" (mov rax, imm64) contain MmPfnDatabase
            if (result.pattern_offset + 15 + 8 <= sizeof(result.mm_get_virtual_bytes))
            {
                // Read the 8-byte pointer at offset (pattern_offset + 15)
                std::uint64_t mmpfn_database = 0;
                crt::copy_memory(&mmpfn_database, &result.mm_get_virtual_bytes[result.pattern_offset + 15], sizeof(mmpfn_database));

                result.mmpfn_database_address = mmpfn_database;
                result.mmpfn_database_found = 1;
                result.mmpfn_status = STATUS_SUCCESS;
            }
            else
            {
                result.mmpfn_database_found = 0;
                result.mmpfn_status = STATUS_UNSUCCESSFUL;
            }
        }
        else
        {
            result.pattern_found = 0;
            result.pattern_status = STATUS_NOT_FOUND;
        }

        // Copy result to usermode
        log.r15 = 0xE005; // about to write result
        log.r14 = sizeof(EXPORT_DISCOVERY_TEST_RESULT);
        logs::add_log(log);

        std::uint64_t bytes_written = memory_manager::operate_on_guest_virtual_memory(
            slat_cr3, &result, output_buffer_gva, guest_cr3,
            sizeof(EXPORT_DISCOVERY_TEST_RESULT), memory_operation_t::write_operation);

        log.r15 = 0xE0FF; // test_export_discovery end
        log.r14 = bytes_written;
        logs::add_log(log);

        trap_frame->rax = (bytes_written == sizeof(EXPORT_DISCOVERY_TEST_RESULT)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

        break;
    }

    case hypercall_type_t::enable_cr3_caching:
    {
        // Deprecated - CR3 caching now always runs opportunistically
        trap_frame->rax = STATUS_SUCCESS;
        break;
    }

    case hypercall_type_t::disable_cr3_caching:
    {
        // Deprecated - CR3 caching now always runs opportunistically
        trap_frame->rax = STATUS_SUCCESS;
        break;
    }

    case hypercall_type_t::get_cached_cr3:
    {
        // Get the cached CR3 for the currently monitored process
        std::uint64_t cached_cr3 = cr3_cache::get_cached_cr3();
        trap_frame->rax = cached_cr3;
        break;
    }

    case hypercall_type_t::get_cr3_cache_stats:
    {
        std::uint64_t output_buffer_gva = trap_frame->rdx;

        if (!output_buffer_gva)
        {
            trap_frame->rax = STATUS_INVALID_PARAMETER;
            break;
        }

        cr3_cache::cr3_stats_t stats = cr3_cache::get_statistics();

        cr3 guest_cr3 = arch::get_guest_cr3();
        cr3 slat_cr3 = slat::get_cr3();

        std::uint64_t bytes_written = memory_manager::operate_on_guest_virtual_memory(
            slat_cr3, &stats, output_buffer_gva, guest_cr3,
            sizeof(cr3_cache::cr3_stats_t), memory_operation_t::write_operation);

        trap_frame->rax = (bytes_written == sizeof(cr3_cache::cr3_stats_t)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        break;
    }

    case hypercall_type_t::set_target_pid_for_cr3_caching:
    {
        std::uint64_t target_pid = trap_frame->rdx;
        cr3_cache::set_target_pid(target_pid);
        trap_frame->rax = STATUS_SUCCESS;
        break;
    }

    case hypercall_type_t::get_target_pid_for_cr3_caching:
    {
        // Return 0 - not implemented in current simplified version
        trap_frame->rax = 0;
        break;
    }

    default:

        break;

    }



    if (advance_rip)

    {

        arch::advance_guest_rip();

    }

}



std::uint64_t hypercall::dirbase_from_base_address(void* base_address)

{

    // This function is kept for compatibility, but usermode should pass ntoskrnl_base

    // For now, we'll return 0 to indicate this version is not supported

    // Note: Using direct logging since logs::print might not be available in all contexts

    return 0;

}

