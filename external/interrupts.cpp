#include "interrupts.h"
#include "../slat/slat.h"
#include "../crt/crt.h"
#include "../arch/arch.h"
#include "ia32-doc/ia32.hpp"
#include <intrin.h>
#include "../memory_manager/heap_manager.h"

//
// ADDED: Missing VMX definitions for exception injection.
// These values are from the Intel Software Developer's Manual.
//
#ifdef _INTELMACHINE
#define VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD 0x00004016
#define VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE 0x00004018
#define VMCS_IDT_VECTORING_INFORMATION_FIELD 0x4408
#define VMCS_IDT_VECTORING_ERROR_CODE 0x440A
#define VM_ENTRY_INTR_TYPE_HARDWARE_EXCEPTION 3

union vmx_vm_entry_interruption_information
{
    std::uint32_t flags;
    struct
    {
        std::uint32_t vector : 8;
        std::uint32_t interruption_type : 3;
        std::uint32_t deliver_error_code : 1;
        std::uint32_t reserved : 19;
        std::uint32_t valid : 1;
    };
};
#endif

extern "C"
{
    std::uint64_t original_nmi_handler = 0;
    void nmi_standalone_entry();
    void nmi_entry();
}

namespace
{
    crt::bitmap_t processor_nmi_states = { };

    void secure_interrupt_vectors(segment_descriptor_register_64 idtr)
    {
        if (idtr.base_address != 0)
        {
            segment_descriptor_interrupt_gate_64* interrupt_gates =
                reinterpret_cast<segment_descriptor_interrupt_gate_64*>(idtr.base_address);

            for (int i = 0; i < 256; i++)
            {
                if (interrupt_gates[i].present == 0)
                {
                    crt::set_memory(&interrupt_gates[i], 0, sizeof(segment_descriptor_interrupt_gate_64));
                }
            }
        }
    }

    void setup_secure_nmi_handling()
    {
        segment_descriptor_register_64 idtr = { };
        __sidt(&idtr);
        secure_interrupt_vectors(idtr);
    }
}

void set_up_nmi_handling()
{
    segment_descriptor_register_64 idtr = { };
    __sidt(&idtr);
    if (idtr.base_address == 0)
    {
        return;
    }
    segment_descriptor_interrupt_gate_64* interrupt_gates = reinterpret_cast<segment_descriptor_interrupt_gate_64*>(idtr.base_address);
    segment_descriptor_interrupt_gate_64* nmi_gate = &interrupt_gates[2];
    segment_descriptor_interrupt_gate_64 new_gate = *nmi_gate;
    std::uint64_t new_handler = reinterpret_cast<std::uint64_t>(nmi_entry);

    if (new_gate.present == 0)
    {
        segment_selector gate_segment_selector = { .index = 1 };
        new_gate.segment_selector = gate_segment_selector.flags;
        new_gate.type = SEGMENT_DESCRIPTOR_TYPE_INTERRUPT_GATE;
        new_gate.present = 1;
        new_handler = reinterpret_cast<std::uint64_t>(nmi_standalone_entry);
    }
    else
    {
        original_nmi_handler = nmi_gate->offset_low | (nmi_gate->offset_middle << 16) | (static_cast<uint64_t>(nmi_gate->offset_high) << 32);
    }
    new_gate.offset_low = new_handler & 0xFFFF;
    new_gate.offset_middle = (new_handler >> 16) & 0xFFFF;
    new_gate.offset_high = (new_handler >> 32) & 0xFFFFFFFF;
    *nmi_gate = new_gate;
}

void interrupts::set_up()
{
    constexpr std::uint64_t processor_nmi_state_count = 0x1000 / sizeof(std::uint64_t);
    processor_nmi_states.set_map_value(static_cast<std::uint64_t*>(heap_manager::allocate_page()));
    processor_nmi_states.set_map_value_count(processor_nmi_state_count);
    apic = apic_t::create_instance();
    setup_secure_nmi_handling();
#ifdef _INTELMACHINE
    set_up_nmi_handling();
#endif
}

void interrupts::set_all_nmi_ready() { processor_nmi_states.set_all(); }
void interrupts::set_nmi_ready(const std::uint64_t apic_id) { processor_nmi_states.set(apic_id); }
void interrupts::clear_nmi_ready(const std::uint64_t apic_id) { processor_nmi_states.clear(apic_id); }
std::uint8_t interrupts::is_nmi_ready(const std::uint64_t apic_id) { return processor_nmi_states.is_set(apic_id); }

void interrupts::process_nmi()
{
    const std::uint64_t current_apic_id = apic_t::current_apic_id();
    if (is_nmi_ready(current_apic_id) == 1)
    {
        slat::flush_current_logical_processor_slat_cache();
        clear_nmi_ready(current_apic_id);
    }
}

void interrupts::send_nmi_all_but_self() { apic->send_nmi(icr_destination_shorthand_t::all_but_self); }

void interrupts::inject_guest_exception(std::uint8_t vector, std::uint32_t error_code, bool has_error_code)
{
#ifdef _INTELMACHINE
    vmx_vm_entry_interruption_information entry_interruption_info = { .flags = 0 };

    entry_interruption_info.valid = 1;
    entry_interruption_info.interruption_type = VM_ENTRY_INTR_TYPE_HARDWARE_EXCEPTION;
    entry_interruption_info.vector = vector;
    entry_interruption_info.deliver_error_code = has_error_code ? 1 : 0;

    __vmx_vmwrite(VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD, entry_interruption_info.flags);

    if (has_error_code)
    {
        __vmx_vmwrite(VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
    }
#else
    vmcb_t* const vmcb = arch::get_vmcb();
    if (vmcb)
    {
        vmcb->control.event_injection.flags = 0;
        vmcb->control.event_injection.valid = 1;
        vmcb->control.event_injection.type = static_cast<std::uint32_t>(event_injection_type::exception);
        vmcb->control.event_injection.vector = vector;
        vmcb->control.event_injection.error_code_valid = has_error_code ? 1 : 0;
        vmcb->control.event_injection_error_code = error_code;
        // Mark VMCB clean bits. We only mark ASID as clean (bit 2).
        // By NOT setting the 'I' bit (bit 0), we ensure the processor re-reads
        // the interrupt-related fields, including our new EVENTINJ, before VM-entry.
        vmcb->control.clean.flags = (1 << 2);
    }
#endif
}

void interrupts::inject_external_interrupt(std::uint8_t vector)
{
#ifdef _INTELMACHINE
	// Check if an event is already pending for injection
	std::uint64_t entry_int_info = 0;
	__vmx_vmread(VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD, &entry_int_info);

	// Only inject if no other event is already queued
	if (!(entry_int_info & 0x80000000))
	{
		vmx_vm_entry_interruption_information new_event = { .flags = 0 };
		new_event.valid = 1;
		new_event.interruption_type = 0; // External interrupt
		new_event.vector = vector;
		new_event.deliver_error_code = 0; // No error code for external interrupts

		__vmx_vmwrite(VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD, new_event.flags);
	}
#else
	// AMD: Use event injection in VMCB
	vmcb_t* const vmcb = arch::get_vmcb();
	if (vmcb)
	{
		// Only inject if no event already queued
		if (!vmcb->control.event_injection.valid)
		{
			vmcb->control.event_injection.flags = 0;
			vmcb->control.event_injection.valid = 1;
			vmcb->control.event_injection.type = static_cast<std::uint32_t>(event_injection_type::external_interrupt);
			vmcb->control.event_injection.vector = vector;
			vmcb->control.event_injection.error_code_valid = 0;
			// Minimal clean bits - only ASID is clean (bit 2)
			vmcb->control.clean.flags = (1 << 2);
		}
	}
#endif
}

void interrupts::reinject_pending_events()
{
#ifndef _INTELMACHINE
    // AMD: Check if there's a pending interrupt from the EXITINTINFO field
    vmcb_t* const vmcb = arch::get_vmcb();
    if (!vmcb)
        return;

    // If there was a pending interrupt at VMEXIT time, reinject it
    if (vmcb->control.exit_int_info.valid)
    {
        // Don't overwrite if we already have an event queued for injection
        if (!vmcb->control.event_injection.valid)
        {
            // Copy the exit interrupt info to event injection
            vmcb->control.event_injection.flags = vmcb->control.exit_int_info.flags;

            // Also copy the error code if present
            if (vmcb->control.exit_int_info.error_code_valid)
            {
                vmcb->control.event_injection_error_code = vmcb->control.exit_int_info_error_code;
            }

            // Mark VMCB clean bits. By not marking the 'I' bit (interrupts) as clean,
            // we ensure the CPU re-reads the EVENTINJ field we just populated from
            // EXITINTINFO.
            vmcb->control.clean.flags = (1 << 2);
        }
    }
#else
    // Intel: Check IDT-vectoring information field (NOT exit interruption info!)
    // IDT-vectoring = event being delivered when VM exit occurred (need to reinject)
    // Exit interruption = event that CAUSED the VM exit (don't reinject)
    std::uint64_t idt_vectoring_info = 0;
    __vmx_vmread(VMCS_IDT_VECTORING_INFORMATION_FIELD, &idt_vectoring_info);

    // If bit 31 (valid bit) is set, there was a pending interrupt
    if (idt_vectoring_info & 0x80000000)
    {
        // Check if we already have an injection pending
        std::uint64_t entry_int_info = 0;
        __vmx_vmread(VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD, &entry_int_info);

        // Only reinject if no event already queued
        if (!(entry_int_info & 0x80000000))
        {
            // Copy the vectoring information to VM-entry interruption info
            __vmx_vmwrite(VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD, idt_vectoring_info);

            // If it has an error code, copy that too
            if (idt_vectoring_info & 0x800) // Bit 11 = error code valid
            {
                std::uint64_t error_code = 0;
                __vmx_vmread(VMCS_IDT_VECTORING_ERROR_CODE, &error_code);
                __vmx_vmwrite(VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
            }
        }
    }
#endif
}