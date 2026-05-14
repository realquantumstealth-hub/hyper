#include "slat.h"
#include "../arch/arch.h"
#include "../memory_manager/memory_manager.h"
#include "../memory_manager/heap_manager.h"
#include "../crt/crt.h"
#include <ia32-doc/ia32.hpp>
#include <cstdint>

#include "../interrupts/interrupts.h"

#ifdef _INTELMACHINE
#include <intrin.h>

using slat_pml4e = ept_pml4e;
using slat_pdpte_1gb = ept_pdpte_1gb;
using slat_pdpte = ept_pdpte;
using slat_pde_2mb = ept_pde_2mb;
using slat_pde = ept_pde;
using slat_pte = ept_pte;
#else
using slat_pml4e = pml4e_64;
using slat_pdpte_1gb = pdpte_1gb_64;
using slat_pdpte = pdpte_64;
using slat_pde_2mb = pde_2mb_64;
using slat_pde = pde_64;
using slat_pte = pte_64;
#endif

namespace
{
	crt::mutex_t hook_mutex = { };

	std::uint64_t dummy_page_pfn = 0;

	// Track hidden heap range
	std::uint64_t hidden_heap_start = 0;
	std::uint64_t hidden_heap_end = 0;

#ifdef _INTELMACHINE
	// Dual static EPTs for Intel
	cr3 clean_ept_cr3 = { };    // EPT0: clean mappings
	cr3 hooked_ept_cr3 = { };   // EPT1: hooked pages (execute-only shadow)
	std::uint8_t current_ept_is_hooked = 0; // Track which EPT is active
#else
	pml4e_64* hook_nested_pml4 = nullptr;
	cr3 hyperv_nested_cr3 = { };
	cr3 hook_nested_cr3 = { };
	std::uint16_t current_asid = 1; // ASID for NPT switching
	std::uint16_t clean_asid = 1;
	std::uint16_t hooked_asid = 2;
#endif
}

void slat::set_up()
{
	constexpr std::uint64_t hook_entries_wanted = 0x1000 / sizeof(hook_entry_t);

	void* hook_entries_allocation = heap_manager::allocate_page();

	available_hook_list_head = static_cast<hook_entry_t*>(hook_entries_allocation);

	hook_entry_t* current_entry = available_hook_list_head;

	for (std::uint64_t i = 0; i < hook_entries_wanted - 1; i++)
	{
		current_entry->set_next(current_entry + 1);
		current_entry->set_original_pfn(0);
		current_entry->set_shadow_pfn(0);

		current_entry = current_entry->get_next();
	}

	current_entry->set_original_pfn(0);
	current_entry->set_shadow_pfn(0);
	current_entry->set_next(nullptr);

	void* dummy_page_allocation = heap_manager::allocate_page();

	std::uint64_t dummy_page_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(dummy_page_allocation));

	dummy_page_pfn = dummy_page_physical_address >> 12;

	crt::set_memory(dummy_page_allocation, 0, 0x1000);

#ifndef _INTELMACHINE
	hook_nested_pml4 = static_cast<pml4e_64*>(heap_manager::allocate_page());

	crt::set_memory(hook_nested_pml4, 0, sizeof(pml4e_64) * 512);

	std::uint64_t pml4_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(hook_nested_pml4));

	hook_nested_cr3.address_of_page_directory = pml4_physical_address >> 12;
#endif
}

cr3 slat::get_cr3()
{
	cr3 slat_cr3 = { };

#ifdef _INTELMACHINE
	ept_pointer ept_pointer = { };

	__vmx_vmread(VMCS_CTRL_EPT_POINTER, &ept_pointer.flags);

	slat_cr3.address_of_page_directory = ept_pointer.page_frame_number;
#else
	slat_cr3 = hyperv_nested_cr3;
#endif

	return slat_cr3;
}

cr3 slat::get_clean_ept()
{
#ifdef _INTELMACHINE
	return clean_ept_cr3;
#else
	return hyperv_nested_cr3;
#endif
}

cr3 slat::get_hooked_ept()
{
#ifdef _INTELMACHINE
	return hooked_ept_cr3;
#else
	return hook_nested_cr3;
#endif
}

void slat::switch_to_ept(cr3 target_ept)
{
#ifdef _INTELMACHINE
	// Switch EPTP via VMCS write
	ept_pointer new_ept_ptr = { };
	new_ept_ptr.memory_type = 6; // Write-back
	new_ept_ptr.page_walk_length = 3; // 4-level page walk (N-1)
	new_ept_ptr.enable_access_and_dirty_flags = 0;
	new_ept_ptr.page_frame_number = target_ept.address_of_page_directory;

	__vmx_vmwrite(VMCS_CTRL_EPT_POINTER, new_ept_ptr.flags);

	// Flush with INVEPT single-context
	invept_descriptor descriptor = {};
	descriptor.ept_pointer = new_ept_ptr.flags;
	invalidate_ept_mappings(invept_type::invept_single_context, descriptor);

	// Update tracking
	current_ept_is_hooked = (target_ept.address_of_page_directory == hooked_ept_cr3.address_of_page_directory) ? 1 : 0;
#else
	// AMD: switch NCR3 and ASID
	vmcb_t* const vmcb = arch::get_vmcb();

	std::uint16_t new_asid = (target_ept.flags == hook_nested_cr3.flags) ? hooked_asid : clean_asid;

	vmcb->control.nested_cr3 = target_ept;
	vmcb->control.guest_asid = new_asid;
	current_asid = new_asid;

	// ASID switch handles TLB, no INVLPGA needed
	vmcb->control.tlb_control = tlb_control_t::do_nothing;
#endif
}

void slat::flush_current_logical_processor_slat_cache(const std::uint8_t has_slat_cr3_changed)
{
#ifdef _INTELMACHINE
	invept_descriptor descriptor = {};
	invalidate_ept_mappings(invept_type::invept_all_context, descriptor);
#else
	vmcb_t* const vmcb = arch::get_vmcb();

	vmcb->control.tlb_control = tlb_control_t::flush_guest_tlb_entries;

	if (has_slat_cr3_changed == 1)
	{
		vmcb->control.clean.nested_paging = 0;
	}
#endif
}

void slat::flush_all_logical_processors_slat_cache()
{
	flush_current_logical_processor_slat_cache();

	interrupts::set_all_nmi_ready();
	interrupts::send_nmi_all_but_self();
}

slat_pml4e* slat_get_pml4e(cr3 slat_cr3, virtual_address_t guest_physical_address)
{
	slat_pml4e* pml4 = reinterpret_cast<slat_pml4e*>(memory_manager::map_host_physical(slat_cr3.address_of_page_directory << 12));

	slat_pml4e* pml4e = &pml4[guest_physical_address.pml4_idx];

	return pml4e;
}

slat_pdpte* slat_get_pdpte(slat_pml4e* pml4e, virtual_address_t guest_physical_address)
{
	slat_pdpte* pdpt = reinterpret_cast<slat_pdpte*>(memory_manager::map_host_physical(pml4e->page_frame_number << 12));

	slat_pdpte* pdpte = &pdpt[guest_physical_address.pdpt_idx];

	return pdpte;
}

slat_pde* slat_get_pde(slat_pdpte* pdpte, virtual_address_t guest_physical_address)
{
	slat_pde* pd = reinterpret_cast<slat_pde*>(memory_manager::map_host_physical(pdpte->page_frame_number << 12));

	slat_pde* pde = &pd[guest_physical_address.pd_idx];

	return pde;
}

slat_pte* slat_get_pte(slat_pde* pde, virtual_address_t guest_physical_address)
{
	slat_pte* pt = reinterpret_cast<slat_pte*>(memory_manager::map_host_physical(pde->page_frame_number << 12));

	slat_pte* pte = &pt[guest_physical_address.pt_idx];

	return pte;
}

#ifdef _INTELMACHINE
// Deep copy entire EPT structure for dual-EPT design
cr3 deep_copy_ept(cr3 source_ept)
{
	slat_pml4e* source_pml4 = reinterpret_cast<slat_pml4e*>(memory_manager::map_host_physical(source_ept.address_of_page_directory << 12));
	slat_pml4e* dest_pml4 = static_cast<slat_pml4e*>(heap_manager::allocate_page());

	if (dest_pml4 == nullptr) {
		return cr3{ .address_of_page_directory = 0 };
	}

	crt::set_memory(dest_pml4, 0, 0x1000);

	// Copy each PML4 entry
	for (std::uint64_t pml4_idx = 0; pml4_idx < 512; pml4_idx++) {
		slat_pml4e* src_pml4e = &source_pml4[pml4_idx];
		if (src_pml4e->read_access == 0 && src_pml4e->write_access == 0 && src_pml4e->execute_access == 0) {
			continue; // Skip empty entries
		}

		slat_pdpte* src_pdpt = reinterpret_cast<slat_pdpte*>(memory_manager::map_host_physical(src_pml4e->page_frame_number << 12));
		slat_pdpte* dest_pdpt = static_cast<slat_pdpte*>(heap_manager::allocate_page());

		if (dest_pdpt == nullptr) continue;

		crt::set_memory(dest_pdpt, 0, 0x1000);

		// Copy each PDPT entry
		for (std::uint64_t pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
			slat_pdpte* src_pdpte = &src_pdpt[pdpt_idx];
			if (src_pdpte->read_access == 0 && src_pdpte->write_access == 0 && src_pdpte->execute_access == 0) {
				continue;
			}

			slat_pdpte_1gb* large_pdpte = reinterpret_cast<slat_pdpte_1gb*>(src_pdpte);
			if (large_pdpte->large_page == 1) {
				dest_pdpt[pdpt_idx] = *src_pdpte; // Direct copy for 1GB pages
				continue;
			}

			slat_pde* src_pd = reinterpret_cast<slat_pde*>(memory_manager::map_host_physical(src_pdpte->page_frame_number << 12));
			slat_pde* dest_pd = static_cast<slat_pde*>(heap_manager::allocate_page());

			if (dest_pd == nullptr) continue;

			crt::set_memory(dest_pd, 0, 0x1000);

			// Copy each PD entry
			for (std::uint64_t pd_idx = 0; pd_idx < 512; pd_idx++) {
				slat_pde* src_pde = &src_pd[pd_idx];
				if (src_pde->read_access == 0 && src_pde->write_access == 0 && src_pde->execute_access == 0) {
					continue;
				}

				slat_pde_2mb* large_pde = reinterpret_cast<slat_pde_2mb*>(src_pde);
				if (large_pde->large_page == 1) {
					dest_pd[pd_idx] = *src_pde; // Direct copy for 2MB pages
					continue;
				}

				slat_pte* src_pt = reinterpret_cast<slat_pte*>(memory_manager::map_host_physical(src_pde->page_frame_number << 12));
				slat_pte* dest_pt = static_cast<slat_pte*>(heap_manager::allocate_page());

				if (dest_pt == nullptr) continue;

				crt::set_memory(dest_pt, 0, 0x1000);

				// Copy all PT entries
				for (std::uint64_t pt_idx = 0; pt_idx < 512; pt_idx++) {
					dest_pt[pt_idx] = src_pt[pt_idx];
				}

				std::uint64_t dest_pt_phys = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(dest_pt));
				dest_pd[pd_idx] = *src_pde;
				dest_pd[pd_idx].page_frame_number = dest_pt_phys >> 12;
			}

			std::uint64_t dest_pd_phys = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(dest_pd));
			dest_pdpt[pdpt_idx] = *src_pdpte;
			dest_pdpt[pdpt_idx].page_frame_number = dest_pd_phys >> 12;
		}

		std::uint64_t dest_pdpt_phys = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(dest_pdpt));
		dest_pml4[pml4_idx] = *src_pml4e;
		dest_pml4[pml4_idx].page_frame_number = dest_pdpt_phys >> 12;
	}

	std::uint64_t dest_pml4_phys = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(dest_pml4));
	cr3 result = { .address_of_page_directory = dest_pml4_phys >> 12 };
	return result;
}
#endif

std::uint8_t slat_split_2mb_pde(slat_pde_2mb* large_pde)
{
	slat_pte* pt = static_cast<slat_pte*>(heap_manager::allocate_page());

	if (pt == nullptr)
	{
		return 0;
	}

	for (std::uint64_t i = 0; i < 512; i++)
	{
		slat_pte* pte = &pt[i];

		pte->flags = 0;

#ifdef _INTELMACHINE
		// FIX: Instead of copying flags, explicitly set full permissions
		// for the new 4KB pages. The higher-level PDE will enforce
		// user-mode access.
		pte->read_access = 1;
		pte->write_access = 1;
		pte->execute_access = 1;
		pte->memory_type = large_pde->memory_type;
		pte->ignore_pat = large_pde->ignore_pat;
#else
		pte->execute_disable = large_pde->execute_disable;
		pte->present = large_pde->present;
		pte->write = large_pde->write;
		pte->global = large_pde->global;
		pte->pat = large_pde->pat;
		pte->protection_key = large_pde->protection_key;
		pte->page_level_write_through = large_pde->page_level_write_through;
		pte->page_level_cache_disable = large_pde->page_level_cache_disable;
		pte->supervisor = large_pde->supervisor;
#endif

		pte->accessed = large_pde->accessed;
		pte->dirty = large_pde->dirty;

		pte->page_frame_number = (large_pde->page_frame_number << 9) + i;
	}

	std::uint64_t pt_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(pt));

	slat_pde new_pde = { .flags = 0 };

	new_pde.page_frame_number = pt_physical_address >> 12;

#ifdef _INTELMACHINE
	new_pde.read_access = 1;
	new_pde.write_access = 1;
	new_pde.execute_access = 1;
	new_pde.user_mode_execute = 1;
#else
	new_pde.present = 1;
	new_pde.write = 1;
	new_pde.supervisor = 0;
#endif

	large_pde->flags = new_pde.flags;

	return 1;
}

std::uint8_t slat_split_1gb_pdpte(slat_pdpte_1gb* large_pdpte)
{
	slat_pde_2mb* pd = static_cast<slat_pde_2mb*>(heap_manager::allocate_page());

	if (pd == nullptr)
	{
		return 0;
	}

	for (std::uint64_t i = 0; i < 512; i++)
	{
		slat_pde_2mb* pde = &pd[i];

		pde->flags = 0;

#ifdef _INTELMACHINE
		// FIX: Explicitly set permissions rather than copying.
		pde->read_access = 1;
		pde->write_access = 1;
		pde->execute_access = 1;
		pde->user_mode_execute = 1;
		pde->memory_type = large_pdpte->memory_type;
		pde->ignore_pat = large_pdpte->ignore_pat;
#else
		pde->execute_disable = large_pdpte->execute_disable;
		pde->present = large_pdpte->present;
		pde->write = large_pdpte->write;
		pde->global = large_pdpte->global;
		pde->pat = large_pdpte->pat;
		pde->protection_key = large_pdpte->protection_key;
		pde->page_level_write_through = large_pdpte->page_level_write_through;
		pde->page_level_cache_disable = large_pdpte->page_level_cache_disable;
		pde->supervisor = large_pdpte->supervisor;
#endif

		pde->accessed = large_pdpte->accessed;
		pde->dirty = large_pdpte->dirty;

		pde->page_frame_number = (large_pdpte->page_frame_number << 9) + i;
		pde->large_page = 1;
	}

	std::uint64_t pd_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(pd));

	slat_pdpte new_pdpte = { .flags = 0 };

	new_pdpte.page_frame_number = pd_physical_address >> 12;

#ifdef _INTELMACHINE
	new_pdpte.read_access = 1;
	new_pdpte.write_access = 1;
	new_pdpte.execute_access = 1;
	new_pdpte.user_mode_execute = 1;
#else
	new_pdpte.present = 1;
	new_pdpte.write = 1;
	new_pdpte.supervisor = 0;
#endif

	large_pdpte->flags = new_pdpte.flags;

	return 1;
}

slat_pde* slat_get_pde(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint8_t force_split_pages = 0)
{
	slat_pml4e* pml4e = slat_get_pml4e(slat_cr3, guest_physical_address);

	if (pml4e == nullptr)
	{
		return nullptr;
	}

	slat_pdpte* pdpte = slat_get_pdpte(pml4e, guest_physical_address);

	if (pdpte == nullptr)
	{
		return nullptr;
	}

	slat_pdpte_1gb* large_pdpte = reinterpret_cast<slat_pdpte_1gb*>(pdpte);

	if (large_pdpte->large_page == 1 && (force_split_pages == 0 || slat_split_1gb_pdpte(large_pdpte) == 0))
	{
		return nullptr;
	}

	return slat_get_pde(pdpte, guest_physical_address);
}

slat_pte* slat_get_pte(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint8_t force_split_pages = 0, std::uint8_t* paging_split_state = nullptr)
{
	slat_pde* pde = slat_get_pde(slat_cr3, guest_physical_address, force_split_pages);

	if (pde == nullptr)
	{
		return nullptr;
	}

	slat_pde_2mb* large_pde = reinterpret_cast<slat_pde_2mb*>(pde);

	if (large_pde->large_page == 1)
	{
		if (force_split_pages == 0 || slat_split_2mb_pde(large_pde) == 0)
		{
			return nullptr;
		}

		if (paging_split_state != nullptr)
		{
			*paging_split_state = 1;
		}
	}

	return slat_get_pte(pde, guest_physical_address);
}

std::uint8_t slat_merge_4kb_pt(cr3 slat_cr3, virtual_address_t guest_physical_address)
{
	slat_pde* pde = slat_get_pde(slat_cr3, guest_physical_address);

	if (pde == nullptr)
	{
		return 0;
	}

	slat_pde_2mb* large_pde = reinterpret_cast<slat_pde_2mb*>(pde);

	if (large_pde->large_page == 1)
	{
		return 1;
	}

	std::uint64_t pt_physical_address = pde->page_frame_number << 12;

	slat_pte* pte = slat_get_pte(pde, guest_physical_address);

	slat_pde_2mb new_large_pde = { large_pde->flags };

#ifdef _INTELMACHINE
	new_large_pde.execute_access = pte->execute_access;
	new_large_pde.read_access = pte->read_access;
	new_large_pde.write_access = pte->write_access;
	new_large_pde.memory_type = pte->memory_type;
	new_large_pde.ignore_pat = pte->ignore_pat;
	new_large_pde.user_mode_execute = pte->user_mode_execute;
	new_large_pde.verify_guest_paging = pte->verify_guest_paging;
	new_large_pde.paging_write_access = pte->paging_write_access;
	new_large_pde.supervisor_shadow_stack = pte->supervisor_shadow_stack;
	new_large_pde.suppress_ve = pte->suppress_ve;
#else
	new_large_pde.execute_disable = pte->execute_disable;
	new_large_pde.present = pte->present;
	new_large_pde.write = pte->write;
	new_large_pde.global = pte->global;
	new_large_pde.pat = pte->pat;
	new_large_pde.protection_key = pte->protection_key;
	new_large_pde.page_level_write_through = pte->page_level_write_through;
	new_large_pde.page_level_cache_disable = pte->page_level_cache_disable;
	new_large_pde.supervisor = pte->supervisor;
#endif

	new_large_pde.page_frame_number = pte->page_frame_number >> 9;
	new_large_pde.large_page = 1;

	*large_pde = new_large_pde;

	void* pt_allocation_mapped = reinterpret_cast<void*>(memory_manager::map_host_physical(pt_physical_address));

	heap_manager::free_page(pt_allocation_mapped);

	return 1;
}

#ifndef _INTELMACHINE
void set_nested_cr3(vmcb_t* const vmcb, cr3 cr3)
{
	vmcb->control.nested_cr3 = cr3;

	slat::flush_current_logical_processor_slat_cache(1);
}

void make_pd_identity_map(pde_2mb_64* hook_pd, std::uint64_t pml4_index, std::uint64_t pdpt_index)
{
	for (std::uint64_t pd_index = 0; pd_index < 512; pd_index++)
	{
		pde_2mb_64* hook_pde = &hook_pd[pd_index];

		hook_pde->flags = 0;

		hook_pde->present = 1;
		hook_pde->execute_disable = 1;
		hook_pde->large_page = 1;
		hook_pde->write = 1;

		hook_pde->page_frame_number = (pml4_index << 18) + (pdpt_index << 9) + pd_index;
	}
}

void make_pdpt_identity_map(pdpte_64* hyperv_pdpt, pdpte_64* hook_pdpt, std::uint64_t pml4_index)
{
	for (std::uint64_t pdpt_index = 0; pdpt_index < 512; pdpt_index++)
	{
		pdpte_64* hyperv_pdpte = &hyperv_pdpt[pdpt_index];
		pdpte_64* hook_pdpte = &hook_pdpt[pdpt_index];

		hook_pdpte->flags = 0;

		if (hyperv_pdpte->present == 0)
		{
			continue;
		}

		pde_64* hyperv_pd = slat_get_pde(hyperv_pdpte, { });

		if (hyperv_pd == nullptr)
		{
			continue;
		}

		pde_2mb_64* hook_pd = static_cast<pde_2mb_64*>(heap_manager::allocate_page());

		if (hook_pd == nullptr)
		{
			continue;
		}

		std::uint64_t hook_pd_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(hook_pd));

		hook_pdpte->present = 1;
		hook_pdpte->write = 1;
		hook_pdpte->page_frame_number = hook_pd_physical_address >> 12;

		make_pd_identity_map(hook_pd, pml4_index, pdpt_index);
	}
}

void make_pml4_identity_map(pml4e_64* hyperv_pml4, pml4e_64* hook_pml4)
{
	for (std::uint64_t pml4_index = 0; pml4_index < 512; pml4_index++)
	{
		pml4e_64* hyperv_pml4e = &hyperv_pml4[pml4_index];
		pml4e_64* hook_pml4e = &hook_pml4[pml4_index];

		hook_pml4e->flags = 0;

		if (hyperv_pml4e->present == 0)
		{
			continue;
		}

		pdpte_64* hyperv_pdpt = slat_get_pdpte(hyperv_pml4e, { });

		if (hyperv_pdpt == nullptr)
		{
			continue;
		}

		pdpte_64* hook_pdpt = static_cast<pdpte_64*>(heap_manager::allocate_page());

		if (hook_pdpt == nullptr)
		{
			continue;
		}

		std::uint64_t hook_pdpt_physical_address = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(hook_pdpt));

		hook_pml4e->present = 1;
		hook_pml4e->write = 1;
		hook_pml4e->page_frame_number = hook_pdpt_physical_address >> 12;

		make_pdpt_identity_map(hyperv_pdpt, hook_pdpt, pml4_index);
	}
}

#endif

void slat::process_first_vmexit()
{
#ifdef _INTELMACHINE
	// Build dual static EPTs at boot
	cr3 current_ept = get_cr3();

	// EPT0: clean - copy current EPT as-is
	clean_ept_cr3 = deep_copy_ept(current_ept);

	// EPT1: hooked - initially same as clean, will be modified when hooks are added
	hooked_ept_cr3 = deep_copy_ept(current_ept);

	// Start with clean EPT active
	current_ept_is_hooked = 0;
#else
	vmcb_t* const vmcb = arch::get_vmcb();

	hyperv_nested_cr3 = vmcb->control.nested_cr3;

	slat_pml4e* hyperv_pml4 = slat_get_pml4e(hyperv_nested_cr3, { });

	make_pml4_identity_map(hyperv_pml4, hook_nested_pml4);

	// Initialize ASID
	vmcb->control.guest_asid = clean_asid;
	current_asid = clean_asid;
#endif
}

std::uint8_t slat::try_hide_heap_pages(std::uint64_t heap_physical_address, std::uint64_t heap_physical_end)
{
	// Save the hidden heap range for violation detection
	hidden_heap_start = heap_physical_address;
	hidden_heap_end = heap_physical_end;

	std::uint64_t current_address = heap_physical_address;

	while (current_address < heap_physical_end)
	{
#ifdef _INTELMACHINE
		// Hide from both EPTs - make completely inaccessible
		hide_physical_page_from_guest(clean_ept_cr3, { .address = current_address });
		hide_physical_page_from_guest(hooked_ept_cr3, { .address = current_address });
#else
		hide_physical_page_from_guest(hyperv_nested_cr3, { .address = current_address });
		hide_physical_page_from_guest(hook_nested_cr3, { .address = current_address });
#endif

		current_address += 0x1000;
	}

	return 1;
}

bool slat::is_hidden_heap_page(virtual_address_t guest_physical_address)
{
	return guest_physical_address.address >= hidden_heap_start &&
	       guest_physical_address.address < hidden_heap_end;
}

std::uint64_t slat::translate_guest_physical_address(cr3 slat_cr3, virtual_address_t guest_physical_address, std::uint64_t* size_left_of_slat_page)
{
#ifdef _INTELMACHINE
	hook_entry_t* hook_entry = hook_entry_t::find(used_hook_list_head, guest_physical_address.address >> 12, nullptr);

	if (hook_entry != nullptr)
	{
		const std::uint64_t page_offset = guest_physical_address.offset;

		if (size_left_of_slat_page != nullptr)
		{
			*size_left_of_slat_page = (1ull << 12) - page_offset;
		}

		return (hook_entry->get_original_pfn() << 12) + guest_physical_address.offset;
	}
#endif

	return memory_manager::translate_host_virtual_address(slat_cr3, guest_physical_address, size_left_of_slat_page);
}

#ifndef _INTELMACHINE
void set_page_executability(cr3 hook_nested_cr3, virtual_address_t target_guest_address, std::uint8_t execute_disable)
{
	slat_pte* pte = slat_get_pte(hook_nested_cr3, target_guest_address, 1);

	if (pte != nullptr)
	{
		pte->execute_disable = execute_disable;
	}
}

void set_previous_page_executability(cr3 hook_nested_cr3, virtual_address_t target_guest_address, std::uint8_t execute_disable)
{
	virtual_address_t previous_page_address = { .address = target_guest_address.address - 0x1000 };

	set_page_executability(hook_nested_cr3, previous_page_address, execute_disable);
}

void set_next_page_executability(cr3 hook_nested_cr3, virtual_address_t target_guest_address, std::uint8_t execute_disable)
{
	virtual_address_t next_page_address = { .address = target_guest_address.address + 0x1000 };

	set_page_executability(hook_nested_cr3, next_page_address, execute_disable);
}

void fix_split_instructions(cr3 hook_nested_cr3, virtual_address_t target_guest_address)
{
	set_previous_page_executability(hook_nested_cr3, target_guest_address, 0);
	set_next_page_executability(hook_nested_cr3, target_guest_address, 0);
}

void unfix_split_instructions(slat::hook_entry_t* hook_entry, cr3 hook_nested_cr3, virtual_address_t target_guest_address)
{
	slat::hook_entry_t* other_hook_entry_in_range = slat::hook_entry_t::find_closest_in_2mb_range(slat::used_hook_list_head, target_guest_address.address >> 12, hook_entry);

	if (other_hook_entry_in_range != nullptr)
	{
		std::int64_t source_pfn = static_cast<std::int64_t>(hook_entry->get_original_pfn());
		std::int64_t other_pfn = static_cast<std::int64_t>(other_hook_entry_in_range->get_original_pfn());

		std::int64_t pfn_difference = source_pfn - other_pfn;
		std::int64_t abs_pfn_difference = crt::abs(pfn_difference);

		std::uint8_t is_page_nearby = abs_pfn_difference <= 2;

		std::uint8_t has_fixed = 1;

		if (is_page_nearby == 1 && 0 < pfn_difference)
		{
			set_next_page_executability(hook_nested_cr3, target_guest_address, 1);

			has_fixed = 1;
		}
		else if (is_page_nearby == 1) // negative pfn difference
		{
			set_previous_page_executability(hook_nested_cr3, target_guest_address, 1);

			has_fixed = 1;
		}

		if (abs_pfn_difference == 1)
		{
			// current page must be executable for the nearby hook

			set_page_executability(hook_nested_cr3, target_guest_address, 0);
		}

		if (has_fixed == 1)
		{
			return;
		}
	}

	// no nearby hooks enough to have to shed executability
	set_previous_page_executability(hook_nested_cr3, target_guest_address, 0);
	set_next_page_executability(hook_nested_cr3, target_guest_address, 0);
}
#endif

std::uint64_t slat::add_slat_code_hook(cr3 slat_cr3, virtual_address_t target_guest_physical_address, virtual_address_t shadow_page_guest_physical_address)
{
	hook_mutex.lock();

	hook_entry_t* already_present_hook_entry = hook_entry_t::find(used_hook_list_head, target_guest_physical_address.address >> 12);

	if (already_present_hook_entry != nullptr)
	{
		hook_mutex.release();
		return 0;
	}

#ifdef _INTELMACHINE
	// Dual-EPT approach: modify EPT1 (hooked), leave EPT0 (clean) unchanged
	std::uint8_t paging_split_state = 0;

	// Get PTE from clean EPT to save original permissions
	slat_pte* clean_pte = slat_get_pte(clean_ept_cr3, target_guest_physical_address, 0);

	if (clean_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	// Get/create PTE in hooked EPT
	slat_pte* hooked_pte = slat_get_pte(hooked_ept_cr3, target_guest_physical_address, 1, &paging_split_state);

	if (hooked_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}
#else
	std::uint8_t paging_split_state = 0;
	slat_pte* target_pte = slat_get_pte(slat_cr3, target_guest_physical_address, 1, &paging_split_state);

	if (target_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	slat_pte* hook_target_pte = slat_get_pte(hook_nested_cr3, target_guest_physical_address, 1);

	if (hook_target_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}
#endif

	if (paging_split_state == 0)
	{
		hook_entry_t* similar_space_hook_entry = hook_entry_t::find_in_2mb_range(used_hook_list_head, target_guest_physical_address.address >> 12);

		if (similar_space_hook_entry != nullptr)
		{
			paging_split_state = static_cast<std::uint8_t>(similar_space_hook_entry->get_paging_split_state());
		}
	}

	std::uint64_t shadow_page_host_physical_address = shadow_page_guest_physical_address.address;

	if (shadow_page_host_physical_address == 0)
	{
		hook_mutex.release();
		return 0;
	}

	hook_entry_t* hook_entry = available_hook_list_head;

	if (hook_entry == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	available_hook_list_head = hook_entry->get_next();

	hook_entry->set_next(used_hook_list_head);
	hook_entry->set_paging_split_state(paging_split_state);

#ifdef _INTELMACHINE
	// Save original state from clean EPT
	hook_entry->set_original_pfn(clean_pte->page_frame_number);
	hook_entry->set_original_read_access(clean_pte->read_access);
	hook_entry->set_original_write_access(clean_pte->write_access);
	hook_entry->set_original_execute_access(clean_pte->execute_access);

	hook_entry->set_shadow_pfn(shadow_page_host_physical_address >> 12);

	// Modify EPT1 (hooked): execute-only, shadow PFN
	hooked_pte->page_frame_number = hook_entry->get_shadow_pfn();
	hooked_pte->execute_access = 1;
	hooked_pte->read_access = 0;
	hooked_pte->write_access = 0;

	// EPT0 (clean) remains unchanged - no races!
#else
	hook_entry->set_original_pfn(target_pte->page_frame_number);
	hook_entry->set_shadow_pfn(shadow_page_host_physical_address >> 12);
	hook_entry->set_original_execute_access(!target_pte->execute_disable);

	hook_target_pte->execute_disable = 0;
	hook_target_pte->page_frame_number = hook_entry->get_shadow_pfn();

	fix_split_instructions(hook_nested_cr3, target_guest_physical_address);

	target_pte->execute_disable = 1;
#endif

	used_hook_list_head = hook_entry;

	hook_mutex.release();

	flush_all_logical_processors_slat_cache();

	return 1;
}

std::uint8_t does_hook_need_merge(slat::hook_entry_t* hook_entry, virtual_address_t guest_physical_address)
{
	if (hook_entry == nullptr)
	{
		return 0;
	}

	std::uint8_t does_source_hook_require_merge = hook_entry->get_paging_split_state() == 1;

	if (does_source_hook_require_merge == 0)
	{
		return 0;
	}

	slat::hook_entry_t* other_hook_entry_in_range = slat::hook_entry_t::find_in_2mb_range(slat::used_hook_list_head, guest_physical_address.address >> 12, hook_entry);

	return other_hook_entry_in_range == nullptr;
}



std::uint8_t clean_up_hook_ptes(cr3 slat_cr3, virtual_address_t target_guest_physical_address, slat::hook_entry_t* hook_entry)
{
#ifdef _INTELMACHINE
	// Dual-EPT: restore hooked EPT to match clean EPT
	slat_pte* hooked_pte = slat_get_pte(hooked_ept_cr3, target_guest_physical_address);

	if (hooked_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	slat_pte* clean_pte = slat_get_pte(clean_ept_cr3, target_guest_physical_address);

	if (clean_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	// Restore hooked EPT entry to match clean EPT
	*hooked_pte = *clean_pte;

	if (does_hook_need_merge(hook_entry, target_guest_physical_address) == 1)
	{
		slat_merge_4kb_pt(hooked_ept_cr3, target_guest_physical_address);
	}
#else
	slat_pte* target_pte = slat_get_pte(slat_cr3, target_guest_physical_address);

	if (target_pte == nullptr)
	{
		hook_mutex.release();
		return 0;
	}

	slat_pte* hook_target_pte = slat_get_pte(hook_nested_cr3, target_guest_physical_address);

	if (hook_target_pte == nullptr)
	{
		return 0;
	}

	slat_pte new_pte = { .flags = target_pte->flags };
	new_pte.execute_disable = !hook_entry->get_original_execute_access();

	*target_pte = new_pte;

	hook_target_pte->page_frame_number = hook_entry->get_original_pfn();
	hook_target_pte->execute_disable = 1;

	unfix_split_instructions(hook_entry, hook_nested_cr3, target_guest_physical_address);

	if (does_hook_need_merge(hook_entry, target_guest_physical_address) == 1)
	{
		slat_merge_4kb_pt(slat_cr3, target_guest_physical_address);
	}
#endif

	return 1;
}

void clean_up_hook_entry(slat::hook_entry_t* hook_entry, slat::hook_entry_t* previous_hook_entry)
{
	if (previous_hook_entry == nullptr)
	{
		slat::used_hook_list_head = hook_entry->get_next();
	}
	else
	{
		previous_hook_entry->set_next(hook_entry->get_next());
	}

	hook_entry->set_next(slat::available_hook_list_head);

	slat::available_hook_list_head = hook_entry;
}

std::uint64_t slat::remove_slat_code_hook(cr3 slat_cr3, virtual_address_t target_guest_physical_address)
{
	hook_mutex.lock();

	hook_entry_t* previous_hook_entry = nullptr;

	hook_entry_t* hook_entry = hook_entry_t::find(used_hook_list_head, target_guest_physical_address.address >> 12, &previous_hook_entry);

	if (hook_entry == nullptr)
	{
		hook_mutex.release();

		return 0;
	}

	std::uint8_t hook_pte_cleanup_status = clean_up_hook_ptes(slat_cr3, target_guest_physical_address, hook_entry);

	clean_up_hook_entry(hook_entry, previous_hook_entry);

	hook_mutex.release();

	flush_all_logical_processors_slat_cache();

	return hook_pte_cleanup_status;
}

std::uint64_t slat::hide_physical_page_from_guest(cr3 slat_cr3, virtual_address_t guest_physical_address)
{
	hook_mutex.lock();

	slat_pte* target_pte = slat_get_pte(slat_cr3, guest_physical_address, 1);

	if (target_pte == nullptr)
	{
		hook_mutex.release();

		return 0;
	}

#ifdef _INTELMACHINE
	// Intel: Make page completely inaccessible (R=0, W=0, X=0)
	// This causes EPT violations on any access, making the GPA appear non-existent
	target_pte->read_access = 0;
	target_pte->write_access = 0;
	target_pte->execute_access = 0;
	// Keep PFN as dummy to avoid misconfiguration
	target_pte->page_frame_number = dummy_page_pfn;
#else
	// AMD: Clear present bit in NPT - page appears as not mapped
	target_pte->present = 0;
	target_pte->page_frame_number = dummy_page_pfn;
#endif

	hook_mutex.release();

	return 1;
}

std::uint8_t slat::process_slat_violation()
{
#ifdef _INTELMACHINE
	vmx_exit_qualification_ept_violation qualification = { };

	__vmx_vmread(VMCS_EXIT_QUALIFICATION, &qualification.flags);

	if (qualification.execute_access == 1 && (qualification.write_access == 1 || qualification.read_access == 1))
	{
		return 0;
	}

	virtual_address_t physical_address = { };

	__vmx_vmread(qualification.caused_by_translation == 1 ? VMCS_GUEST_PHYSICAL_ADDRESS : VMCS_EXIT_GUEST_LINEAR_ADDRESS, &physical_address.address);

	// Check if accessing hidden heap - make it appear as non-existent
	if (is_hidden_heap_page(physical_address))
	{
		// Guest tried to access hidden hypervisor heap
		// Return 0 to not handle - will cause guest to see fault/error
		// The page appears completely non-existent (R=W=X=0)
		return 0;
	}

	hook_entry_t* hook_entry = hook_entry_t::find(used_hook_list_head, physical_address.address >> 12);

	if (hook_entry == nullptr)
	{
		return 0;
	}

	// Dual-EPT switching approach: no PTE modification, just switch EPTP
	if (qualification.execute_access == 1)
	{
		// Execute access: switch to EPT1 (hooked) with execute-only shadow pages
		if (current_ept_is_hooked == 0)
		{
			switch_to_ept(hooked_ept_cr3);
		}
	}
	else
	{
		// Read/Write access: switch to EPT0 (clean) with original pages
		if (current_ept_is_hooked == 1)
		{
			switch_to_ept(clean_ept_cr3);
		}
	}
#else
	vmcb_t* const vmcb = arch::get_vmcb();

	npf_exit_info_1 npf_info = { .flags = vmcb->control.first_exit_info };

	if (npf_info.present == 0 || npf_info.execute_access == 0)
	{
		return 0;
	}

	virtual_address_t physical_address = { vmcb->control.second_exit_info };

	// Check if accessing hidden heap - make it appear as non-existent
	if (is_hidden_heap_page(physical_address))
	{
		// Guest tried to access hidden hypervisor heap
		// Return 0 to not handle - will cause NPF in guest
		// The page appears completely non-existent (present=0)
		return 0;
	}

	hook_entry_t* hook_entry = hook_entry_t::find(used_hook_list_head, physical_address.address >> 12);

	if (hook_entry == nullptr)
	{
		if (current_asid == hooked_asid)
		{
			// Switch back to clean NCR3 with ASID switch
			vmcb->control.nested_cr3 = hyperv_nested_cr3;
			vmcb->control.guest_asid = clean_asid;
			current_asid = clean_asid;

			return 1;
		}

		return 0;
	}

	// Switch to hooked NCR3 with ASID switch
	vmcb->control.nested_cr3 = hook_nested_cr3;
	vmcb->control.guest_asid = hooked_asid;
	current_asid = hooked_asid;
#endif

	return 1;
}
bool slat::map_guest_physical_to_host_physical(std::uint64_t guest_physical_address, std::uint64_t host_physical_address, bool readable, bool writable, bool executable)
{
	if (guest_physical_address == 0 || host_physical_address == 0) {
		return false;
	}

	cr3 slat_cr3 = get_cr3();
	virtual_address_t gpa = { .address = guest_physical_address };

	// Get/create PML4E
	slat_pml4e* pml4e = slat_get_pml4e(slat_cr3, gpa);
	if (!pml4e) {
		return false;
	}

	if (pml4e->page_frame_number == 0) {
		void* new_pdpt = heap_manager::allocate_page();
		if (!new_pdpt) {
			return false;
		}

		crt::set_memory(new_pdpt, 0, 0x1000);
		std::uint64_t pdpt_physical = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pdpt));

		pml4e->page_frame_number = pdpt_physical >> 12;
#ifdef _INTELMACHINE
		pml4e->read_access = 1;
		pml4e->write_access = 1;
		pml4e->execute_access = 1;
		pml4e->user_mode_execute = 1; // Allow user-mode access
#else
		pml4e->present = 1;
		pml4e->write = 1;
		pml4e->supervisor = 0;  // Correct: 0 for User, 1 for Supervisor
#endif
	}

	// Get/create PDPTE
	slat_pdpte* pdpte = slat_get_pdpte(pml4e, gpa);
	if (!pdpte) {
		return false;
	}

	if (pdpte->page_frame_number == 0) {
		void* new_pd = heap_manager::allocate_page();
		if (!new_pd) {
			return false;
		}

		crt::set_memory(new_pd, 0, 0x1000);
		std::uint64_t pd_physical = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pd));

		pdpte->page_frame_number = pd_physical >> 12;
#ifdef _INTELMACHINE
		pdpte->read_access = 1;
		pdpte->write_access = 1;
		pdpte->execute_access = 1;
		pdpte->user_mode_execute = 1; // Allow user-mode access
#else
		pdpte->present = 1;
		pdpte->write = 1;
		pdpte->supervisor = 0;  // Correct: 0 for User
#endif
	}

	// Get/create PDE
	slat_pde* pde = slat_get_pde(pdpte, gpa);
	if (!pde) {
		return false;
	}

	if (pde->page_frame_number == 0) {
		void* new_pt = heap_manager::allocate_page();
		if (!new_pt) {
			return false;
		}

		crt::set_memory(new_pt, 0, 0x1000);
		std::uint64_t pt_physical = memory_manager::unmap_host_physical(reinterpret_cast<std::uint64_t>(new_pt));

		pde->page_frame_number = pt_physical >> 12;
#ifdef _INTELMACHINE
		pde->read_access = 1;
		pde->write_access = 1;
		pde->execute_access = 1;
		pde->user_mode_execute = 1; // Allow user-mode access
#else
		pde->present = 1;
		pde->write = 1;
		pde->supervisor = 0;  // Correct: 0 for User
#endif
	}

	// Get/create PTE
	slat_pte* pte = slat_get_pte(pde, gpa);
	if (!pte) {
		return false;
	}

	// Set the final mapping with requested permissions
	pte->page_frame_number = host_physical_address >> 12;

#ifdef _INTELMACHINE
	pte->read_access = readable ? 1 : 0;
	pte->write_access = writable ? 1 : 0;
	pte->execute_access = executable ? 1 : 0;
#else
	pte->present = 1; // Must be present to be accessible
	pte->write = writable ? 1 : 0;
	pte->execute_disable = executable ? 0 : 1;
	pte->supervisor = 0;  // Correct: 0 for User
#endif

	flush_all_logical_processors_slat_cache();
	return true;
}




// ADDED: Implementation for unmapping a GPA from the SLAT.
void slat::unmap_guest_physical(std::uint64_t guest_physical_address)
{
	if (guest_physical_address == 0) {
		return;
	}

	cr3 slat_cr3 = get_cr3();
	virtual_address_t gpa = { .address = guest_physical_address };

	slat_pml4e* pml4e = slat_get_pml4e(slat_cr3, gpa);
#ifdef _INTELMACHINE
	if (!pml4e || !(pml4e->read_access || pml4e->write_access || pml4e->execute_access)) return;
#else
	if (!pml4e || !pml4e->present) return;
#endif

	slat_pdpte* pdpte = slat_get_pdpte(pml4e, gpa);
#ifdef _INTELMACHINE
	if (!pdpte || !(pdpte->read_access || pdpte->write_access || pdpte->execute_access)) return;
#else
	if (!pdpte || !pdpte->present) return;
#endif

	slat_pde* pde = slat_get_pde(pdpte, gpa);
#ifdef _INTELMACHINE
	if (!pde || !(pde->read_access || pde->write_access || pde->execute_access)) return;
#else
	if (!pde || !pde->present) return;
#endif

	slat_pte* pte = slat_get_pte(pde, gpa);
	if (pte) {
		pte->flags = 0; // Clear all flags to unmap
	}

	flush_all_logical_processors_slat_cache();
}

slat::hook_entry_t* slat::hook_entry_t::get_next() const
{
	return reinterpret_cast<hook_entry_t*>(this->next);
}

void slat::hook_entry_t::set_next(hook_entry_t* next_entry)
{
	this->next = reinterpret_cast<std::uint64_t>(next_entry);
}

std::uint64_t slat::hook_entry_t::get_original_pfn() const
{
	return (static_cast<uint64_t>(this->higher_original_pfn) << 32) | this->lower_original_pfn;
}

std::uint64_t slat::hook_entry_t::get_shadow_pfn() const
{
	return (static_cast<uint64_t>(this->higher_shadow_pfn) << 32) | this->lower_shadow_pfn;
}

std::uint64_t slat::hook_entry_t::get_original_read_access() const
{
	return this->original_read_access;
}

std::uint64_t slat::hook_entry_t::get_original_write_access() const
{
	return this->original_write_access;
}

std::uint64_t slat::hook_entry_t::get_original_execute_access() const
{
	return this->original_execute_access;
}

std::uint64_t slat::hook_entry_t::get_paging_split_state() const
{
	return this->paging_split_state;
}

void slat::hook_entry_t::set_original_pfn(const std::uint64_t original_pfn)
{
	this->lower_original_pfn = static_cast<std::uint32_t>(original_pfn);
	this->higher_original_pfn = original_pfn >> 32;
}

void slat::hook_entry_t::set_shadow_pfn(const std::uint64_t shadow_pfn)
{
	this->lower_shadow_pfn = static_cast<std::uint32_t>(shadow_pfn);
	this->higher_shadow_pfn = shadow_pfn >> 32;
}

void slat::hook_entry_t::set_original_read_access(const std::uint64_t original_read_access_in)
{
	this->original_read_access = original_read_access_in;
}

void slat::hook_entry_t::set_original_write_access(const std::uint64_t original_write_access_in)
{
	this->original_write_access = original_write_access_in;
}

void slat::hook_entry_t::set_original_execute_access(const std::uint64_t original_execute_access_in)
{
	this->original_execute_access = original_execute_access_in;
}

void slat::hook_entry_t::set_paging_split_state(const std::uint64_t paging_split_state_in)
{
	this->paging_split_state = paging_split_state_in;
}

slat::hook_entry_t* slat::hook_entry_t::find(hook_entry_t* list_head, std::uint64_t target_original_4kb_pfn, hook_entry_t** previous_entry_out)
{
	hook_entry_t* current_entry = list_head;
	hook_entry_t* previous_entry = nullptr;

	while (current_entry != nullptr)
	{
		if (current_entry->get_original_pfn() == target_original_4kb_pfn)
		{
			if (previous_entry_out != nullptr)
			{
				*previous_entry_out = previous_entry;
			}

			return current_entry;
		}

		previous_entry = current_entry;
		current_entry = current_entry->get_next();
	}

	return nullptr;
}

slat::hook_entry_t* slat::hook_entry_t::find_in_2mb_range(hook_entry_t* list_head, std::uint64_t target_original_4kb_pfn, hook_entry_t* excluding_hook)
{
	hook_entry_t* current_entry = list_head;

	std::uint64_t target_2mb_pfn = target_original_4kb_pfn >> 9;

	while (current_entry != nullptr)
	{
		std::uint64_t current_hook_2mb_pfn = current_entry->get_original_pfn() >> 9;

		if (excluding_hook != current_entry && current_hook_2mb_pfn == target_2mb_pfn)
		{
			return current_entry;
		}

		current_entry = current_entry->get_next();
	}

	return nullptr;
}

slat::hook_entry_t* slat::hook_entry_t::find_closest_in_2mb_range(hook_entry_t* list_head, std::uint64_t target_original_4kb_pfn, hook_entry_t* excluding_hook)
{
	hook_entry_t* current_entry = list_head;

	std::uint64_t target_2mb_pfn = target_original_4kb_pfn >> 9;

	hook_entry_t* closest_entry = nullptr;
	std::int64_t closest_difference = INT64_MAX;

	while (current_entry != nullptr)
	{
		std::uint64_t current_hook_4kb_pfn = current_entry->get_original_pfn();
		std::uint64_t current_hook_2mb_pfn = current_hook_4kb_pfn >> 9;

		if (excluding_hook != current_entry && current_hook_2mb_pfn == target_2mb_pfn)
		{
			std::int64_t current_difference = crt::abs(static_cast<std::int64_t>(current_hook_4kb_pfn) - static_cast<std::int64_t>(target_original_4kb_pfn));

			if (current_difference < closest_difference)
			{
				closest_difference = current_difference;
				closest_entry = current_entry;
			}
		}

		current_entry = current_entry->get_next();
	}

	return closest_entry;
}