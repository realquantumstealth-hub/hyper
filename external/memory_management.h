#pragma once
#include <cstdint>
#include <ia32-doc/ia32.hpp>

// Windows type definitions (replacing ntddk.h dependency)
typedef unsigned char UCHAR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef unsigned long long ULONGLONG;
typedef long LONG;
typedef long long LONGLONG;
typedef void* PVOID;
typedef size_t SIZE_T;
typedef uintptr_t ULONG_PTR;
typedef long NTSTATUS;
typedef char CHAR;
typedef unsigned long long UINT64;

// Status codes
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_FOUND ((NTSTATUS)0xC0000225L)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#define STATUS_PROCEDURE_NOT_FOUND ((NTSTATUS)0xC000007AL)
#define STATUS_INVALID_IMAGE_FORMAT ((NTSTATUS)0xC000007BL)
#define STATUS_PARTIAL_COPY ((NTSTATUS)0x8000000DL)

// Calling convention
#define NTAPI __stdcall

// PE constants
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_DIRECTORY_ENTRY_EXPORT 0

// Memory alignment
#define PAGE_SIZE_4KB 0x1000
#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE_4KB - 1)))

// Windows list structures
typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY* Flink;
    struct _LIST_ENTRY* Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _RTL_BALANCED_NODE {
    union {
        struct _RTL_BALANCED_NODE* Children[2];
        struct {
            struct _RTL_BALANCED_NODE* Left;
            struct _RTL_BALANCED_NODE* Right;
        };
    };
    union {
        UCHAR Red : 1;
        UCHAR Balance : 2;
        ULONG_PTR ParentValue;
    };
} RTL_BALANCED_NODE, *PRTL_BALANCED_NODE;

typedef struct _SINGLE_LIST_ENTRY {
    struct _SINGLE_LIST_ENTRY* Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

// Physical address structure
typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

// Physical memory range structure
typedef struct _PHYSICAL_MEMORY_RANGE {
    PHYSICAL_ADDRESS BaseAddress;
    LARGE_INTEGER NumberOfBytes;
} PHYSICAL_MEMORY_RANGE, *PPHYSICAL_MEMORY_RANGE;

// Windows memory management structures
typedef union _virt_addr_t
{
    void* value;
    struct
    {
        uintptr_t offset : 12;
        uintptr_t pt_index : 9;
        uintptr_t pd_index : 9;
        uintptr_t pdpt_index : 9;
        uintptr_t pml4_index : 9;
        uintptr_t reserved : 16;
    };
} virt_addr_t, * pvirt_addr_t;

typedef struct _MI_ACTIVE_PFN
{
    union
    {
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned __int64 Tradable : 1; /* bit position: 0 */
                /* 0x0000 */ unsigned __int64 NonPagedBuddy : 43; /* bit position: 1 */
            }; /* bitfield */
        } /* size: 0x0008 */ Leaf;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned __int64 Tradable : 1; /* bit position: 0 */
                /* 0x0000 */ unsigned __int64 WsleAge : 3; /* bit position: 1 */
                /* 0x0000 */ unsigned __int64 OldestWsleLeafEntries : 10; /* bit position: 4 */
                /* 0x0000 */ unsigned __int64 OldestWsleLeafAge : 3; /* bit position: 14 */
                /* 0x0000 */ unsigned __int64 NonPagedBuddy : 43; /* bit position: 17 */
            }; /* bitfield */
        } /* size: 0x0008 */ PageTable;
        /* 0x0000 */ unsigned __int64 EntireActiveField;
    }; /* size: 0x0008 */
} MI_ACTIVE_PFN, * PMI_ACTIVE_PFN; /* size: 0x0008 */

typedef struct _MMPTE_HARDWARE
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 Dirty1 : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 Owner : 1; /* bit position: 2 */
        /* 0x0000 */ unsigned __int64 WriteThrough : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned __int64 CacheDisable : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Accessed : 1; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Dirty : 1; /* bit position: 6 */
        /* 0x0000 */ unsigned __int64 LargePage : 1; /* bit position: 7 */
        /* 0x0000 */ unsigned __int64 Global : 1; /* bit position: 8 */
        /* 0x0000 */ unsigned __int64 CopyOnWrite : 1; /* bit position: 9 */
        /* 0x0000 */ unsigned __int64 Unused : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Write : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 PageFrameNumber : 40; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 ReservedForSoftware : 4; /* bit position: 52 */
        /* 0x0000 */ unsigned __int64 WsleAge : 4; /* bit position: 56 */
        /* 0x0000 */ unsigned __int64 WsleProtection : 3; /* bit position: 60 */
        /* 0x0000 */ unsigned __int64 NoExecute : 1; /* bit position: 63 */
    }; /* bitfield */
} MMPTE_HARDWARE, * PMMPTE_HARDWARE; /* size: 0x0008 */

typedef struct _MMPTE_PROTOTYPE
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 DemandFillProto : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 HiberVerifyConverted : 1; /* bit position: 2 */
        /* 0x0000 */ unsigned __int64 ReadOnly : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Combined : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 Unused1 : 4; /* bit position: 12 */
        /* 0x0000 */ __int64 ProtoAddress : 48; /* bit position: 16 */
    }; /* bitfield */
} MMPTE_PROTOTYPE, * PMMPTE_PROTOTYPE; /* size: 0x0008 */

typedef struct _MMPTE_SOFTWARE
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 PageFileReserved : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 PageFileAllocated : 1; /* bit position: 2 */
        /* 0x0000 */ unsigned __int64 ColdPage : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Transition : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 PageFileLow : 4; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 UsedPageTableEntries : 10; /* bit position: 16 */
        /* 0x0000 */ unsigned __int64 ShadowStack : 1; /* bit position: 26 */
        /* 0x0000 */ unsigned __int64 Unused : 5; /* bit position: 27 */
        /* 0x0000 */ unsigned __int64 PageFileHigh : 32; /* bit position: 32 */
    }; /* bitfield */
} MMPTE_SOFTWARE, * PMMPTE_SOFTWARE; /* size: 0x0008 */

typedef struct _MMPTE_TIMESTAMP
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 MustBeZero : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 Unused : 3; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Transition : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 PageFileLow : 4; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 Reserved : 16; /* bit position: 16 */
        /* 0x0000 */ unsigned __int64 GlobalTimeStamp : 32; /* bit position: 32 */
    }; /* bitfield */
} MMPTE_TIMESTAMP, * PMMPTE_TIMESTAMP; /* size: 0x0008 */

typedef struct _MMPTE_TRANSITION
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 Write : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 Spare : 1; /* bit position: 2 */
        /* 0x0000 */ unsigned __int64 IoTracker : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Transition : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 PageFrameNumber : 40; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 Unused : 12; /* bit position: 52 */
    }; /* bitfield */
} MMPTE_TRANSITION, * PMMPTE_TRANSITION; /* size: 0x0008 */

typedef struct _MMPTE_SUBSECTION
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 Unused0 : 3; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 ColdPage : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 Unused1 : 3; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 ExecutePrivilege : 1; /* bit position: 15 */
        /* 0x0000 */ __int64 SubsectionAddress : 48; /* bit position: 16 */
    }; /* bitfield */
} MMPTE_SUBSECTION, * PMMPTE_SUBSECTION; /* size: 0x0008 */

typedef struct _MMPTE_LIST
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned __int64 Valid : 1; /* bit position: 0 */
        /* 0x0000 */ unsigned __int64 OneEntry : 1; /* bit position: 1 */
        /* 0x0000 */ unsigned __int64 filler0 : 2; /* bit position: 2 */
        /* 0x0000 */ unsigned __int64 SwizzleBit : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned __int64 Protection : 5; /* bit position: 5 */
        /* 0x0000 */ unsigned __int64 Prototype : 1; /* bit position: 10 */
        /* 0x0000 */ unsigned __int64 Transition : 1; /* bit position: 11 */
        /* 0x0000 */ unsigned __int64 filler1 : 16; /* bit position: 12 */
        /* 0x0000 */ unsigned __int64 NextEntry : 36; /* bit position: 28 */
    }; /* bitfield */
} MMPTE_LIST, * PMMPTE_LIST; /* size: 0x0008 */

typedef struct _MMPTE
{
    union
    {
        union
        {
            /* 0x0000 */ unsigned __int64 Long;
            /* 0x0000 */ volatile unsigned __int64 VolatileLong;
            /* 0x0000 */ struct _MMPTE_HARDWARE Hard;
            /* 0x0000 */ struct _MMPTE_PROTOTYPE Proto;
            /* 0x0000 */ struct _MMPTE_SOFTWARE Soft;
            /* 0x0000 */ struct _MMPTE_TIMESTAMP TimeStamp;
            /* 0x0000 */ struct _MMPTE_TRANSITION Trans;
            /* 0x0000 */ struct _MMPTE_SUBSECTION Subsect;
            /* 0x0000 */ struct _MMPTE_LIST List;
        }; /* size: 0x0008 */
    } /* size: 0x0008 */ u;
} MMPTE, * PMMPTE; /* size: 0x0008 */

typedef struct _MIPFNBLINK
{
    union
    {
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 Blink : 40; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 NodeBlinkLow : 19; /* bit position: 40 */
            /* 0x0000 */ unsigned __int64 TbFlushStamp : 3; /* bit position: 59 */
            /* 0x0000 */ unsigned __int64 PageBlinkDeleteBit : 1; /* bit position: 62 */
            /* 0x0000 */ unsigned __int64 PageBlinkLockBit : 1; /* bit position: 63 */
        }; /* bitfield */
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 ShareCount : 62; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 PageShareCountDeleteBit : 1; /* bit position: 62 */
            /* 0x0000 */ unsigned __int64 PageShareCountLockBit : 1; /* bit position: 63 */
        }; /* bitfield */
        /* 0x0000 */ unsigned __int64 EntireField;
        /* 0x0000 */ volatile __int64 Lock;
        struct /* bitfield */
        {
            /* 0x0000 */ unsigned __int64 LockNotUsed : 62; /* bit position: 0 */
            /* 0x0000 */ unsigned __int64 DeleteBit : 1; /* bit position: 62 */
            /* 0x0000 */ unsigned __int64 LockBit : 1; /* bit position: 63 */
        }; /* bitfield */
    }; /* size: 0x0008 */
} MIPFNBLINK, * PMIPFNBLINK; /* size: 0x0008 */

typedef struct _MMPFNENTRY1
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned char PageLocation : 3; /* bit position: 0 */
        /* 0x0000 */ unsigned char WriteInProgress : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned char Modified : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned char ReadInProgress : 1; /* bit position: 5 */
        /* 0x0000 */ unsigned char CacheAttribute : 2; /* bit position: 6 */
    }; /* bitfield */
} MMPFNENTRY1, * PMMPFNENTRY1; /* size: 0x0001 */

typedef struct _MMPFNENTRY3
{
    struct /* bitfield */
    {
        /* 0x0000 */ unsigned char Priority : 3; /* bit position: 0 */
        /* 0x0000 */ unsigned char OnProtectedStandby : 1; /* bit position: 3 */
        /* 0x0000 */ unsigned char InPageError : 1; /* bit position: 4 */
        /* 0x0000 */ unsigned char SystemChargedPage : 1; /* bit position: 5 */
        /* 0x0000 */ unsigned char RemovalRequested : 1; /* bit position: 6 */
        /* 0x0000 */ unsigned char ParityError : 1; /* bit position: 7 */
    }; /* bitfield */
} MMPFNENTRY3, * PMMPFNENTRY3; /* size: 0x0001 */

typedef struct _MI_PFN_ULONG5
{
    union
    {
        /* 0x0000 */ unsigned long EntireField;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned long NodeBlinkHigh : 21; /* bit position: 0 */
                /* 0x0000 */ unsigned long NodeFlinkMiddle : 11; /* bit position: 21 */
            }; /* bitfield */
        } /* size: 0x0004 */ StandbyList;
        struct
        {
            /* 0x0000 */ unsigned char ModifiedListBucketIndex : 4; /* bit position: 0 */
        } /* size: 0x0001 */ MappedPageList;
        struct
        {
            struct /* bitfield */
            {
                /* 0x0000 */ unsigned char AnchorLargePageSize : 2; /* bit position: 0 */
                /* 0x0000 */ unsigned char Spare1 : 6; /* bit position: 2 */
            }; /* bitfield */
            /* 0x0001 */ unsigned char ViewCount;
            /* 0x0002 */ unsigned short Spare2;
        } /* size: 0x0004 */ Active;
    }; /* size: 0x0004 */
} MI_PFN_ULONG5, * PMI_PFN_ULONG5; /* size: 0x0004 */

typedef struct _MMPFN
{
    union
    {
        /* 0x0000 */ struct _LIST_ENTRY ListEntry;
        /* 0x0000 */ struct _RTL_BALANCED_NODE TreeNode;
        struct
        {
            union
            {
                union
                {
                    /* 0x0000 */ struct _SINGLE_LIST_ENTRY NextSlistPfn;
                    /* 0x0000 */ void* Next;
                    struct /* bitfield */
                    {
                        /* 0x0000 */ unsigned __int64 Flink : 40; /* bit position: 0 */
                        /* 0x0000 */ unsigned __int64 NodeFlinkLow : 24; /* bit position: 40 */
                    }; /* bitfield */
                    /* 0x0000 */ struct _MI_ACTIVE_PFN Active;
                }; /* size: 0x0008 */
            } /* size: 0x0008 */ u1;
            union
            {
                /* 0x0008 */ struct _MMPTE* PteAddress;
                /* 0x0008 */ unsigned __int64 PteLong;
            }; /* size: 0x0008 */
            /* 0x0010 */ struct _MMPTE OriginalPte;
        }; /* size: 0x0018 */
    }; /* size: 0x0018 */
    /* 0x0018 */ struct _MIPFNBLINK u2;
    union
    {
        union
        {
            struct
            {
                /* 0x0020 */ unsigned short ReferenceCount;
                /* 0x0022 */ struct _MMPFNENTRY1 e1;
                /* 0x0023 */ struct _MMPFNENTRY3 e3;
            }; /* size: 0x0004 */
            struct
            {
                /* 0x0020 */ unsigned short ReferenceCount;
            } /* size: 0x0002 */ e2;
            struct
            {
                /* 0x0020 */ unsigned long EntireField;
            } /* size: 0x0004 */ e4;
        }; /* size: 0x0004 */
    } /* size: 0x0004 */ u3;
    /* 0x0024 */ struct _MI_PFN_ULONG5 u5;
    union
    {
        union
        {
            struct /* bitfield */
            {
                /* 0x0028 */ unsigned __int64 PteFrame : 40; /* bit position: 0 */
                /* 0x0028 */ unsigned __int64 ResidentPage : 1; /* bit position: 40 */
                /* 0x0028 */ unsigned __int64 Unused1 : 1; /* bit position: 41 */
                /* 0x0028 */ unsigned __int64 Unused2 : 1; /* bit position: 42 */
                /* 0x0028 */ unsigned __int64 Partition : 10; /* bit position: 43 */
                /* 0x0028 */ unsigned __int64 FileOnly : 1; /* bit position: 53 */
                /* 0x0028 */ unsigned __int64 PfnExists : 1; /* bit position: 54 */
                /* 0x0028 */ unsigned __int64 NodeFlinkHigh : 5; /* bit position: 55 */
                /* 0x0028 */ unsigned __int64 PageIdentity : 3; /* bit position: 60 */
                /* 0x0028 */ unsigned __int64 PrototypePte : 1; /* bit position: 63 */
            }; /* bitfield */
            /* 0x0028 */ unsigned __int64 EntireField;
        }; /* size: 0x0008 */
    } /* size: 0x0008 */ u4;
} MMPFN, * PMMPFN; /* size: 0x0030 */

// Structure to return detailed PFN information to usermode
typedef struct _HYPERVISOR_PFN_INFO
{
    std::uint64_t physical_address;
    std::uint64_t pfn_index;
    
    // PFN Entry information
    struct {
        std::uint64_t flink;
        std::uint64_t blink;
    } list_entry;
    
    std::uint64_t reference_count;
    std::uint64_t share_count;
    
    // PTE information
    std::uint64_t pte_address;
    std::uint64_t pte_frame;
    
    // Flags from MMPFNENTRY1
    struct {
        std::uint8_t page_location : 3;
        std::uint8_t write_in_progress : 1;
        std::uint8_t modified : 1;
        std::uint8_t read_in_progress : 1;
        std::uint8_t cache_attribute : 2;
    } e1_flags;
    
    // Flags from MMPFNENTRY3  
    struct {
        std::uint8_t priority : 3;
        std::uint8_t on_prototype_pte : 1;
        std::uint8_t inactive_list_head : 1;
        std::uint8_t active_flink : 1;
        std::uint8_t removal_requested : 1;
        std::uint8_t parity_error : 1;
    } e3_flags;
    
    // Color and other info
    std::uint32_t page_color;
    std::uint32_t node_blink;
    
    // Raw data for debugging
    std::uint8_t raw_pfn_data[0x30]; // Full MMPFN structure
    
    // Status information
    NTSTATUS query_status;
    std::uint8_t is_hypervisor_page;
    std::uint8_t is_valid;
    std::uint8_t reserved[6];
    
} HYPERVISOR_PFN_INFO, *PHYPERVISOR_PFN_INFO;

// Structure to return hypervisor memory layout information
typedef struct _HYPERVISOR_MEMORY_INFO
{
    std::uint64_t physical_base;
    std::uint64_t size_bytes;
    std::uint64_t page_count;
    std::uint64_t heap_physical_base;
    std::uint64_t heap_size_bytes;
    std::uint64_t heap_page_count;
    std::uint64_t uefi_boot_base;
    std::uint64_t uefi_boot_size;
    std::uint8_t is_valid;
    std::uint8_t reserved[7];
} HYPERVISOR_MEMORY_INFO, *PHYPERVISOR_MEMORY_INFO;

// Export discovery test results
typedef struct _EXPORT_DISCOVERY_TEST_RESULT
{
    // Status flags
    std::uint8_t ntoskrnl_found;
    std::uint8_t mm_get_virtual_for_physical_found;
    std::uint8_t pattern_found;
    std::uint8_t mmpfn_database_found;

    // Addresses
    std::uint64_t ntoskrnl_base;
    std::uint64_t mm_get_virtual_for_physical_address;
    std::uint64_t pattern_offset;
    std::uint64_t mmpfn_database_address;

    // First 128 bytes of MmGetVirtualForPhysical for inspection
    std::uint8_t mm_get_virtual_bytes[128];

    // NTSTATUS codes
    std::uint64_t ntoskrnl_status;
    std::uint64_t export_status;
    std::uint64_t pattern_status;
    std::uint64_t mmpfn_status;
} EXPORT_DISCOVERY_TEST_RESULT, *PEXPORT_DISCOVERY_TEST_RESULT;

// PE parsing structures
typedef struct _IMAGE_DOS_HEADER {
    WORD   e_magic;
    WORD   e_cblp;
    WORD   e_cp;
    WORD   e_crlc;
    WORD   e_cparhdr;
    WORD   e_minalloc;
    WORD   e_maxalloc;
    WORD   e_ss;
    WORD   e_sp;
    WORD   e_csum;
    WORD   e_ip;
    WORD   e_cs;
    WORD   e_lfarlc;
    WORD   e_ovno;
    WORD   e_res[4];
    WORD   e_oemid;
    WORD   e_oeminfo;
    WORD   e_res2[10];
    LONG   e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD   VirtualAddress;
    DWORD   Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;
    WORD    NumberOfSections;
    DWORD   TimeDateStamp;
    DWORD   PointerToSymbolTable;
    DWORD   NumberOfSymbols;
    WORD    SizeOfOptionalHeader;
    WORD    Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    WORD        Magic;
    BYTE        MajorLinkerVersion;
    BYTE        MinorLinkerVersion;
    DWORD       SizeOfCode;
    DWORD       SizeOfInitializedData;
    DWORD       SizeOfUninitializedData;
    DWORD       AddressOfEntryPoint;
    DWORD       BaseOfCode;
    ULONGLONG   ImageBase;
    DWORD       SectionAlignment;
    DWORD       FileAlignment;
    WORD        MajorOperatingSystemVersion;
    WORD        MinorOperatingSystemVersion;
    WORD        MajorImageVersion;
    WORD        MinorImageVersion;
    WORD        MajorSubsystemVersion;
    WORD        MinorSubsystemVersion;
    DWORD       Win32VersionValue;
    DWORD       SizeOfImage;
    DWORD       SizeOfHeaders;
    DWORD       CheckSum;
    WORD        Subsystem;
    WORD        DllCharacteristics;
    ULONGLONG   SizeOfStackReserve;
    ULONGLONG   SizeOfStackCommit;
    ULONGLONG   SizeOfHeapReserve;
    ULONGLONG   SizeOfHeapCommit;
    DWORD       LoaderFlags;
    DWORD       NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;

// Alias for compatibility
typedef IMAGE_NT_HEADERS64 IMAGE_NT_HEADERS;
typedef PIMAGE_NT_HEADERS64 PIMAGE_NT_HEADERS;

typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD   Characteristics;
    DWORD   TimeDateStamp;
    WORD    MajorVersion;
    WORD    MinorVersion;
    DWORD   Name;
    DWORD   Base;
    DWORD   NumberOfFunctions;
    DWORD   NumberOfNames;
    DWORD   AddressOfFunctions;
    DWORD   AddressOfNames;
    DWORD   AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    union {
        DWORD   Characteristics;
        DWORD   OriginalFirstThunk;
    } DUMMYUNIONNAME;
    DWORD   TimeDateStamp;
    DWORD   ForwarderChain;
    DWORD   Name;
    DWORD   FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR, *PIMAGE_IMPORT_DESCRIPTOR;

typedef struct _IMAGE_THUNK_DATA64 {
    union {
        ULONGLONG ForwarderString;
        ULONGLONG Function;
        ULONGLONG Ordinal;
        ULONGLONG AddressOfData;
    } u1;
} IMAGE_THUNK_DATA64, *PIMAGE_THUNK_DATA64;

typedef struct _IMAGE_IMPORT_BY_NAME {
    WORD    Hint;
    CHAR    Name[1];
} IMAGE_IMPORT_BY_NAME, *PIMAGE_IMPORT_BY_NAME;

// Forward declarations for Windows kernel functions
typedef PPHYSICAL_MEMORY_RANGE (NTAPI *MmGetPhysicalMemoryRanges_t)();
typedef PVOID (NTAPI *MmGetVirtualForPhysical_t)(PHYSICAL_ADDRESS PhysicalAddress);

// Structure for detailed page information
typedef struct _page_info_t {
    std::uint64_t physical_address;
    std::uint64_t pfn_number;
    std::uint64_t pte_address;
    bool has_pte_address;
    
    // From PFN entry
    unsigned short reference_count;
    unsigned char page_location;
    unsigned char write_in_progress : 1;
    unsigned char modified : 1;
    unsigned char read_in_progress : 1;
    unsigned char cache_attribute : 2;
    
    std::uint64_t pte_frame : 40;
    unsigned char resident_page : 1;
    unsigned char pfn_exists : 1;
    unsigned char file_only : 1;
    unsigned char prototype_pte : 1;
    
    const char* location_name;
} page_info_t;

// Structure for memory statistics
typedef struct _memory_statistics_t {
    std::uint64_t total_pages;
    std::uint64_t free_pages;
    std::uint64_t zeroed_pages;
    std::uint64_t standby_pages;
    std::uint64_t modified_pages;
    std::uint64_t modified_no_write_pages;
    std::uint64_t bad_pages;
    std::uint64_t active_pages;
    std::uint64_t transition_pages;
    std::uint64_t referenced_pages;
    std::uint64_t dirty_pages;
    std::uint64_t sample_size;
} memory_statistics_t;

// Callback function type for page enumeration
typedef bool (*page_enumeration_callback_t)(const page_info_t* page_info, void* context);

namespace memory_management
{
    // Utility function for string comparison (since crt::compare_strings doesn't exist)
    inline int compare_strings(const char* str1, const char* str2)
    {
        if (!str1 || !str2)
            return str1 == str2 ? 0 : (str1 ? 1 : -1);
        
        while (*str1 && *str2 && *str1 == *str2) {
            str1++;
            str2++;
        }
        return *str1 - *str2;
    }
    // Global cached values
    extern PVOID g_mmonp_MmPfnDatabase;
    extern MmGetVirtualForPhysical_t g_MmGetVirtualForPhysical;
    extern MmGetPhysicalMemoryRanges_t g_MmGetPhysicalMemoryRanges;
    extern bool g_initialized;
    extern cr3 g_kernel_cr3;

    // PE parsing functions
    PVOID find_pattern_in_memory(PVOID search_base, SIZE_T search_size, const void* pattern, SIZE_T pattern_size);
    PVOID get_module_base(const char* module_name);
    PVOID get_procedure_address(PVOID module_base, const char* procedure_name);
    PVOID parse_imports_and_find_function(PVOID module_base, const char* function_name);

    // MmPfnDatabase and kernel function initialization
    NTSTATUS initialize_mmpfn_database(PVOID ntoskrnl_base);
    NTSTATUS initialize_kernel_functions(PVOID ntoskrnl_base);
    
    // Auto-initialize using captured ntoskrnl base address from boot
    NTSTATUS auto_initialize_kernel_functions();
    
    // Physical memory reading
    NTSTATUS read_physical_memory(PVOID physical_address, PVOID buffer, SIZE_T size, SIZE_T* bytes_read);
    
    // New CR3 function (ntoskrnl_base parameter is now optional - uses captured base if nullptr)
    std::uint64_t dirbase_from_base_address(void* base, PVOID ntoskrnl_base = nullptr);
    
    // Helper functions for CR3 detection
    bool test_virtual_address_in_cr3(void* virtual_address, std::uint64_t test_cr3, cr3 slat_cr3);
    std::uint64_t find_cr3_by_page_table_walk(void* virtual_address, cr3 guest_cr3, cr3 slat_cr3);
    
    // MmPfnDatabase access functions
    NTSTATUS get_pfn_information(std::uint64_t physical_address, _MMPFN* pfn_info_out);
    NTSTATUS get_page_information(std::uint64_t physical_address, page_info_t* page_info_out);
    NTSTATUS enumerate_physical_pages(std::uint64_t start_physical_address, 
                                    std::uint64_t end_physical_address, 
                                    page_enumeration_callback_t callback, 
                                    void* context);
    bool is_pfn_valid(std::uint64_t physical_address);
    NTSTATUS get_memory_statistics(memory_statistics_t* stats_out);
    
    // MmPfnDatabase manipulation functions for stealth
    NTSTATUS find_mmpfn_database_address(PVOID ntoskrnl_base, PVOID* mmpfn_database_out);
    NTSTATUS unlink_hypervisor_memory_from_pfn_database(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes);
    NTSTATUS hide_physical_page_from_pfn_database(std::uint64_t physical_address);
    NTSTATUS restore_physical_page_in_pfn_database(std::uint64_t physical_address);
    
    // Hypervisor PFN query functions
    NTSTATUS query_hypervisor_pfn_detailed(std::uint64_t physical_address, HYPERVISOR_PFN_INFO* pfn_info_out);
    NTSTATUS enumerate_hypervisor_pfns(std::uint64_t hypervisor_base_physical, std::uint64_t hypervisor_size_bytes, 
                                      HYPERVISOR_PFN_INFO* pfn_info_array, std::uint32_t max_entries, std::uint32_t* entries_filled);
    
    // Get hypervisor memory layout information
    NTSTATUS get_hypervisor_memory_info(HYPERVISOR_MEMORY_INFO* memory_info_out);
}
