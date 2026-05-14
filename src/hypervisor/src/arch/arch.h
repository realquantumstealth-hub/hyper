#pragma once
#include <ia32-doc/ia32.hpp>
#include <cstdint>

#include "amd_def.h"

// Forward declaration
struct trap_frame_t;

namespace arch
{
	std::uint64_t get_vmexit_reason();
	std::uint8_t is_cpuid(std::uint64_t vmexit_reason);
	std::uint8_t is_slat_violation(std::uint64_t vmexit_reason);
	std::uint8_t is_cr3_write(std::uint64_t vmexit_reason);

	std::uint8_t is_non_maskable_interrupt_exit(std::uint64_t vmexit_reason);

	cr3 get_guest_cr3();
	std::uint64_t get_guest_gs_base();
	std::uint64_t get_guest_cs_selector();
	std::uint64_t get_guest_idtr_base();
	std::uint64_t get_guest_rip();
	void set_guest_rip(std::uint64_t guest_rip);
	void flush_target_process_tlb(cr3 target_cr3, std::uint64_t gva);
	
	// Guest register getters/setters
	std::uint64_t get_guest_rsp();
	void set_guest_rsp(std::uint64_t guest_rsp);
	
#ifdef _INTELMACHINE
	// Intel: GPRs accessed through trap_frame
	std::uint64_t get_guest_rax(trap_frame_t* trap_frame);
	std::uint64_t get_guest_rcx(trap_frame_t* trap_frame);
	std::uint64_t get_guest_rdx(trap_frame_t* trap_frame);
	std::uint64_t get_guest_r8(trap_frame_t* trap_frame);
	std::uint64_t get_guest_r9(trap_frame_t* trap_frame);
	std::uint64_t get_guest_r10(trap_frame_t* trap_frame);
	std::uint64_t get_guest_r11(trap_frame_t* trap_frame);
	
	void set_guest_rax(trap_frame_t* trap_frame, std::uint64_t guest_rax);
	void set_guest_rcx(trap_frame_t* trap_frame, std::uint64_t guest_rcx);
	void set_guest_rdx(trap_frame_t* trap_frame, std::uint64_t guest_rdx);
	void set_guest_r8(trap_frame_t* trap_frame, std::uint64_t guest_r8);
	void set_guest_r9(trap_frame_t* trap_frame, std::uint64_t guest_r9);
	void set_guest_r10(trap_frame_t* trap_frame, std::uint64_t guest_r10);
	void set_guest_r11(trap_frame_t* trap_frame, std::uint64_t guest_r11);
#else
	// AMD: GPRs accessed through VMCB
	std::uint64_t get_guest_rax();
	std::uint64_t get_guest_rcx();
	std::uint64_t get_guest_rdx();
	std::uint64_t get_guest_r8();
	std::uint64_t get_guest_r9();
	std::uint64_t get_guest_r10();
	std::uint64_t get_guest_r11();
	
	void set_guest_rax(std::uint64_t guest_rax);
	void set_guest_rcx(std::uint64_t guest_rcx);
	void set_guest_rdx(std::uint64_t guest_rdx);
	void set_guest_r8(std::uint64_t guest_r8);
	void set_guest_r9(std::uint64_t guest_r9);
	void set_guest_r10(std::uint64_t guest_r10);
	void set_guest_r11(std::uint64_t guest_r11);
#endif
	
	void flush_guest_tlb(std::uint64_t guest_cr3_value);
	std::uint64_t get_vmexit_instruction_length();
	void invlpg(std::uint64_t address);
	void advance_guest_rip();

	// Monitor Trap Flag (MTF) for Intel single-stepping
	void set_monitor_trap_flag(bool enable);
	bool is_mtf_exit(std::uint64_t vmexit_reason);

#ifndef _INTELMACHINE
	vmcb_t* get_vmcb();
	void parse_vmcb_gadget(const std::uint8_t* get_vmcb_gadget);
#endif
}
