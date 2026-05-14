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

enum class event_injection_type : std::uint8_t
{
	external_interrupt = 0,
	reserved = 1,
	nmi = 2,
	exception = 3,
	software_interrupt = 4
};

union svm_event_injection_t
{
	std::uint32_t flags;
	struct
	{
		std::uint32_t vector : 8;
		std::uint32_t type : 3;
		std::uint32_t error_code_valid : 1;
		std::uint32_t reserved : 19;
		std::uint32_t valid : 1;
	};
};

struct vmcb_control_area_t
{
	// Intercept vectors (0x00-0x13)
	std::uint8_t pad_one[0xC];              // 0x00-0x0B
	std::uint32_t intercept_misc_vector_3;  // 0x0C-0x0F
	std::uint8_t pad_two[0x48];             // 0x10-0x57

	// ASID and TLB control (0x58-0x6F)
	std::uint32_t asid;                     // 0x58-0x5B
	tlb_control_t tlb_control;              // 0x5C
	std::uint8_t pad_three[0x13];           // 0x5D-0x6F

	// Exit information (0x70-0x87)
	std::uint64_t vmexit_reason;            // 0x70-0x77
	std::uint64_t first_exit_info;          // 0x78-0x7F
	std::uint64_t second_exit_info;         // 0x80-0x87

	// Event Injection Fields (from AMD64 Architecture Programmer's Manual)
	// EXITINTINFO: Read-only field describing an interrupt that was pending at the time of a VMEXIT.
	// This is what the hypervisor must check to avoid losing interrupts.
	svm_event_injection_t exit_int_info;    // 0x88-0x8B (EXITINTINFO)
	std::uint32_t exit_int_info_error_code; // 0x8C-0x8F
	std::uint64_t nested_ctl;               // 0x90-0x97
	std::uint8_t pad_four[0x10];            // 0x98-0xA7 (16 bytes)

	// EVENTINJ: Writable field used by the hypervisor to inject an event (interrupt or exception)
	// into the guest upon the next VM-entry. This is the primary mechanism for virtualizing
	// interrupts.
	svm_event_injection_t event_injection;  // 0xA8-0xAB (EVENTINJ)
	std::uint32_t event_injection_error_code; // 0xAC-0xAF

	// Nested paging and clean bits (0xB0-0xCF)
	cr3 nested_cr3;                         // 0xB0-0xB7
	std::uint64_t lbr_ctl;                  // 0xB8-0xBF
	vmcb_clean_t clean;                     // 0xC0-0xC3
	std::uint8_t pad_five[0x4];             // 0xC4-0xC7
	std::uint64_t next_rip;                 // 0xC8-0xCF

	// Remaining padding to 0x400
	std::uint8_t pad_six[0x330];            // 0xD0-0x3FF (816 bytes)
};

struct vmcb_seg {
	uint16_t selector;
	uint16_t attrib;
	uint32_t limit;
	uint64_t base;
};

struct vmcb_state_save_t
{
	vmcb_seg es;      // 0x000
	vmcb_seg cs;      // 0x010
	vmcb_seg ss;      // 0x020
	vmcb_seg ds;      // 0x030
	vmcb_seg fs;      // 0x040
	vmcb_seg gs;      // 0x050
	vmcb_seg gdtr;    // 0x060
	vmcb_seg ldtr;    // 0x070
	vmcb_seg idtr;    // 0x080
	vmcb_seg tr;      // 0x090
	std::uint8_t pad_one[0xB0];  // 0x0A0 to 0x14F
	std::uint64_t cr3;           // 0x150
	std::uint8_t pad_five[0x20];
	std::uint64_t rip;           // 0x178
	std::uint8_t pad_six[0x58];
	std::uint64_t rsp;           // 0x1D8
	std::uint8_t pad_seven[0x18];
	std::uint64_t rax;           // 0x1F8
	std::uint8_t pad_eight[0x200];  // 0x200 to 0x3FF - pad to full 0x400 bytes
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
