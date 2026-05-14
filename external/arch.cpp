#include "arch.h"

#include <intrin.h>

#include "../memory_manager/memory_manager.h"
#include "../crt/crt.h"

#ifdef _INTELMACHINE
#include <intrin.h>
#include <ia32-doc/ia32.hpp>

std::uint64_t arch::get_vmexit_instruction_length()
{
#ifdef _INTELMACHINE
	std::uint64_t vmexit_instruction_length = 0;
	__vmx_vmread(VMCS_VMEXIT_INSTRUCTION_LENGTH, &vmexit_instruction_length);
	return vmexit_instruction_length;
#else
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->control.next_rip - vmcb->save_state.rip;
#endif
}

#else
std::uint8_t get_vmcb_routine_bytes[27];

typedef vmcb_t*(*get_vmcb_routine_t)();

vmcb_t* arch::get_vmcb()
{
	get_vmcb_routine_t get_vmcb_routine = reinterpret_cast<get_vmcb_routine_t>(&get_vmcb_routine_bytes[0]);

	return get_vmcb_routine();
}

void arch::parse_vmcb_gadget(const std::uint8_t* get_vmcb_gadget)
{
	constexpr std::uint32_t final_needed_opcode_offset = 23;

	crt::copy_memory(&get_vmcb_routine_bytes[0], get_vmcb_gadget, final_needed_opcode_offset);

	if (get_vmcb_gadget[25] == 8)
	{
		constexpr std::uint8_t return_bytes[4] = {
			0x48, 0x8B, 0x00,
			0xC3
		};

		crt::copy_memory(&get_vmcb_routine_bytes[final_needed_opcode_offset], &return_bytes[0], sizeof(return_bytes));
	}
	else
	{
		get_vmcb_routine_bytes[final_needed_opcode_offset] = 0xC3;
	}
}
#endif


void arch::flush_target_process_tlb(cr3 target_cr3, std::uint64_t gva)
{
#ifdef _INTELMACHINE
	invpcid_descriptor descriptor = {};
	descriptor.pcid = 0;
	descriptor.linear_address = gva;
	_invpcid(0, &descriptor);
#else
	vmcb_t* const vmcb = get_vmcb();
	vmcb->control.tlb_control = tlb_control_t::flush_guest_tlb_entries;

	vmcb->control.clean.flags = (1 << 2);

	__invlpg(reinterpret_cast<void*>(gva));
#endif
}

void arch::flush_guest_tlb(std::uint64_t guest_cr3_value)
{
#ifdef _INTELMACHINE
	invpcid_descriptor descriptor = {};
	descriptor.pcid = 0; // Assuming PCID 0 for the guest
	descriptor.linear_address = 0;
	_invpcid(1, &descriptor);
#else
	vmcb_t* const vmcb = get_vmcb();
	vmcb->control.tlb_control = tlb_control_t::flush_guest_tlb_entries;

	vmcb->control.clean.flags = (1 << 2);
#endif
}


std::uint64_t arch::get_vmexit_reason()
{
#ifdef _INTELMACHINE
	std::uint64_t vmexit_reason = 0;

	__vmx_vmread(VMCS_EXIT_REASON, &vmexit_reason);

	return vmexit_reason;
#else
	vmcb_t* const vmcb = get_vmcb();

	return vmcb->control.vmexit_reason;
#endif
}

std::uint8_t arch::is_cpuid(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return vmexit_reason == VMX_EXIT_REASON_EXECUTE_CPUID;
#else
	return vmexit_reason == SVM_EXIT_REASON_CPUID;
#endif
}

std::uint8_t arch::is_slat_violation(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return vmexit_reason == VMX_EXIT_REASON_EPT_VIOLATION;
#else
	return vmexit_reason == SVM_EXIT_REASON_NPF;
#endif
}

std::uint8_t arch::is_cr3_write(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return vmexit_reason == VMX_EXIT_REASON_MOV_CR;
#else
	return vmexit_reason == SVM_EXIT_REASON_WRITE_CR3;
#endif
}

std::uint8_t arch::is_non_maskable_interrupt_exit(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	if (vmexit_reason != VMX_EXIT_REASON_EXCEPTION_OR_NMI)
	{
		return 0;
	}

	std::uint64_t raw_interruption_information = 0;

	__vmx_vmread(VMCS_VMEXIT_INTERRUPTION_INFORMATION, &raw_interruption_information);

	vmexit_interrupt_information interrupt_information = { .flags = static_cast<std::uint32_t>(raw_interruption_information) };

	return interrupt_information.interruption_type == interruption_type::non_maskable_interrupt;
#else
	return vmexit_reason == SVM_EXIT_REASON_PHYSICAL_NMI;
#endif
}

cr3 arch::get_guest_cr3()
{
	cr3 guest_cr3 = { };

#ifdef _INTELMACHINE
	__vmx_vmread(VMCS_GUEST_CR3, &guest_cr3.flags);
#else
	vmcb_t* const vmcb = get_vmcb();

	guest_cr3.flags = vmcb->save_state.cr3;
#endif

	return guest_cr3;
}

std::uint64_t arch::get_guest_gs_base()
{
#ifdef _INTELMACHINE
	std::uint64_t gs_base = 0;
	__vmx_vmread(VMCS_GUEST_GS_BASE, &gs_base);
	return gs_base;
#else
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->save_state.gs.base;
#endif
}

std::uint64_t arch::get_guest_cs_selector()
{
#ifdef _INTELMACHINE
	std::uint64_t cs_selector = 0;
	__vmx_vmread(VMCS_GUEST_CS_SELECTOR, &cs_selector);
	return cs_selector;
#else
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->save_state.cs.selector;
#endif
}

std::uint64_t arch::get_guest_idtr_base()
{
#ifdef _INTELMACHINE
	std::uint64_t idtr_base = 0;
	__vmx_vmread(VMCS_GUEST_IDTR_BASE, &idtr_base);
	return idtr_base;
#else
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->save_state.idtr.base;
#endif
}

std::uint64_t arch::get_guest_rip()
{
#ifdef _INTELMACHINE
	std::uint64_t guest_rip = 0;

	__vmx_vmread(VMCS_GUEST_RIP, &guest_rip);

	return guest_rip;
#else
	vmcb_t* const vmcb = get_vmcb();

	return vmcb->save_state.rip;
#endif
}

void arch::set_guest_rip(std::uint64_t guest_rip)
{
#ifdef _INTELMACHINE
	__vmx_vmwrite(VMCS_GUEST_RIP, guest_rip);
#else
	vmcb_t* const vmcb = get_vmcb();

	vmcb->save_state.rip = guest_rip;

	vmcb->control.clean.flags = (1 << 2);
#endif
}

void arch::advance_guest_rip()
{
#ifdef _INTELMACHINE
	std::uint64_t guest_rip = get_guest_rip();

	std::uint64_t instruction_length = get_vmexit_instruction_length();

	std::uint64_t next_rip = guest_rip + instruction_length;
#else
	vmcb_t* const vmcb = get_vmcb();

	std::uint64_t next_rip = vmcb->control.next_rip;
#endif

	set_guest_rip(next_rip);
}

// Guest RSP getter and setter
std::uint64_t arch::get_guest_rsp()
{
#ifdef _INTELMACHINE
	std::uint64_t guest_rsp = 0;
	__vmx_vmread(VMCS_GUEST_RSP, &guest_rsp);
	return guest_rsp;
#else
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->save_state.rsp;
#endif
}

void arch::set_guest_rsp(std::uint64_t guest_rsp)
{
#ifdef _INTELMACHINE
	__vmx_vmwrite(VMCS_GUEST_RSP, guest_rsp);
#else
	vmcb_t* const vmcb = get_vmcb();
	vmcb->save_state.rsp = guest_rsp;

	vmcb->control.clean.flags = (1 << 2);
#endif
}

#ifdef _INTELMACHINE


#else

std::uint64_t arch::get_guest_rax()
{
	vmcb_t* const vmcb = get_vmcb();
	return vmcb->save_state.rax;
}

void arch::set_guest_rax(std::uint64_t guest_rax)
{
	vmcb_t* const vmcb = get_vmcb();
	vmcb->save_state.rax = guest_rax;

	vmcb->control.clean.flags = (1 << 2);
}


#endif

void arch::invlpg(std::uint64_t address)
{
	flush_guest_tlb(get_guest_cr3().flags);
}

void arch::set_monitor_trap_flag(bool enable)
{
#ifdef _INTELMACHINE
	std::uint64_t cpu_based_vm_exec_controls = 0;

	__vmx_vmread(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, &cpu_based_vm_exec_controls);

	if (enable)
	{
		cpu_based_vm_exec_controls |= IA32_VMX_PROCBASED_CTLS_MONITOR_TRAP_FLAG_FLAG;
	}
	else
	{
		cpu_based_vm_exec_controls &= ~IA32_VMX_PROCBASED_CTLS_MONITOR_TRAP_FLAG_FLAG;
	}

	__vmx_vmwrite(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, cpu_based_vm_exec_controls);
#else
#endif
}

bool arch::is_mtf_exit(std::uint64_t vmexit_reason)
{
#ifdef _INTELMACHINE
	return (vmexit_reason == VMX_EXIT_REASON_MONITOR_TRAP_FLAG);
#else
	return false;
#endif
}

