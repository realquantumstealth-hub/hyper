#pragma once

#ifndef _INTELMACHINE

#include <cstdint>
#include <ia32-doc/ia32.hpp>

#pragma warning(push)
#pragma warning(disable: 4201)

enum class tlb_control_t : std::uint8_t
{
	do_not_flush = 0,
	flush_entire_tlb = 1, // should only be used on legacy hardware
	flush_guest_tlb_entries = 3,
	flush_guest_non_global_tlb_entries = 7,
};

union vmcb_clean_t
{
	std::uint32_t flags;

	struct
	{
		std::uint32_t i : 1;
		std::uint32_t iopm : 1;
		std::uint32_t asid : 1;
		std::uint32_t tpr : 1;
		std::uint32_t nested_paging : 1;
		std::uint32_t cr : 1;
		std::uint32_t dr : 1;
		std::uint32_t dt : 1;
		std::uint32_t seg : 1;
		std::uint32_t cr2 : 1;
		std::uint32_t lbr : 1;
		std::uint32_t avic : 1;
		std::uint32_t cet : 1;
		std::uint32_t reserved : 19;
	};
};

enum class event_injection_type : std::uint32_t
{
	interrupt = 0,
	nmi = 2,
	exception = 3,
	software_interrupt = 4
};

union event_injection_t
{
	std::uint64_t flags;
	struct
	{
		std::uint64_t vector : 8;
		event_injection_type type : 3;
		std::uint64_t error_code_valid : 1;
		std::uint64_t reserved_one : 19;
		std::uint64_t valid : 1;
	};
};

struct vmcb_control_area_t
{
	std::uint16_t intercept_cr_read;      // 0x000 - CR read intercepts
	std::uint16_t intercept_cr_write;     // 0x002 - CR write intercepts
	std::uint16_t intercept_dr_read;      // 0x004 - DR read intercepts
	std::uint16_t intercept_dr_write;     // 0x006 - DR write intercepts
	std::uint32_t intercept_exceptions;   // 0x008 - Exception intercepts
	std::uint32_t intercept_misc_vector_3; // 0x00C - Misc intercepts
	std::uint8_t pad_one[0x4C];
	tlb_control_t tlb_control;
	std::uint8_t pad_three[0x13];
	std::uint64_t vmexit_reason;
	std::uint64_t first_exit_info;
	std::uint64_t second_exit_info;
	std::uint8_t pad_four[0x8];
	event_injection_t event_injection;
	std::uint32_t event_injection_error_code;
	std::uint8_t pad_five_a[0x4];
	cr3 nested_cr3;
	std::uint8_t pad_five_b[0x8];
	vmcb_clean_t clean;
	std::uint8_t pad_six[0x4];
	std::uint64_t next_rip;
	std::uint8_t pad_seven[0x330];
};

struct vmcb_segment_t
{
	std::uint16_t selector;
	std::uint16_t attrib;
	std::uint32_t limit;
	std::uint64_t base;
};

struct vmcb_state_save_t
{
	vmcb_segment_t es;      // 0x000
	vmcb_segment_t cs;      // 0x010
	vmcb_segment_t ss;      // 0x020
	vmcb_segment_t ds;      // 0x030
	vmcb_segment_t fs;      // 0x040
	vmcb_segment_t gs;      // 0x050
	vmcb_segment_t gdtr;    // 0x060
	vmcb_segment_t ldtr;    // 0x070
	vmcb_segment_t idtr;    // 0x080
	vmcb_segment_t tr;      // 0x090
	std::uint8_t pad_one[0x10]; // Padding to CR3 at 0x0B0
	std::uint64_t cr3;
	std::uint8_t pad_two[0x10];
	std::uint64_t kernel_gs_base;
	std::uint8_t pad_four[0x10];
	std::uint64_t rip;
	std::uint8_t pad_five[0x58];
	std::uint64_t rsp;
	std::uint8_t pad_six[0x18];
	std::uint64_t rax;
	std::uint64_t rcx;
	std::uint64_t rdx;
	std::uint64_t rbx;
	std::uint8_t pad_seven[0x8];
	std::uint64_t rbp;
	std::uint64_t rsi;
	std::uint64_t rdi;
	std::uint64_t r8;
	std::uint64_t r9;
	std::uint64_t r10;
	std::uint64_t r11;
	std::uint64_t r12;
	std::uint64_t r13;
	std::uint64_t r14;
	std::uint64_t r15;
};

struct vmcb_t
{
	vmcb_control_area_t control;
	vmcb_state_save_t save_state;
};

union npf_exit_info_1
{
	std::uint64_t flags;

	struct
	{
		std::uint64_t present : 1;
		std::uint64_t write_access : 1;
		std::uint64_t user_access : 1;
		std::uint64_t npte_reserved_set : 1;
		std::uint64_t execute_access : 1;
		std::uint64_t reserved_one : 1;
		std::uint64_t shadow_stack_access : 1;
		std::uint64_t reserved_two : 25;
		std::uint64_t final_gpa_translation : 1;
		std::uint64_t gpt_translation : 1;
		std::uint64_t reserved_three : 3;
		std::uint64_t supervisor_shadow_stack : 1;
		std::uint64_t reserved_four : 26;
	};
};

#pragma warning(pop)

#define SVM_EXIT_REASON_WRITE_CR3 0x13
#define SVM_EXIT_REASON_PHYSICAL_NMI 0x61
#define SVM_EXIT_REASON_CPUID 0x72
#define SVM_EXIT_REASON_NPF 0x400

#endif