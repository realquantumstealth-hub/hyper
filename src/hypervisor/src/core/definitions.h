#pragma once

// Page sizes and masks

#define PAGE_SIZE_4KB           0x1000
#define PAGE_SIZE_2MB           0x200000
#define PAGE_SIZE_1GB           0x40000000
#define PAGE_SIZE               0x1000

#define PAGE_MASK_4KB           (~(PAGE_SIZE_4KB - 1))
#define PAGE_MASK_2MB           (~(PAGE_SIZE_2MB - 1))
#define PAGE_MASK_1GB           (~(PAGE_SIZE_1GB - 1))

#define PAGE_SHIFT_4KB          12
#define PAGE_SHIFT_2MB          21
#define PAGE_SHIFT_1GB          30

// PML4/PDPT/PD/PT indices
#define PML4_INDEX_MASK         0x1FF
#define PDPT_INDEX_MASK         0x1FF
#define PD_INDEX_MASK           0x1FF
#define PT_INDEX_MASK           0x1FF

#define PML4_INDEX_SHIFT        39
#define PDPT_INDEX_SHIFT        30
#define PD_INDEX_SHIFT          21
#define PT_INDEX_SHIFT          12

// Table entry counts
#define PML4_ENTRY_COUNT        512
#define PDPT_ENTRY_COUNT        512
#define PD_ENTRY_COUNT          512
#define PT_ENTRY_COUNT          512

// Physical address limits
#define PHYSICAL_ADDRESS_MASK   0x000FFFFFFFFFF000ULL
#define MAXPHYADDR              52

// Memory types
#define MEMORY_TYPE_UC          0x00
#define MEMORY_TYPE_WC          0x01
#define MEMORY_TYPE_WT          0x04
#define MEMORY_TYPE_WP          0x05
#define MEMORY_TYPE_WB          0x06

// CPU vendor detection
#define CPUID_VENDOR_INTEL      0x756E6547
#define CPUID_VENDOR_AMD        0x68747541

// VMX/SVM exit reasons
#define EXIT_REASON_CPUID       10
#define EXIT_REASON_EPT_VIOLATION 48
#define EXIT_REASON_NMI         0

// Stack alignment
#define STACK_ALIGN_SIZE        16
#define STACK_ALIGN_MASK        (~(STACK_ALIGN_SIZE - 1))

// Hypervisor magic values
#define HYPERV_CPUID_INTERFACE  0x40000001
#define HYPERV_CPUID_VERSION    0x40000002

// Shadow stack sizes
#define SHADOW_STACK_SIZE       (PAGE_SIZE_4KB * 4)
#define SHADOW_STACK_GUARD_SIZE PAGE_SIZE_4KB

// Memory protection flags (mmap-style)
#define PROT_READ               0x1
#define PROT_WRITE              0x2
#define PROT_EXEC               0x4

// VMCS field encodings for Intel VT-x
// Note: Intel VT-x only stores system registers in VMCS, not general-purpose registers
#ifdef _INTELMACHINE
#define VMCS_GUEST_RIP          0x0000681E
#define VMCS_GUEST_RSP          0x0000681C
#define VMCS_GUEST_RFLAGS       0x00006820
#define VMCS_GUEST_CR0          0x00006800
#define VMCS_GUEST_CR3          0x00006802
#define VMCS_GUEST_CR4          0x00006804
#endif
