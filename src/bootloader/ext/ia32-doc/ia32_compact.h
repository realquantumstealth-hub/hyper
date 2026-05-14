#pragma once

typedef unsigned __int64    UINT64;
typedef __int64             INT64;
typedef unsigned __int32    UINT32;
typedef __int32             INT32;
typedef unsigned short      UINT16;
typedef unsigned short      CHAR16;
typedef short               INT16;
typedef unsigned char       BOOLEAN;
typedef unsigned char       UINT8;
typedef char                CHAR8;
typedef signed char         INT8;

#if defined(_MSC_EXTENSIONS)
#pragma warning(push)
#pragma warning(disable: 4201)
#endif

/**
 * @defgroup intel_manual \
 *           Intel Manual
 * @{
 */
 /**
  * @defgroup control_registers \
  *           Control registers
  * @{
  */
typedef union {
    struct {
        UINT64 protection_enable : 1;
        UINT64 monitor_coprocessor : 1;
        UINT64 emulate_fpu : 1;
        UINT64 task_switched : 1;
        UINT64 extensionype : 1;
        UINT64 numeric_error : 1;
        UINT64 reserved_1 : 10;
        UINT64 write_protect : 1;
        UINT64 reserved_2 : 1;
        UINT64 alignment_mask : 1;
        UINT64 reserved_3 : 10;
        UINT64 not_writehrough : 1;
        UINT64 cache_disable : 1;
        UINT64 paging_enable : 1;
    };

    UINT64 flags;
} cr0;

typedef union {
    struct {
        UINT64 reserved_1 : 3;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 reserved_2 : 7;
        UINT64 address_of_page_directory : 36;
    };

    UINT64 flags;
} cr3;

typedef union {
    struct {
        UINT64 virtual_mode_extensions : 1;
        UINT64 protected_mode_virtual_interrupts : 1;
        UINT64 timestamp_disable : 1;
        UINT64 debugging_extensions : 1;
        UINT64 page_size_extensions : 1;
        UINT64 physical_address_extension : 1;
        UINT64 machine_check_enable : 1;
        UINT64 page_global_enable : 1;
        UINT64 performance_monitoring_counter_enable : 1;
        UINT64 os_fxsave_fxrstor_support : 1;
        UINT64 os_xmm_exception_support : 1;
        UINT64 usermode_instruction_prevention : 1;
        UINT64 linear_addresses_57_bit : 1;
        UINT64 vmx_enable : 1;
        UINT64 smx_enable : 1;
        UINT64 reserved_1 : 1;
        UINT64 fsgsbase_enable : 1;
        UINT64 pcid_enable : 1;
        UINT64 os_xsave : 1;
        UINT64 key_locker_enable : 1;
        UINT64 smep_enable : 1;
        UINT64 smap_enable : 1;
        UINT64 protection_key_enable : 1;
        UINT64 control_flow_enforcement_enable : 1;
        UINT64 protection_key_for_supervisor_mode_enable : 1;
    };

    UINT64 flags;
} cr4;

typedef union {
    struct {
        UINT64 task_priority_level : 4;
        UINT64 reserved : 60;
    };

    UINT64 flags;
} cr8;

/**
 * @}
 */

 /**
  * @defgroup debug_registers \
  *           Debug registers
  * @{
  */
typedef union {
    struct {
        UINT64 breakpoint_condition : 4;
        UINT64 reserved_1 : 9;
        UINT64 debug_register_access_detected : 1;
        UINT64 single_instruction : 1;
        UINT64 task_switch : 1;
        UINT64 restrictedransactional_memory : 1;
    };

    UINT64 flags;
} dr6;

typedef union {
    struct {
        UINT64 local_breakpoint_0 : 1;
        UINT64 global_breakpoint_0 : 1;
        UINT64 local_breakpoint_1 : 1;
        UINT64 global_breakpoint_1 : 1;
        UINT64 local_breakpoint_2 : 1;
        UINT64 global_breakpoint_2 : 1;
        UINT64 local_breakpoint_3 : 1;
        UINT64 global_breakpoint_3 : 1;
        UINT64 local_exact_breakpoint : 1;
        UINT64 global_exact_breakpoint : 1;
        UINT64 reserved_1 : 1;
        UINT64 restrictedransactional_memory : 1;
        UINT64 reserved_2 : 1;
        UINT64 general_detect : 1;
        UINT64 reserved_3 : 2;
        UINT64 read_write_0 : 2;
        UINT64 length_0 : 2;
        UINT64 read_write_1 : 2;
        UINT64 length_1 : 2;
        UINT64 read_write_2 : 2;
        UINT64 length_2 : 2;
        UINT64 read_write_3 : 2;
        UINT64 length_3 : 2;
    };

    UINT64 flags;
} dr7;

/**
 * @}
 */

 /**
  * @defgroup cpuid \
  *           CPUID
  * @{
  */
#define CPUID_SIGNATURE                                              0x00000000
typedef struct {
    UINT32 max_cpuid_input_value;
    UINT32 ebx_value_genu;
    UINT32 ecx_value_ntel;
    UINT32 edx_value_inei;
} cpuid_eax_00;

#define CPUID_VERSION_INFO                                           0x00000001
typedef struct {
    union {
        struct {
            UINT32 stepping_id : 4;
            UINT32 model : 4;
            UINT32 family_id : 4;
            UINT32 processorype : 2;
            UINT32 reserved_1 : 2;
            UINT32 extended_model_id : 4;
            UINT32 extended_family_id : 8;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 brand_index : 8;
            UINT32 clflush_line_size : 8;
            UINT32 max_addressable_ids : 8;
            UINT32 initial_apic_id : 8;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 streaming_simd_extensions_3 : 1;
            UINT32 pclmulqdq_instruction : 1;
            UINT32 ds_area_64bit_layout : 1;
            UINT32 monitor_mwait_instruction : 1;
            UINT32 cpl_qualified_debug_store : 1;
            UINT32 virtual_machine_extensions : 1;
            UINT32 safer_mode_extensions : 1;
            UINT32 enhanced_intel_speedstepechnology : 1;
            UINT32 thermal_monitor_2 : 1;
            UINT32 supplemental_streaming_simd_extensions_3 : 1;
            UINT32 l1_context_id : 1;
            UINT32 silicon_debug : 1;
            UINT32 fma_extensions : 1;
            UINT32 cmpxchg16b_instruction : 1;
            UINT32 xtpr_update_control : 1;
            UINT32 perfmon_and_debug_capability : 1;
            UINT32 reserved_1 : 1;
            UINT32 process_context_identifiers : 1;
            UINT32 direct_cache_access : 1;
            UINT32 sse41_support : 1;
            UINT32 sse42_support : 1;
            UINT32 x2apic_support : 1;
            UINT32 movbe_instruction : 1;
            UINT32 popcnt_instruction : 1;
            UINT32 tsc_deadline : 1;
            UINT32 aesni_instruction_extensions : 1;
            UINT32 xsave_xrstor_instruction : 1;
            UINT32 osx_save : 1;
            UINT32 avx_support : 1;
            UINT32 half_precision_conversion_instructions : 1;
            UINT32 rdrand_instruction : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 floating_point_unit_on_chip : 1;
            UINT32 virtual_8086_mode_enhancements : 1;
            UINT32 debugging_extensions : 1;
            UINT32 page_size_extension : 1;
            UINT32 timestamp_counter : 1;
            UINT32 rdmsr_wrmsr_instructions : 1;
            UINT32 physical_address_extension : 1;
            UINT32 machine_check_exception : 1;
            UINT32 cmpxchg8b_instruction : 1;
            UINT32 apic_on_chip : 1;
            UINT32 reserved_1 : 1;
            UINT32 sysenter_sysexit_instructions : 1;
            UINT32 memoryype_range_registers : 1;
            UINT32 page_global_bit : 1;
            UINT32 machine_check_architecture : 1;
            UINT32 conditional_move_instructions : 1;
            UINT32 page_attributeable : 1;
            UINT32 page_size_extension_36bit : 1;
            UINT32 processor_serial_number : 1;
            UINT32 clflush_instruction : 1;
            UINT32 reserved_2 : 1;
            UINT32 debug_store : 1;
            UINT32 thermal_control_msrs_for_acpi : 1;
            UINT32 mmx_support : 1;
            UINT32 fxsave_fxrstor_instructions : 1;
            UINT32 sse_support : 1;
            UINT32 sse2_support : 1;
            UINT32 self_snoop : 1;
            UINT32 hyperhreadingechnology : 1;
            UINT32 thermal_monitor : 1;
            UINT32 reserved_3 : 1;
            UINT32 pending_break_enable : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_01;

#define CPUID_CACHE_PARAMS                                           0x00000004
typedef struct {
    union {
        struct {
            UINT32 cacheype_field : 5;
            UINT32 cache_level : 3;
            UINT32 self_initializing_cache_level : 1;
            UINT32 fully_associative_cache : 1;
            UINT32 reserved_1 : 4;
            UINT32 max_addressable_ids_for_logical_processors_sharinghis_cache : 12;
            UINT32 max_addressable_ids_for_processor_cores_in_physical_package : 6;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 system_coherency_line_size : 12;
            UINT32 physical_line_partitions : 10;
            UINT32 ways_of_associativity : 10;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 number_of_sets : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 write_back_invalidate : 1;
            UINT32 cache_inclusiveness : 1;
            UINT32 complex_cache_indexing : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_04;

#define CPUID_MONITOR_MWAIT                                          0x00000005
typedef struct {
    union {
        struct {
            UINT32 smallest_monitor_line_size : 16;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 largest_monitor_line_size : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 enumeration_of_monitor_mwait_extensions : 1;
            UINT32 supportsreating_interrupts_as_break_event_for_mwait : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 number_of_c0_sub_c_states : 4;
            UINT32 number_of_c1_sub_c_states : 4;
            UINT32 number_of_c2_sub_c_states : 4;
            UINT32 number_of_c3_sub_c_states : 4;
            UINT32 number_of_c4_sub_c_states : 4;
            UINT32 number_of_c5_sub_c_states : 4;
            UINT32 number_of_c6_sub_c_states : 4;
            UINT32 number_of_c7_sub_c_states : 4;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_05;

#define CPUIDHERMAL_POWER_MANAGEMENT                               0x00000006
typedef struct {
    union {
        struct {
            UINT32 temperature_sensor_supported : 1;
            UINT32 intelurbo_boostechnology_available : 1;
            UINT32 apicimer_always_running : 1;
            UINT32 reserved_1 : 1;
            UINT32 power_limit_notification : 1;
            UINT32 clock_modulation_duty : 1;
            UINT32 packagehermal_management : 1;
            UINT32 hwp_base_registers : 1;
            UINT32 hwp_notification : 1;
            UINT32 hwp_activity_window : 1;
            UINT32 hwp_energy_performance_preference : 1;
            UINT32 hwp_package_level_request : 1;
            UINT32 reserved_2 : 1;
            UINT32 hdc : 1;
            UINT32 intelurbo_boost_maxechnology_3_available : 1;
            UINT32 hwp_capabilities : 1;
            UINT32 hwp_peci_override : 1;
            UINT32 flexible_hwp : 1;
            UINT32 fast_access_mode_for_hwp_request_msr : 1;
            UINT32 reserved_3 : 1;
            UINT32 ignoring_idle_logical_processor_hwp_request : 1;
            UINT32 reserved_4 : 2;
            UINT32 intelhread_director : 1;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 number_of_interrupthresholds_inhermal_sensor : 4;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 hardware_coordination_feedback_capability : 1;
            UINT32 reserved_1 : 2;
            UINT32 number_of_intelhread_director_classes : 1;
            UINT32 reserved_2 : 4;
            UINT32 performance_energy_bias_preference : 8;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_06;

#define CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS                      0x00000007
typedef struct {
    union {
        struct {
            UINT32 number_of_sub_leaves : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 fsgsbase : 1;
            UINT32 ia32sc_adjust_msr : 1;
            UINT32 sgx : 1;
            UINT32 bmi1 : 1;
            UINT32 hle : 1;
            UINT32 avx2 : 1;
            UINT32 fdp_excptn_only : 1;
            UINT32 smep : 1;
            UINT32 bmi2 : 1;
            UINT32 enhanced_rep_movsb_stosb : 1;
            UINT32 invpcid : 1;
            UINT32 rtm : 1;
            UINT32 rdt_m : 1;
            UINT32 deprecates : 1;
            UINT32 mpx : 1;
            UINT32 rdt : 1;
            UINT32 avx512f : 1;
            UINT32 avx512dq : 1;
            UINT32 rdseed : 1;
            UINT32 adx : 1;
            UINT32 smap : 1;
            UINT32 avx512_ifma : 1;
            UINT32 reserved_1 : 1;
            UINT32 clflushopt : 1;
            UINT32 clwb : 1;
            UINT32 intel : 1;
            UINT32 avx512pf : 1;
            UINT32 avx512er : 1;
            UINT32 avx512cd : 1;
            UINT32 sha : 1;
            UINT32 avx512bw : 1;
            UINT32 avx512vl : 1;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 prefetchwt1 : 1;
            UINT32 avx512_vbmi : 1;
            UINT32 umip : 1;
            UINT32 pku : 1;
            UINT32 ospke : 1;
            UINT32 waitpkg : 1;
            UINT32 avx512_vbmi2 : 1;
            UINT32 cet_ss : 1;
            UINT32 gfni : 1;
            UINT32 vaes : 1;
            UINT32 vpclmulqdq : 1;
            UINT32 avx512_vnni : 1;
            UINT32 avx512_bitalg : 1;
            UINT32 tme_en : 1;
            UINT32 avx512_vpopcntdq : 1;
            UINT32 reserved_1 : 1;
            UINT32 la57 : 1;
            UINT32 mawau : 5;
            UINT32 rdpid : 1;
            UINT32 kl : 1;
            UINT32 reserved_2 : 1;
            UINT32 cldemote : 1;
            UINT32 reserved_3 : 1;
            UINT32 movdiri : 1;
            UINT32 movdir64b : 1;
            UINT32 reserved_4 : 1;
            UINT32 sgx_lc : 1;
            UINT32 pks : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved_1 : 2;
            UINT32 avx512_4vnniw : 1;
            UINT32 avx512_4fmaps : 1;
            UINT32 fast_short_rep_mov : 1;
            UINT32 reserved_2 : 3;
            UINT32 avx512_vp2intersect : 1;
            UINT32 reserved_3 : 1;
            UINT32 md_clear : 1;
            UINT32 reserved_4 : 3;
            UINT32 serialize : 1;
            UINT32 hybrid : 1;
            UINT32 reserved_5 : 2;
            UINT32 pconfig : 1;
            UINT32 reserved_6 : 1;
            UINT32 cet_ibt : 1;
            UINT32 reserved_7 : 5;
            UINT32 ibrs_ibpb : 1;
            UINT32 stibp : 1;
            UINT32 l1d_flush : 1;
            UINT32 ia32_arch_capabilities : 1;
            UINT32 ia32_core_capabilities : 1;
            UINT32 ssbd : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_07;

#define CPUID_DIRECT_CACHE_ACCESS_INFO                               0x00000009
typedef struct {
    union {
        struct {
            UINT32 ia32_platform_dca_cap : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_09;

#define CPUID_ARCHITECTURAL_PERFORMANCE_MONITORING                   0x0000000A
typedef struct {
    union {
        struct {
            UINT32 version_id_of_architectural_performance_monitoring : 8;
            UINT32 number_of_performance_monitoring_counter_per_logical_processor : 8;
            UINT32 bit_width_of_performance_monitoring_counter : 8;
            UINT32 ebx_bit_vector_length : 8;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 core_cycle_event_not_available : 1;
            UINT32 instruction_retired_event_not_available : 1;
            UINT32 reference_cycles_event_not_available : 1;
            UINT32 last_level_cache_reference_event_not_available : 1;
            UINT32 last_level_cache_misses_event_not_available : 1;
            UINT32 branch_instruction_retired_event_not_available : 1;
            UINT32 branch_mispredict_retired_event_not_available : 1;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 number_of_fixed_function_performance_counters : 5;
            UINT32 bit_width_of_fixed_function_performance_counters : 8;
            UINT32 reserved_1 : 2;
            UINT32 anyhread_deprecation : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0a;

#define CPUID_EXTENDEDOPOLOGY                                      0x0000000B
typedef struct {
    union {
        struct {
            UINT32 x2apic_ido_uniqueopology_id_shift : 5;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 number_of_logical_processors_athis_levelype : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 level_number : 8;
            UINT32 levelype : 8;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 x2apic_id : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0b;

/**
 * @defgroup cpuid_eax_0d \
 *           EAX = 0x0D
 * @{
 */
#define CPUID_EXTENDED_STATE                                         0x0000000D
typedef struct {
    union {
        struct {
            UINT32 x87_state : 1;
            UINT32 sse_state : 1;
            UINT32 avx_state : 1;
            UINT32 mpx_state : 2;
            UINT32 avx_512_state : 3;
            UINT32 used_for_ia32_xss_1 : 1;
            UINT32 pkru_state : 1;
            UINT32 reserved_1 : 3;
            UINT32 used_for_ia32_xss_2 : 1;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 max_size_required_by_enabled_features_in_xcr0 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 max_size_of_xsave_xrstor_save_area : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 xcr0_supported_bits : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0d_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 reserved_1 : 1;
            UINT32 supports_xsavec_and_compacted_xrstor : 1;
            UINT32 supports_xgetbv_with_ecx_1 : 1;
            UINT32 supports_xsave_xrstor_and_ia32_xss : 1;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 size_of_xsave_aread : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 used_for_xcr0_1 : 8;
            UINT32 pt_state : 1;
            UINT32 used_for_xcr0_2 : 1;
            UINT32 reserved_1 : 1;
            UINT32 cet_user_state : 1;
            UINT32 cet_supervisor_state : 1;
            UINT32 hdc_state : 1;
            UINT32 reserved_2 : 1;
            UINT32 lbr_state : 1;
            UINT32 hwp_state : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 supported_upper_ia32_xss_bits : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0d_ecx_01;

typedef struct {
    union {
        struct {
            UINT32 ia32_platform_dca_cap : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 ecx_2 : 1;
            UINT32 ecx_1 : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0d_ecx_n;

/**
 * @}
 */

 /**
  * @defgroup cpuid_eax_0f \
  *           EAX = 0x0F
  * @{
  */
#define CPUID_INTEL_RDT_MONITORING                                   0x0000000F
typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 rmid_max_range : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved_1 : 1;
            UINT32 supports_l3_cache_intel_rdt_monitoring : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0f_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 conversion_factor : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 rmid_max_range : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 supports_l3_occupancy_monitoring : 1;
            UINT32 supports_l3otal_bandwidth_monitoring : 1;
            UINT32 supports_l3_local_bandwidth_monitoring : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_0f_ecx_01;

/**
 * @}
 */

 /**
  * @defgroup cpuid_eax_10 \
  *           EAX = 0x10
  * @{
  */
#define CPUID_INTEL_RDT_ALLOCATION                                   0x00000010
typedef struct {
    union {
        struct {
            UINT32 ia32_platform_dca_cap : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved_1 : 1;
            UINT32 supports_l3_cache_allocationechnology : 1;
            UINT32 supports_l2_cache_allocationechnology : 1;
            UINT32 supports_memory_bandwidth_allocation : 1;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_10_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 length_of_capacity_bit_mask : 5;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 ebx_0 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved_1 : 2;
            UINT32 code_and_data_priorizationechnology_supported : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 highest_cos_number_supported : 16;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_10_ecx_01;

typedef struct {
    union {
        struct {
            UINT32 length_of_capacity_bit_mask : 5;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 ebx_0 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 highest_cos_number_supported : 16;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_10_ecx_02;

typedef struct {
    union {
        struct {
            UINT32 max_mbahrottling_value : 12;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved_1 : 2;
            UINT32 response_of_delay_is_linear : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 highest_cos_number_supported : 16;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_10_ecx_03;

/**
 * @}
 */

 /**
  * @defgroup cpuid_eax_12 \
  *           EAX = 0x12
  * @{
  */
#define CPUID_INTEL_SGX                                              0x00000012
typedef struct {
    union {
        struct {
            UINT32 sgx1 : 1;
            UINT32 sgx2 : 1;
            UINT32 reserved_1 : 3;
            UINT32 sgx_enclv_advanced : 1;
            UINT32 sgx_encls_advanced : 1;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 miscselect : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 max_enclave_size_not64 : 8;
            UINT32 max_enclave_size_64 : 8;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_12_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 valid_secs_attributes_0 : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 valid_secs_attributes_1 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 valid_secs_attributes_2 : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 valid_secs_attributes_3 : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_12_ecx_01;

typedef struct {
    union {
        struct {
            UINT32 sub_leafype : 4;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 zero : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 zero : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 zero : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_12_ecx_02p_slt_0;

typedef struct {
    union {
        struct {
            UINT32 sub_leafype : 4;
            UINT32 reserved_1 : 8;
            UINT32 epc_base_physical_address_1 : 20;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 epc_base_physical_address_2 : 20;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 epc_section_property : 4;
            UINT32 reserved_1 : 8;
            UINT32 epc_size_1 : 20;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 epc_size_2 : 20;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_12_ecx_02p_slt_1;

/**
 * @}
 */

 /**
  * @defgroup cpuid_eax_14 \
  *           EAX = 0x14
  * @{
  */
#define CPUID_INTEL_PROCESSORRACE                                  0x00000014
typedef struct {
    union {
        struct {
            UINT32 max_sub_leaf : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 flag0 : 1;
            UINT32 flag1 : 1;
            UINT32 flag2 : 1;
            UINT32 flag3 : 1;
            UINT32 flag4 : 1;
            UINT32 flag5 : 1;
            UINT32 flag6 : 1;
            UINT32 flag7 : 1;
            UINT32 flag8 : 1;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 flag0 : 1;
            UINT32 flag1 : 1;
            UINT32 flag2 : 1;
            UINT32 flag3 : 1;
            UINT32 reserved_1 : 27;
            UINT32 flag31 : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_14_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 number_of_configurable_address_ranges_for_filtering : 3;
            UINT32 reserved_1 : 13;
            UINT32 bitmap_of_supported_mtc_period_encodings : 16;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 bitmap_of_supported_cyclehreshold_value_encodings : 16;
            UINT32 bitmap_of_supported_configurable_psb_frequency_encodings : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_14_ecx_01;

/**
 * @}
 */

#define CPUIDIME_STAMP_COUNTER                                     0x00000015
typedef struct {
    union {
        struct {
            UINT32 denominator : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 numerator : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 nominal_frequency : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_15;

#define CPUID_PROCESSOR_FREQUENCY                                    0x00000016
typedef struct {
    union {
        struct {
            UINT32 procesor_base_frequency_mhz : 16;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 processor_maximum_frequency_mhz : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 bus_frequency_mhz : 16;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_16;

/**
 * @defgroup cpuid_eax_17 \
 *           EAX = 0x17
 * @{
 */
#define CPUID_SOC_VENDOR                                             0x00000017
typedef struct {
    union {
        struct {
            UINT32 max_soc_id_index : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 soc_vendor_id : 16;
            UINT32 is_vendor_scheme : 1;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 project_id : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 stepping_id : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_17_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 soc_vendor_brand_string : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 soc_vendor_brand_string : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 soc_vendor_brand_string : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 soc_vendor_brand_string : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_17_ecx_01_03;

typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_17_ecx_n;

/**
 * @}
 */

 /**
  * @defgroup cpuid_eax_18 \
  *           EAX = 0x18
  * @{
  */
#define CPUID_DETERMINISTIC_ADDRESSRANSLATION_PARAMETERS           0x00000018
typedef struct {
    union {
        struct {
            UINT32 max_sub_leaf : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 page_entries_4kb_supported : 1;
            UINT32 page_entries_2mb_supported : 1;
            UINT32 page_entries_4mb_supported : 1;
            UINT32 page_entries_1gb_supported : 1;
            UINT32 reserved_1 : 4;
            UINT32 partitioning : 3;
            UINT32 reserved_2 : 5;
            UINT32 ways_of_associativity_00 : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 number_of_sets : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 translation_cacheype_field : 5;
            UINT32 translation_cache_level : 3;
            UINT32 fully_associative_structure : 1;
            UINT32 reserved_1 : 5;
            UINT32 max_addressable_ids_for_logical_processors : 12;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_18_ecx_00;

typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 page_entries_4kb_supported : 1;
            UINT32 page_entries_2mb_supported : 1;
            UINT32 page_entries_4mb_supported : 1;
            UINT32 page_entries_1gb_supported : 1;
            UINT32 reserved_1 : 4;
            UINT32 partitioning : 3;
            UINT32 reserved_2 : 5;
            UINT32 ways_of_associativity_01 : 16;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 number_of_sets : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 translation_cacheype_field : 5;
            UINT32 translation_cache_level : 3;
            UINT32 fully_associative_structure : 1;
            UINT32 reserved_1 : 5;
            UINT32 max_addressable_ids_for_logical_processors : 12;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_18_ecx_01p;

/**
 * @}
 */

#define CPUID_EXTENDED_FUNCTION                                      0x80000000
typedef struct {
    union {
        struct {
            UINT32 max_extended_functions : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000000;

#define CPUID_EXTENDED_CPU_SIGNATURE                                 0x80000001
typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 lahf_sahf_available_in_64_bit_mode : 1;
            UINT32 reserved_1 : 4;
            UINT32 lzcnt : 1;
            UINT32 reserved_2 : 2;
            UINT32 prefetchw : 1;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved_1 : 11;
            UINT32 syscall_sysret_available_in_64_bit_mode : 1;
            UINT32 reserved_2 : 8;
            UINT32 execute_disable_bit_available : 1;
            UINT32 reserved_3 : 5;
            UINT32 pages_1gb_available : 1;
            UINT32 rdtscp_available : 1;
            UINT32 reserved_4 : 1;
            UINT32 ia64_available : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000001;

#define CPUID_BRAND_STRING1                                          0x80000002
#define CPUID_BRAND_STRING2                                          0x80000003
#define CPUID_BRAND_STRING3                                          0x80000004
typedef struct {
    union {
        struct {
            UINT32 processor_brand_string_1 : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 processor_brand_string_2 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 processor_brand_string_3 : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 processor_brand_string_4 : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000002;

typedef struct {
    union {
        struct {
            UINT32 processor_brand_string_5 : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 processor_brand_string_6 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 processor_brand_string_7 : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 processor_brand_string_8 : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000003;

typedef struct {
    union {
        struct {
            UINT32 processor_brand_string_9 : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 processor_brand_string_10 : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 processor_brand_string_11 : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 processor_brand_string_12 : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000004;

typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000005;

#define CPUID_EXTENDED_CACHE_INFO                                    0x80000006
typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 cache_line_size_in_bytes : 8;
            UINT32 reserved_1 : 4;
            UINT32 l2_associativity_field : 4;
            UINT32 cache_size_in_1k_units : 16;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000006;

#define CPUID_EXTENDEDIME_STAMP_COUNTER                            0x80000007
typedef struct {
    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved_1 : 8;
            UINT32 invariantsc_available : 1;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000007;

#define CPUID_EXTENDED_VIRT_PHYS_ADDRESS_SIZE                        0x80000008
typedef struct {
    union {
        struct {
            UINT32 number_of_physical_address_bits : 8;
            UINT32 number_of_linear_address_bits : 8;
        };

        UINT32 flags;
    } eax;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ebx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } ecx;

    union {
        struct {
            UINT32 reserved : 32;
        };

        UINT32 flags;
    } edx;

} cpuid_eax_80000008;

/**
 * @}
 */

 /**
  * @defgroup model_specific_registers \
  *           Model Specific Registers
  * @{
  */
  /**
   * @defgroup ia32_p5_mc \
   *           IA32_P5_MC_(x)
   * @{
   */
#define IA32_P5_MC_ADDR                                              0x00000000
#define IA32_P5_MCYPE                                              0x00000001
   /**
    * @}
    */

#define IA32_MONITOR_FILTER_SIZE                                     0x00000006
#define IA32IME_STAMP_COUNTER                                      0x00000010
#define IA32_PLATFORM_ID                                             0x00000017
typedef union {
    struct {
        UINT64 reserved_1 : 50;
        UINT64 platform_id : 3;
    };

    UINT64 flags;
} ia32_platform_id_register;

#define IA32_APIC_BASE                                               0x0000001B
typedef union {
    struct {
        UINT64 reserved_1 : 8;
        UINT64 bsp_flag : 1;
        UINT64 reserved_2 : 1;
        UINT64 enable_x2apic_mode : 1;
        UINT64 apic_global_enable : 1;
        UINT64 apic_base : 36;
    };

    UINT64 flags;
} ia32_apic_base_register;

#define IA32_FEATURE_CONTROL                                         0x0000003A
typedef union {
    struct {
        UINT64 lock_bit : 1;
        UINT64 enable_vmx_inside_smx : 1;
        UINT64 enable_vmx_outside_smx : 1;
        UINT64 reserved_1 : 5;
        UINT64 senter_local_function_enables : 7;
        UINT64 senter_global_enable : 1;
        UINT64 reserved_2 : 1;
        UINT64 sgx_launch_control_enable : 1;
        UINT64 sgx_global_enable : 1;
        UINT64 reserved_3 : 1;
        UINT64 lmce_on : 1;
    };

    UINT64 flags;
} ia32_feature_control_register;

#define IA32SC_ADJUST                                              0x0000003B
typedef struct {
    UINT64 thread_adjust;
} ia32sc_adjust_register;

#define IA32_SPEC_CTRL                                               0x00000048
typedef union {
    struct {
        UINT64 ibrs : 1;
        UINT64 stibp : 1;
        UINT64 ssbd : 1;
    };

    UINT64 flags;
} ia32_spec_ctrl_register;

#define IA32_PRED_CMD                                                0x00000049
typedef union {
    struct {
        UINT64 ibpb : 1;
    };

    UINT64 flags;
} ia32_pred_cmd_register;

#define IA32_BIOS_UPDTRIG                                          0x00000079
#define IA32_BIOS_SIGN_ID                                            0x0000008B
typedef union {
    struct {
        UINT64 reserved : 32;
        UINT64 microcode_update_signature : 32;
    };

    UINT64 flags;
} ia32_bios_sign_id_register;

/**
 * @defgroup ia32_sgxlepubkeyhash \
 *           IA32_SGXLEPUBKEYHASH[(64*n+63):(64*n)]
 * @{
 */
#define IA32_SGXLEPUBKEYHASH0                                        0x0000008C
#define IA32_SGXLEPUBKEYHASH1                                        0x0000008D
#define IA32_SGXLEPUBKEYHASH2                                        0x0000008E
#define IA32_SGXLEPUBKEYHASH3                                        0x0000008F
 /**
  * @}
  */

#define IA32_SMM_MONITOR_CTL                                         0x0000009B
typedef union {
    struct {
        UINT64 valid : 1;
        UINT64 reserved_1 : 1;
        UINT64 smi_unblocking_by_vmxoff : 1;
        UINT64 reserved_2 : 9;
        UINT64 mseg_base : 20;
    };

    UINT64 flags;
} ia32_smm_monitor_ctl_register;

typedef struct {
    UINT32 mseg_header_revision;
    UINT32 monitor_features;

#define IA32_STM_FEATURES_IA32E                                      0x00000001
    UINT32 gdtr_limit;
    UINT32 gdtr_base_offset;
    UINT32 cs_selector;
    UINT32 eip_offset;
    UINT32 esp_offset;
    UINT32 cr3_offset;
} ia32_mseg_header;

#define IA32_SMBASE                                                  0x0000009E
/**
 * @defgroup ia32_pmc \
 *           IA32_PMC(n)
 * @{
 */
#define IA32_PMC0                                                    0x000000C1
#define IA32_PMC1                                                    0x000000C2
#define IA32_PMC2                                                    0x000000C3
#define IA32_PMC3                                                    0x000000C4
#define IA32_PMC4                                                    0x000000C5
#define IA32_PMC5                                                    0x000000C6
#define IA32_PMC6                                                    0x000000C7
#define IA32_PMC7                                                    0x000000C8
 /**
  * @}
  */

#define IA32_MPERF                                                   0x000000E7
typedef struct {
    UINT64 c0_mcnt;
} ia32_mperf_register;

#define IA32_APERF                                                   0x000000E8
typedef struct {
    UINT64 c0_acnt;
} ia32_aperf_register;

#define IA32_MTRRCAP                                                 0x000000FE
typedef union {
    struct {
        UINT64 variable_range_registers_count : 8;
        UINT64 fixed_range_registers_supported : 1;
        UINT64 reserved_1 : 1;
        UINT64 write_combining : 1;
        UINT64 system_management_range_register : 1;
    };

    UINT64 flags;
} ia32_mtrrcap_register;

#define IA32_ARCH_CAPABILITIES                                       0x0000010A
typedef union {
    struct {
        UINT64 rdcl_no : 1;
        UINT64 ibrs_all : 1;
        UINT64 rsba : 1;
        UINT64 skip_l1dfl_vmentry : 1;
        UINT64 ssb_no : 1;
        UINT64 mds_no : 1;
        UINT64 if_pschange_mc_no : 1;
        UINT64 tsx_ctrl : 1;
        UINT64 taa_no : 1;
    };

    UINT64 flags;
} ia32_arch_capabilities_register;

#define IA32_FLUSH_CMD                                               0x0000010B
typedef union {
    struct {
        UINT64 l1d_flush : 1;
    };

    UINT64 flags;
} ia32_flush_cmd_register;

#define IA32SX_CTRL                                                0x00000122
typedef union {
    struct {
        UINT64 rtm_disable : 1;
        UINT64 tsx_cpuid_clear : 1;
    };

    UINT64 flags;
} ia32sx_ctrl_register;

#define IA32_SYSENTER_CS                                             0x00000174
typedef union {
    struct {
        UINT64 cs_selector : 16;
        UINT64 not_used_1 : 16;
        UINT64 not_used_2 : 32;
    };

    UINT64 flags;
} ia32_sysenter_cs_register;

#define IA32_SYSENTER_ESP                                            0x00000175
#define IA32_SYSENTER_EIP                                            0x00000176
#define IA32_MCG_CAP                                                 0x00000179
typedef union {
    struct {
        UINT64 count : 8;
        UINT64 mcg_ctl_p : 1;
        UINT64 mcg_ext_p : 1;
        UINT64 mcp_cmci_p : 1;
        UINT64 mcges_p : 1;
        UINT64 reserved_1 : 4;
        UINT64 mcg_ext_cnt : 8;
        UINT64 mcg_ser_p : 1;
        UINT64 reserved_2 : 1;
        UINT64 mcg_elog_p : 1;
        UINT64 mcg_lmce_p : 1;
    };

    UINT64 flags;
} ia32_mcg_cap_register;

#define IA32_MCG_STATUS                                              0x0000017A
typedef union {
    struct {
        UINT64 ripv : 1;
        UINT64 eipv : 1;
        UINT64 mcip : 1;
        UINT64 lmce_s : 1;
    };

    UINT64 flags;
} ia32_mcg_status_register;

#define IA32_MCG_CTL                                                 0x0000017B
/**
 * @defgroup ia32_perfevtsel \
 *           IA32_PERFEVTSEL(n)
 * @{
 */
#define IA32_PERFEVTSEL0                                             0x00000186
#define IA32_PERFEVTSEL1                                             0x00000187
#define IA32_PERFEVTSEL2                                             0x00000188
#define IA32_PERFEVTSEL3                                             0x00000189
typedef union {
    struct {
        UINT64 event_select : 8;
        UINT64 u_mask : 8;
        UINT64 usr : 1;
        UINT64 os : 1;
        UINT64 edge : 1;
        UINT64 pc : 1;
        UINT64 intr : 1;
        UINT64 anyhread : 1;
        UINT64 en : 1;
        UINT64 inv : 1;
        UINT64 cmask : 8;
    };

    UINT64 flags;
} ia32_perfevtsel_register;

/**
 * @}
 */

#define IA32_PERF_STATUS                                             0x00000198
typedef union {
    struct {
        UINT64 current_performance_state_value : 16;
    };

    UINT64 flags;
} ia32_perf_status_register;

#define IA32_PERF_CTL                                                0x00000199
typedef union {
    struct {
        UINT64 target_performance_state_value : 16;
        UINT64 reserved_1 : 16;
        UINT64 ida_engage : 1;
    };

    UINT64 flags;
} ia32_perf_ctl_register;

#define IA32_CLOCK_MODULATION                                        0x0000019A
typedef union {
    struct {
        UINT64 extended_on_demand_clock_modulation_duty_cycle : 1;
        UINT64 on_demand_clock_modulation_duty_cycle : 3;
        UINT64 on_demand_clock_modulation_enable : 1;
    };

    UINT64 flags;
} ia32_clock_modulation_register;

#define IA32HERM_INTERRUPT                                         0x0000019B
typedef union {
    struct {
        UINT64 highemperature_interrupt_enable : 1;
        UINT64 lowemperature_interrupt_enable : 1;
        UINT64 prochot_interrupt_enable : 1;
        UINT64 forcepr_interrupt_enable : 1;
        UINT64 criticalemperature_interrupt_enable : 1;
        UINT64 reserved_1 : 3;
        UINT64 threshold1_value : 7;
        UINT64 threshold1_interrupt_enable : 1;
        UINT64 threshold2_value : 7;
        UINT64 threshold2_interrupt_enable : 1;
        UINT64 power_limit_notification_enable : 1;
    };

    UINT64 flags;
} ia32herm_interrupt_register;

#define IA32HERM_STATUS                                            0x0000019C
typedef union {
    struct {
        UINT64 thermal_status : 1;
        UINT64 thermal_status_log : 1;
        UINT64 prochot_forcepr_event : 1;
        UINT64 prochot_forcepr_log : 1;
        UINT64 criticalemperature_status : 1;
        UINT64 criticalemperature_status_log : 1;
        UINT64 thermalhreshold1_status : 1;
        UINT64 thermalhreshold1_log : 1;
        UINT64 thermalhreshold2_status : 1;
        UINT64 thermalhreshold2_log : 1;
        UINT64 power_limitation_status : 1;
        UINT64 power_limitation_log : 1;
        UINT64 current_limit_status : 1;
        UINT64 current_limit_log : 1;
        UINT64 cross_domain_limit_status : 1;
        UINT64 cross_domain_limit_log : 1;
        UINT64 digital_readout : 7;
        UINT64 reserved_1 : 4;
        UINT64 resolution_in_degrees_celsius : 4;
        UINT64 reading_valid : 1;
    };

    UINT64 flags;
} ia32herm_status_register;

#define IA32_MISC_ENABLE                                             0x000001A0
typedef union {
    struct {
        UINT64 fast_strings_enable : 1;
        UINT64 reserved_1 : 2;
        UINT64 automatichermal_control_circuit_enable : 1;
        UINT64 reserved_2 : 3;
        UINT64 performance_monitoring_available : 1;
        UINT64 reserved_3 : 3;
        UINT64 branchrace_storage_unavailable : 1;
        UINT64 processor_event_based_sampling_unavailable : 1;
        UINT64 reserved_4 : 3;
        UINT64 enhanced_intel_speedstepechnology_enable : 1;
        UINT64 reserved_5 : 1;
        UINT64 enable_monitor_fsm : 1;
        UINT64 reserved_6 : 3;
        UINT64 limit_cpuid_maxval : 1;
        UINT64 xtpr_message_disable : 1;
        UINT64 reserved_7 : 10;
        UINT64 xd_bit_disable : 1;
    };

    UINT64 flags;
} ia32_misc_enable_register;

#define IA32_ENERGY_PERF_BIAS                                        0x000001B0
typedef union {
    struct {
        UINT64 power_policy_preference : 4;
    };

    UINT64 flags;
} ia32_energy_perf_bias_register;

#define IA32_PACKAGEHERM_STATUS                                    0x000001B1
typedef union {
    struct {
        UINT64 thermal_status : 1;
        UINT64 thermal_status_log : 1;
        UINT64 prochot_event : 1;
        UINT64 prochot_log : 1;
        UINT64 criticalemperature_status : 1;
        UINT64 criticalemperature_status_log : 1;
        UINT64 thermalhreshold1_status : 1;
        UINT64 thermalhreshold1_log : 1;
        UINT64 thermalhreshold2_status : 1;
        UINT64 thermalhreshold2_log : 1;
        UINT64 power_limitation_status : 1;
        UINT64 power_limitation_log : 1;
        UINT64 reserved_1 : 4;
        UINT64 digital_readout : 7;
    };

    UINT64 flags;
} ia32_packageherm_status_register;

#define IA32_PACKAGEHERM_INTERRUPT                                 0x000001B2
typedef union {
    struct {
        UINT64 highemperature_interrupt_enable : 1;
        UINT64 lowemperature_interrupt_enable : 1;
        UINT64 prochot_interrupt_enable : 1;
        UINT64 reserved_1 : 1;
        UINT64 overheat_interrupt_enable : 1;
        UINT64 reserved_2 : 3;
        UINT64 threshold1_value : 7;
        UINT64 threshold1_interrupt_enable : 1;
        UINT64 threshold2_value : 7;
        UINT64 threshold2_interrupt_enable : 1;
        UINT64 power_limit_notification_enable : 1;
    };

    UINT64 flags;
} ia32_packageherm_interrupt_register;

#define IA32_DEBUGCTL                                                0x000001D9
typedef union {
    struct {
        UINT64 lbr : 1;
        UINT64 btf : 1;
        UINT64 reserved_1 : 4;
        UINT64 tr : 1;
        UINT64 bts : 1;
        UINT64 btint : 1;
        UINT64 bts_off_os : 1;
        UINT64 bts_off_usr : 1;
        UINT64 freeze_lbrs_on_pmi : 1;
        UINT64 freeze_perfmon_on_pmi : 1;
        UINT64 enable_uncore_pmi : 1;
        UINT64 freeze_while_smm : 1;
        UINT64 rtm_debug : 1;
    };

    UINT64 flags;
} ia32_debugctl_register;

#define IA32_SMRR_PHYSBASE                                           0x000001F2
typedef union {
    struct {
        UINT64 type : 8;
        UINT64 reserved_1 : 4;
        UINT64 smrr_physical_base_address : 20;
    };

    UINT64 flags;
} ia32_smrr_physbase_register;

#define IA32_SMRR_PHYSMASK                                           0x000001F3
typedef union {
    struct {
        UINT64 reserved_1 : 11;
        UINT64 enable_range_mask : 1;
        UINT64 smrr_address_range_mask : 20;
    };

    UINT64 flags;
} ia32_smrr_physmask_register;

#define IA32_PLATFORM_DCA_CAP                                        0x000001F8
#define IA32_CPU_DCA_CAP                                             0x000001F9
#define IA32_DCA_0_CAP                                               0x000001FA
typedef union {
    struct {
        UINT64 dca_active : 1;
        UINT64 transaction : 2;
        UINT64 dcaype : 4;
        UINT64 dca_queue_size : 4;
        UINT64 reserved_1 : 2;
        UINT64 dca_delay : 4;
        UINT64 reserved_2 : 7;
        UINT64 sw_block : 1;
        UINT64 reserved_3 : 1;
        UINT64 hw_block : 1;
    };

    UINT64 flags;
} ia32_dca_0_cap_register;

/**
 * @defgroup ia32_mtrr_physbase \
 *           IA32_MTRR_PHYSBASE(n)
 * @{
 */
typedef union {
    struct {
        UINT64 type : 8;
        UINT64 reserved_1 : 4;
        UINT64 physical_addres_base : 36;
    };

    UINT64 flags;
} ia32_mtrr_physbase_register;

#define IA32_MTRR_PHYSBASE0                                          0x00000200
#define IA32_MTRR_PHYSBASE1                                          0x00000202
#define IA32_MTRR_PHYSBASE2                                          0x00000204
#define IA32_MTRR_PHYSBASE3                                          0x00000206
#define IA32_MTRR_PHYSBASE4                                          0x00000208
#define IA32_MTRR_PHYSBASE5                                          0x0000020A
#define IA32_MTRR_PHYSBASE6                                          0x0000020C
#define IA32_MTRR_PHYSBASE7                                          0x0000020E
#define IA32_MTRR_PHYSBASE8                                          0x00000210
#define IA32_MTRR_PHYSBASE9                                          0x00000212
/**
 * @}
 */

 /**
  * @defgroup ia32_mtrr_physmask \
  *           IA32_MTRR_PHYSMASK(n)
  * @{
  */
typedef union {
    struct {
        UINT64 reserved_1 : 11;
        UINT64 valid : 1;
        UINT64 physical_addres_mask : 36;
    };

    UINT64 flags;
} ia32_mtrr_physmask_register;

#define IA32_MTRR_PHYSMASK0                                          0x00000201
#define IA32_MTRR_PHYSMASK1                                          0x00000203
#define IA32_MTRR_PHYSMASK2                                          0x00000205
#define IA32_MTRR_PHYSMASK3                                          0x00000207
#define IA32_MTRR_PHYSMASK4                                          0x00000209
#define IA32_MTRR_PHYSMASK5                                          0x0000020B
#define IA32_MTRR_PHYSMASK6                                          0x0000020D
#define IA32_MTRR_PHYSMASK7                                          0x0000020F
#define IA32_MTRR_PHYSMASK8                                          0x00000211
#define IA32_MTRR_PHYSMASK9                                          0x00000213
/**
 * @}
 */

 /**
  * @defgroup ia32_mtrr_fix \
  *           IA32_MTRR_FIX(x)
  * @{
  */
  /**
   * @defgroup ia32_mtrr_fix64k \
   *           IA32_MTRR_FIX64K(x)
   * @{
   */
#define IA32_MTRR_FIX64K_BASE                                        0x00000000
#define IA32_MTRR_FIX64K_SIZE                                        0x00010000
#define IA32_MTRR_FIX64K_00000                                       0x00000250
   /**
    * @}
    */

    /**
     * @defgroup ia32_mtrr_fix16k \
     *           IA32_MTRR_FIX16K(x)
     * @{
     */
#define IA32_MTRR_FIX16K_BASE                                        0x00080000
#define IA32_MTRR_FIX16K_SIZE                                        0x00004000
#define IA32_MTRR_FIX16K_80000                                       0x00000258
#define IA32_MTRR_FIX16K_A0000                                       0x00000259
     /**
      * @}
      */

      /**
       * @defgroup ia32_mtrr_fix4k \
       *           IA32_MTRR_FIX4K(x)
       * @{
       */
#define IA32_MTRR_FIX4K_BASE                                         0x000C0000
#define IA32_MTRR_FIX4K_SIZE                                         0x00001000
#define IA32_MTRR_FIX4K_C0000                                        0x00000268
#define IA32_MTRR_FIX4K_C8000                                        0x00000269
#define IA32_MTRR_FIX4K_D0000                                        0x0000026A
#define IA32_MTRR_FIX4K_D8000                                        0x0000026B
#define IA32_MTRR_FIX4K_E0000                                        0x0000026C
#define IA32_MTRR_FIX4K_E8000                                        0x0000026D
#define IA32_MTRR_FIX4K_F0000                                        0x0000026E
#define IA32_MTRR_FIX4K_F8000                                        0x0000026F
       /**
        * @}
        */

#define IA32_MTRR_FIX_COUNT                                          ((1 + 2 + 8) * 8)
#define IA32_MTRR_VARIABLE_COUNT                                     0x0000000A
#define IA32_MTRR_COUNT                                              (IA32_MTRR_FIX_COUNT + IA32_MTRR_VARIABLE_COUNT)
        /**
         * @}
         */

#define IA32_PAT                                                     0x00000277
typedef union {
    struct {
        UINT64 pa0 : 3;
        UINT64 reserved_1 : 5;
        UINT64 pa1 : 3;
        UINT64 reserved_2 : 5;
        UINT64 pa2 : 3;
        UINT64 reserved_3 : 5;
        UINT64 pa3 : 3;
        UINT64 reserved_4 : 5;
        UINT64 pa4 : 3;
        UINT64 reserved_5 : 5;
        UINT64 pa5 : 3;
        UINT64 reserved_6 : 5;
        UINT64 pa6 : 3;
        UINT64 reserved_7 : 5;
        UINT64 pa7 : 3;
    };

    UINT64 flags;
} ia32_pat_register;

/**
 * @defgroup ia32_mc_ctl2 \
 *           IA32_MC(i)_CTL2
 * @{
 */
#define IA32_MC0_CTL2                                                0x00000280
#define IA32_MC1_CTL2                                                0x00000281
#define IA32_MC2_CTL2                                                0x00000282
#define IA32_MC3_CTL2                                                0x00000283
#define IA32_MC4_CTL2                                                0x00000284
#define IA32_MC5_CTL2                                                0x00000285
#define IA32_MC6_CTL2                                                0x00000286
#define IA32_MC7_CTL2                                                0x00000287
#define IA32_MC8_CTL2                                                0x00000288
#define IA32_MC9_CTL2                                                0x00000289
#define IA32_MC10_CTL2                                               0x0000028A
#define IA32_MC11_CTL2                                               0x0000028B
#define IA32_MC12_CTL2                                               0x0000028C
#define IA32_MC13_CTL2                                               0x0000028D
#define IA32_MC14_CTL2                                               0x0000028E
#define IA32_MC15_CTL2                                               0x0000028F
#define IA32_MC16_CTL2                                               0x00000290
#define IA32_MC17_CTL2                                               0x00000291
#define IA32_MC18_CTL2                                               0x00000292
#define IA32_MC19_CTL2                                               0x00000293
#define IA32_MC20_CTL2                                               0x00000294
#define IA32_MC21_CTL2                                               0x00000295
#define IA32_MC22_CTL2                                               0x00000296
#define IA32_MC23_CTL2                                               0x00000297
#define IA32_MC24_CTL2                                               0x00000298
#define IA32_MC25_CTL2                                               0x00000299
#define IA32_MC26_CTL2                                               0x0000029A
#define IA32_MC27_CTL2                                               0x0000029B
#define IA32_MC28_CTL2                                               0x0000029C
#define IA32_MC29_CTL2                                               0x0000029D
#define IA32_MC30_CTL2                                               0x0000029E
#define IA32_MC31_CTL2                                               0x0000029F
typedef union {
    struct {
        UINT64 corrected_error_counthreshold : 15;
        UINT64 reserved_1 : 15;
        UINT64 cmci_en : 1;
    };

    UINT64 flags;
} ia32_mc_ctl2_register;

/**
 * @}
 */

#define IA32_MTRR_DEFYPE                                           0x000002FF
typedef union {
    struct {
        UINT64 default_memoryype : 3;
        UINT64 reserved_1 : 7;
        UINT64 fixed_range_mtrr_enable : 1;
        UINT64 mtrr_enable : 1;
    };

    UINT64 flags;
} ia32_mtrr_defype_register;

/**
 * @defgroup ia32_fixed_ctr \
 *           IA32_FIXED_CTR(n)
 * @{
 */
#define IA32_FIXED_CTR0                                              0x00000309
#define IA32_FIXED_CTR1                                              0x0000030A
#define IA32_FIXED_CTR2                                              0x0000030B
 /**
  * @}
  */

#define IA32_PERF_CAPABILITIES                                       0x00000345
typedef union {
    struct {
        UINT64 lbr_format : 6;
        UINT64 pebsrap : 1;
        UINT64 pebs_save_arch_regs : 1;
        UINT64 pebs_record_format : 4;
        UINT64 freeze_while_smm_is_supported : 1;
        UINT64 full_width_counter_write : 1;
    };

    UINT64 flags;
} ia32_perf_capabilities_register;

#define IA32_FIXED_CTR_CTRL                                          0x0000038D
typedef union {
    struct {
        UINT64 en0_os : 1;
        UINT64 en0_usr : 1;
        UINT64 anyhread0 : 1;
        UINT64 en0_pmi : 1;
        UINT64 en1_os : 1;
        UINT64 en1_usr : 1;
        UINT64 anyhread1 : 1;
        UINT64 en1_pmi : 1;
        UINT64 en2_os : 1;
        UINT64 en2_usr : 1;
        UINT64 anyhread2 : 1;
        UINT64 en2_pmi : 1;
    };

    UINT64 flags;
} ia32_fixed_ctr_ctrl_register;

#define IA32_PERF_GLOBAL_STATUS                                      0x0000038E
typedef union {
    struct {
        UINT64 ovf_pmc0 : 1;
        UINT64 ovf_pmc1 : 1;
        UINT64 ovf_pmc2 : 1;
        UINT64 ovf_pmc3 : 1;
        UINT64 reserved_1 : 28;
        UINT64 ovf_fixedctr0 : 1;
        UINT64 ovf_fixedctr1 : 1;
        UINT64 ovf_fixedctr2 : 1;
        UINT64 reserved_2 : 20;
        UINT64 traceopa_pmi : 1;
        UINT64 reserved_3 : 2;
        UINT64 lbr_frz : 1;
        UINT64 ctr_frz : 1;
        UINT64 asci : 1;
        UINT64 ovf_uncore : 1;
        UINT64 ovf_buf : 1;
        UINT64 cond_chgd : 1;
    };

    UINT64 flags;
} ia32_perf_global_status_register;

#define IA32_PERF_GLOBAL_CTRL                                        0x0000038F
typedef union {
    struct {
        UINT64 en_pmcn : 32;
        UINT64 en_fixed_ctrn : 32;
    };

    UINT64 flags;
} ia32_perf_global_ctrl_register;

#define IA32_PERF_GLOBAL_STATUS_RESET                                0x00000390
typedef union {
    struct {
        UINT64 clear_ovf_pmcn : 32;
        UINT64 clear_ovf_fixed_ctrn : 3;
        UINT64 reserved_1 : 20;
        UINT64 clearraceopa_pmi : 1;
        UINT64 reserved_2 : 2;
        UINT64 clear_lbr_frz : 1;
        UINT64 clear_ctr_frz : 1;
        UINT64 clear_asci : 1;
        UINT64 clear_ovf_uncore : 1;
        UINT64 clear_ovf_buf : 1;
        UINT64 clear_cond_chgd : 1;
    };

    UINT64 flags;
} ia32_perf_global_status_reset_register;

#define IA32_PERF_GLOBAL_STATUS_SET                                  0x00000391
typedef union {
    struct {
        UINT64 ovf_pmcn : 32;
        UINT64 ovf_fixed_ctrn : 3;
        UINT64 reserved_1 : 20;
        UINT64 traceopa_pmi : 1;
        UINT64 reserved_2 : 2;
        UINT64 lbr_frz : 1;
        UINT64 ctr_frz : 1;
        UINT64 asci : 1;
        UINT64 ovf_uncore : 1;
        UINT64 ovf_buf : 1;
    };

    UINT64 flags;
} ia32_perf_global_status_set_register;

#define IA32_PERF_GLOBAL_INUSE                                       0x00000392
typedef union {
    struct {
        UINT64 ia32_perfevtseln_in_use : 32;
        UINT64 ia32_fixed_ctrn_in_use : 3;
        UINT64 reserved_1 : 28;
        UINT64 pmi_in_use : 1;
    };

    UINT64 flags;
} ia32_perf_global_inuse_register;

#define IA32_PEBS_ENABLE                                             0x000003F1
typedef union {
    struct {
        UINT64 enable_pebs : 1;
        UINT64 reservedormodelspecific1 : 3;
        UINT64 reserved_1 : 28;
        UINT64 reservedormodelspecific2 : 4;
    };

    UINT64 flags;
} ia32_pebs_enable_register;

/**
 * @defgroup ia32_mc_ctl \
 *           IA32_MC(i)_CTL
 * @{
 */
#define IA32_MC0_CTL                                                 0x00000400
#define IA32_MC1_CTL                                                 0x00000404
#define IA32_MC2_CTL                                                 0x00000408
#define IA32_MC3_CTL                                                 0x0000040C
#define IA32_MC4_CTL                                                 0x00000410
#define IA32_MC5_CTL                                                 0x00000414
#define IA32_MC6_CTL                                                 0x00000418
#define IA32_MC7_CTL                                                 0x0000041C
#define IA32_MC8_CTL                                                 0x00000420
#define IA32_MC9_CTL                                                 0x00000424
#define IA32_MC10_CTL                                                0x00000428
#define IA32_MC11_CTL                                                0x0000042C
#define IA32_MC12_CTL                                                0x00000430
#define IA32_MC13_CTL                                                0x00000434
#define IA32_MC14_CTL                                                0x00000438
#define IA32_MC15_CTL                                                0x0000043C
#define IA32_MC16_CTL                                                0x00000440
#define IA32_MC17_CTL                                                0x00000444
#define IA32_MC18_CTL                                                0x00000448
#define IA32_MC19_CTL                                                0x0000044C
#define IA32_MC20_CTL                                                0x00000450
#define IA32_MC21_CTL                                                0x00000454
#define IA32_MC22_CTL                                                0x00000458
#define IA32_MC23_CTL                                                0x0000045C
#define IA32_MC24_CTL                                                0x00000460
#define IA32_MC25_CTL                                                0x00000464
#define IA32_MC26_CTL                                                0x00000468
#define IA32_MC27_CTL                                                0x0000046C
#define IA32_MC28_CTL                                                0x00000470
 /**
  * @}
  */

  /**
   * @defgroup ia32_mc_status \
   *           IA32_MC(i)_STATUS
   * @{
   */
#define IA32_MC0_STATUS                                              0x00000401
#define IA32_MC1_STATUS                                              0x00000405
#define IA32_MC2_STATUS                                              0x00000409
#define IA32_MC3_STATUS                                              0x0000040D
#define IA32_MC4_STATUS                                              0x00000411
#define IA32_MC5_STATUS                                              0x00000415
#define IA32_MC6_STATUS                                              0x00000419
#define IA32_MC7_STATUS                                              0x0000041D
#define IA32_MC8_STATUS                                              0x00000421
#define IA32_MC9_STATUS                                              0x00000425
#define IA32_MC10_STATUS                                             0x00000429
#define IA32_MC11_STATUS                                             0x0000042D
#define IA32_MC12_STATUS                                             0x00000431
#define IA32_MC13_STATUS                                             0x00000435
#define IA32_MC14_STATUS                                             0x00000439
#define IA32_MC15_STATUS                                             0x0000043D
#define IA32_MC16_STATUS                                             0x00000441
#define IA32_MC17_STATUS                                             0x00000445
#define IA32_MC18_STATUS                                             0x00000449
#define IA32_MC19_STATUS                                             0x0000044D
#define IA32_MC20_STATUS                                             0x00000451
#define IA32_MC21_STATUS                                             0x00000455
#define IA32_MC22_STATUS                                             0x00000459
#define IA32_MC23_STATUS                                             0x0000045D
#define IA32_MC24_STATUS                                             0x00000461
#define IA32_MC25_STATUS                                             0x00000465
#define IA32_MC26_STATUS                                             0x00000469
#define IA32_MC27_STATUS                                             0x0000046D
#define IA32_MC28_STATUS                                             0x00000471
   /**
    * @}
    */

    /**
     * @defgroup ia32_mc_addr \
     *           IA32_MC(i)_ADDR
     * @{
     */
#define IA32_MC0_ADDR                                                0x00000402
#define IA32_MC1_ADDR                                                0x00000406
#define IA32_MC2_ADDR                                                0x0000040A
#define IA32_MC3_ADDR                                                0x0000040E
#define IA32_MC4_ADDR                                                0x00000412
#define IA32_MC5_ADDR                                                0x00000416
#define IA32_MC6_ADDR                                                0x0000041A
#define IA32_MC7_ADDR                                                0x0000041E
#define IA32_MC8_ADDR                                                0x00000422
#define IA32_MC9_ADDR                                                0x00000426
#define IA32_MC10_ADDR                                               0x0000042A
#define IA32_MC11_ADDR                                               0x0000042E
#define IA32_MC12_ADDR                                               0x00000432
#define IA32_MC13_ADDR                                               0x00000436
#define IA32_MC14_ADDR                                               0x0000043A
#define IA32_MC15_ADDR                                               0x0000043E
#define IA32_MC16_ADDR                                               0x00000442
#define IA32_MC17_ADDR                                               0x00000446
#define IA32_MC18_ADDR                                               0x0000044A
#define IA32_MC19_ADDR                                               0x0000044E
#define IA32_MC20_ADDR                                               0x00000452
#define IA32_MC21_ADDR                                               0x00000456
#define IA32_MC22_ADDR                                               0x0000045A
#define IA32_MC23_ADDR                                               0x0000045E
#define IA32_MC24_ADDR                                               0x00000462
#define IA32_MC25_ADDR                                               0x00000466
#define IA32_MC26_ADDR                                               0x0000046A
#define IA32_MC27_ADDR                                               0x0000046E
#define IA32_MC28_ADDR                                               0x00000472
     /**
      * @}
      */

      /**
       * @defgroup ia32_mc_misc \
       *           IA32_MC(i)_MISC
       * @{
       */
#define IA32_MC0_MISC                                                0x00000403
#define IA32_MC1_MISC                                                0x00000407
#define IA32_MC2_MISC                                                0x0000040B
#define IA32_MC3_MISC                                                0x0000040F
#define IA32_MC4_MISC                                                0x00000413
#define IA32_MC5_MISC                                                0x00000417
#define IA32_MC6_MISC                                                0x0000041B
#define IA32_MC7_MISC                                                0x0000041F
#define IA32_MC8_MISC                                                0x00000423
#define IA32_MC9_MISC                                                0x00000427
#define IA32_MC10_MISC                                               0x0000042B
#define IA32_MC11_MISC                                               0x0000042F
#define IA32_MC12_MISC                                               0x00000433
#define IA32_MC13_MISC                                               0x00000437
#define IA32_MC14_MISC                                               0x0000043B
#define IA32_MC15_MISC                                               0x0000043F
#define IA32_MC16_MISC                                               0x00000443
#define IA32_MC17_MISC                                               0x00000447
#define IA32_MC18_MISC                                               0x0000044B
#define IA32_MC19_MISC                                               0x0000044F
#define IA32_MC20_MISC                                               0x00000453
#define IA32_MC21_MISC                                               0x00000457
#define IA32_MC22_MISC                                               0x0000045B
#define IA32_MC23_MISC                                               0x0000045F
#define IA32_MC24_MISC                                               0x00000463
#define IA32_MC25_MISC                                               0x00000467
#define IA32_MC26_MISC                                               0x0000046B
#define IA32_MC27_MISC                                               0x0000046F
#define IA32_MC28_MISC                                               0x00000473
       /**
        * @}
        */

#define IA32_VMX_BASIC                                               0x00000480
typedef union {
    struct {
        UINT64 vmcs_revision_id : 31;
        UINT64 must_be_zero : 1;
        UINT64 vmcs_size_in_bytes : 13;
        UINT64 reserved_1 : 3;
        UINT64 vmcs_physical_address_width : 1;
        UINT64 dual_monitor : 1;
        UINT64 memoryype : 4;
        UINT64 ins_outs_vmexit_information : 1;
        UINT64 true_controls : 1;
    };

    UINT64 flags;
} ia32_vmx_basic_register;

#define IA32_VMX_PINBASED_CTLS                                       0x00000481
typedef union {
    struct {
        UINT64 external_interrupt_exiting : 1;
        UINT64 reserved_1 : 2;
        UINT64 nmi_exiting : 1;
        UINT64 reserved_2 : 1;
        UINT64 virtual_nmis : 1;
        UINT64 activate_vmx_preemptionimer : 1;
        UINT64 process_posted_interrupts : 1;
    };

    UINT64 flags;
} ia32_vmx_pinbased_ctls_register;

#define IA32_VMX_PROCBASED_CTLS                                      0x00000482
typedef union {
    struct {
        UINT64 reserved_1 : 2;
        UINT64 interrupt_window_exiting : 1;
        UINT64 usesc_offsetting : 1;
        UINT64 reserved_2 : 3;
        UINT64 hlt_exiting : 1;
        UINT64 reserved_3 : 1;
        UINT64 invlpg_exiting : 1;
        UINT64 mwait_exiting : 1;
        UINT64 rdpmc_exiting : 1;
        UINT64 rdtsc_exiting : 1;
        UINT64 reserved_4 : 2;
        UINT64 cr3_load_exiting : 1;
        UINT64 cr3_store_exiting : 1;
        UINT64 activateertiary_controls : 1;
        UINT64 reserved_5 : 1;
        UINT64 cr8_load_exiting : 1;
        UINT64 cr8_store_exiting : 1;
        UINT64 usepr_shadow : 1;
        UINT64 nmi_window_exiting : 1;
        UINT64 mov_dr_exiting : 1;
        UINT64 unconditional_io_exiting : 1;
        UINT64 use_io_bitmaps : 1;
        UINT64 reserved_6 : 1;
        UINT64 monitorrap_flag : 1;
        UINT64 use_msr_bitmaps : 1;
        UINT64 monitor_exiting : 1;
        UINT64 pause_exiting : 1;
        UINT64 activate_secondary_controls : 1;
    };

    UINT64 flags;
} ia32_vmx_procbased_ctls_register;

#define IA32_VMX_EXIT_CTLS                                           0x00000483
typedef union {
    struct {
        UINT64 reserved_1 : 2;
        UINT64 save_debug_controls : 1;
        UINT64 reserved_2 : 6;
        UINT64 host_address_space_size : 1;
        UINT64 reserved_3 : 2;
        UINT64 load_ia32_perf_global_ctrl : 1;
        UINT64 reserved_4 : 2;
        UINT64 acknowledge_interrupt_on_exit : 1;
        UINT64 reserved_5 : 2;
        UINT64 save_ia32_pat : 1;
        UINT64 load_ia32_pat : 1;
        UINT64 save_ia32_efer : 1;
        UINT64 load_ia32_efer : 1;
        UINT64 save_vmx_preemptionimer_value : 1;
        UINT64 clear_ia32_bndcfgs : 1;
        UINT64 conceal_vmx_from_pt : 1;
        UINT64 clear_ia32_rtit_ctl : 1;
        UINT64 clear_ia32_lbr_ctl : 1;
        UINT64 clear_uinv : 1;
        UINT64 load_ia32_cet_state : 1;
        UINT64 load_ia32_pkrs : 1;
        UINT64 save_ia32_perf_global_ctl : 1;
        UINT64 activate_secondary_controls : 1;
    };

    UINT64 flags;
} ia32_vmx_exit_ctls_register;

#define IA32_VMX_ENTRY_CTLS                                          0x00000484
typedef union {
    struct {
        UINT64 reserved_1 : 2;
        UINT64 load_debug_controls : 1;
        UINT64 reserved_2 : 6;
        UINT64 ia32e_mode_guest : 1;
        UINT64 entryo_smm : 1;
        UINT64 deactivate_dual_monitorreatment : 1;
        UINT64 reserved_3 : 1;
        UINT64 load_ia32_perf_global_ctrl : 1;
        UINT64 load_ia32_pat : 1;
        UINT64 load_ia32_efer : 1;
        UINT64 load_ia32_bndcfgs : 1;
        UINT64 conceal_vmx_from_pt : 1;
        UINT64 load_ia32_rtit_ctl : 1;
        UINT64 load_uinv : 1;
        UINT64 load_cet_state : 1;
        UINT64 load_ia32_lbr_ctl : 1;
        UINT64 load_ia32_pkrs : 1;
    };

    UINT64 flags;
} ia32_vmx_entry_ctls_register;

#define IA32_VMX_MISC                                                0x00000485
typedef union {
    struct {
        UINT64 preemptionimersc_relationship : 5;
        UINT64 store_efer_lma_on_vmexit : 1;
        UINT64 activity_states : 3;
        UINT64 reserved_1 : 5;
        UINT64 intel_pt_available_in_vmx : 1;
        UINT64 rdmsr_can_read_ia32_smbase_msr_in_smm : 1;
        UINT64 cr3arget_count : 9;
        UINT64 max_number_of_msr : 3;
        UINT64 smm_monitor_ctl_b2 : 1;
        UINT64 vmwrite_vmexit_info : 1;
        UINT64 zero_length_instruction_vmentry_injection : 1;
        UINT64 reserved_2 : 1;
        UINT64 mseg_id : 32;
    };

    UINT64 flags;
} ia32_vmx_misc_register;

#define IA32_VMX_CR0_FIXED0                                          0x00000486
#define IA32_VMX_CR0_FIXED1                                          0x00000487
#define IA32_VMX_CR4_FIXED0                                          0x00000488
#define IA32_VMX_CR4_FIXED1                                          0x00000489
#define IA32_VMX_VMCS_ENUM                                           0x0000048A
typedef union {
    struct {
        UINT64 accessype : 1;
        UINT64 highest_index_value : 9;
        UINT64 fieldype : 2;
        UINT64 reserved_1 : 1;
        UINT64 field_width : 2;
    };

    UINT64 flags;
} ia32_vmx_vmcs_enum_register;

#define IA32_VMX_PROCBASED_CTLS2                                     0x0000048B
typedef union {
    struct {
        UINT64 virtualize_apic_accesses : 1;
        UINT64 enable_ept : 1;
        UINT64 descriptorable_exiting : 1;
        UINT64 enable_rdtscp : 1;
        UINT64 virtualize_x2apic_mode : 1;
        UINT64 enable_vpid : 1;
        UINT64 wbinvd_exiting : 1;
        UINT64 unrestricted_guest : 1;
        UINT64 apic_register_virtualization : 1;
        UINT64 virtual_interrupt_delivery : 1;
        UINT64 pause_loop_exiting : 1;
        UINT64 rdrand_exiting : 1;
        UINT64 enable_invpcid : 1;
        UINT64 enable_vm_functions : 1;
        UINT64 vmcs_shadowing : 1;
        UINT64 enable_encls_exiting : 1;
        UINT64 rdseed_exiting : 1;
        UINT64 enable_pml : 1;
        UINT64 ept_violation : 1;
        UINT64 conceal_vmx_from_pt : 1;
        UINT64 enable_xsaves : 1;
        UINT64 enable_pasidranslation : 1;
        UINT64 mode_based_execute_control_for_ept : 1;
        UINT64 sub_page_write_permissions_for_ept : 1;
        UINT64 pt_uses_guest_physical_addresses : 1;
        UINT64 usesc_scaling : 1;
        UINT64 enable_user_wait_pause : 1;
        UINT64 enable_pconfig : 1;
        UINT64 enable_enclv_exiting : 1;
        UINT64 reserved_1 : 1;
        UINT64 enable_vmm_bus_lock_detection : 1;
        UINT64 enable_instructionimeout_exit : 1;
    };

    UINT64 flags;
} ia32_vmx_procbased_ctls2_register;

#define IA32_VMX_EPT_VPID_CAP                                        0x0000048C
typedef union {
    struct {
        UINT64 execute_only_pages : 1;
        UINT64 reserved_1 : 5;
        UINT64 page_walk_length_4 : 1;
        UINT64 reserved_2 : 1;
        UINT64 memoryype_uncacheable : 1;
        UINT64 reserved_3 : 5;
        UINT64 memoryype_write_back : 1;
        UINT64 reserved_4 : 1;
        UINT64 pde_2mb_pages : 1;
        UINT64 pdpte_1gb_pages : 1;
        UINT64 reserved_5 : 2;
        UINT64 invept : 1;
        UINT64 ept_accessed_and_dirty_flags : 1;
        UINT64 advanced_vmexit_ept_violations_information : 1;
        UINT64 supervisor_shadow_stack : 1;
        UINT64 reserved_6 : 1;
        UINT64 invept_single_context : 1;
        UINT64 invept_all_contexts : 1;
        UINT64 reserved_7 : 5;
        UINT64 invvpid : 1;
        UINT64 reserved_8 : 7;
        UINT64 invvpid_individual_address : 1;
        UINT64 invvpid_single_context : 1;
        UINT64 invvpid_all_contexts : 1;
        UINT64 invvpid_single_context_retain_globals : 1;
        UINT64 reserved_9 : 4;
        UINT64 max_hlat_prefix_size : 6;
    };

    UINT64 flags;
} ia32_vmx_ept_vpid_cap_register;

/**
 * @defgroup ia32_vmxrue_ctls \
 *           IA32_VMXRUE_(x)_CTLS
 * @{
 */
#define IA32_VMXRUE_PINBASED_CTLS                                  0x0000048D
#define IA32_VMXRUE_PROCBASED_CTLS                                 0x0000048E
#define IA32_VMXRUE_EXIT_CTLS                                      0x0000048F
#define IA32_VMXRUE_ENTRY_CTLS                                     0x00000490
typedef union {
    struct {
        UINT64 allowed_0_settings : 32;
        UINT64 allowed_1_settings : 32;
    };

    UINT64 flags;
} ia32_vmxrue_ctls_register;

/**
 * @}
 */

#define IA32_VMX_VMFUNC                                              0x00000491
typedef union {
    struct {
        UINT64 eptp_switching : 1;
    };

    UINT64 flags;
} ia32_vmx_vmfunc_register;

#define IA32_VMX_PROCBASED_CTLS3                                     0x00000492
typedef union {
    struct {
        UINT64 loadiwkey_exiting : 1;
        UINT64 enable_hlat : 1;
        UINT64 ept_paging_write : 1;
        UINT64 guest_paging : 1;
        UINT64 enable_ipi_virtualization : 1;
        UINT64 reserved_1 : 1;
        UINT64 enable_rdmsrlist_wrmsrlist : 1;
        UINT64 virtualize_ia32_spec_ctrl : 1;
    };

    UINT64 flags;
} ia32_vmx_procbased_ctls3_register;

#define IA32_VMX_EXIT_CTLS2                                          0x00000493
typedef union {
    struct {
        UINT64 reserved_1 : 3;
        UINT64 enable_prematurely_busy_shadow_stack_indication : 1;
    };

    UINT64 flags;
} ia32_vmx_exit_ctls2_register;

/**
 * @defgroup ia32_a_pmc \
 *           IA32_A_PMC(n)
 * @{
 */
#define IA32_A_PMC0                                                  0x000004C1
#define IA32_A_PMC1                                                  0x000004C2
#define IA32_A_PMC2                                                  0x000004C3
#define IA32_A_PMC3                                                  0x000004C4
#define IA32_A_PMC4                                                  0x000004C5
#define IA32_A_PMC5                                                  0x000004C6
#define IA32_A_PMC6                                                  0x000004C7
#define IA32_A_PMC7                                                  0x000004C8
 /**
  * @}
  */

#define IA32_MCG_EXT_CTL                                             0x000004D0
typedef union {
    struct {
        UINT64 lmce_en : 1;
    };

    UINT64 flags;
} ia32_mcg_ext_ctl_register;

#define IA32_SGX_SVN_STATUS                                          0x00000500
typedef union {
    struct {
        UINT64 lock : 1;
        UINT64 reserved_1 : 15;
        UINT64 sgx_svn_sinit : 8;
    };

    UINT64 flags;
} ia32_sgx_svn_status_register;

#define IA32_RTIT_OUTPUT_BASE                                        0x00000560
typedef union {
    struct {
        UINT64 reserved_1 : 7;
        UINT64 base_physical_address : 41;
    };

    UINT64 flags;
} ia32_rtit_output_base_register;

#define IA32_RTIT_OUTPUT_MASK_PTRS                                   0x00000561
typedef union {
    struct {
        UINT64 lower_mask : 7;
        UINT64 mask_orable_offset : 25;
        UINT64 output_offset : 32;
    };

    UINT64 flags;
} ia32_rtit_output_mask_ptrs_register;

#define IA32_RTIT_CTL                                                0x00000570
typedef union {
    struct {
        UINT64 trace_en : 1;
        UINT64 cyc_en : 1;
        UINT64 os : 1;
        UINT64 user : 1;
        UINT64 pwr_evt_en : 1;
        UINT64 fup_on_ptw : 1;
        UINT64 fabric_en : 1;
        UINT64 cr3_filter : 1;
        UINT64 topa : 1;
        UINT64 mtc_en : 1;
        UINT64 tsc_en : 1;
        UINT64 dis_retc : 1;
        UINT64 ptw_en : 1;
        UINT64 branch_en : 1;
        UINT64 mtc_freq : 4;
        UINT64 reserved_1 : 1;
        UINT64 cychresh : 4;
        UINT64 reserved_2 : 1;
        UINT64 psb_freq : 4;
        UINT64 reserved_3 : 4;
        UINT64 addr0_cfg : 4;
        UINT64 addr1_cfg : 4;
        UINT64 addr2_cfg : 4;
        UINT64 addr3_cfg : 4;
        UINT64 reserved_4 : 8;
        UINT64 inject_psb_pmi_on_enable : 1;
    };

    UINT64 flags;
} ia32_rtit_ctl_register;

#define IA32_RTIT_STATUS                                             0x00000571
typedef union {
    struct {
        UINT64 filter_en : 1;
        UINT64 contex_en : 1;
        UINT64 trigger_en : 1;
        UINT64 reserved_1 : 1;
        UINT64 error : 1;
        UINT64 stopped : 1;
        UINT64 pend_psb : 1;
        UINT64 pendopa_pmi : 1;
        UINT64 reserved_2 : 24;
        UINT64 packet_byte_cnt : 17;
    };

    UINT64 flags;
} ia32_rtit_status_register;

#define IA32_RTIT_CR3_MATCH                                          0x00000572
typedef union {
    struct {
        UINT64 reserved_1 : 5;
        UINT64 cr3_valueo_match : 59;
    };

    UINT64 flags;
} ia32_rtit_cr3_match_register;

/**
 * @defgroup ia32_rtit_addr \
 *           IA32_RTIT_ADDR(x)
 * @{
 */
 /**
  * @defgroup ia32_rtit_addr_a \
  *           IA32_RTIT_ADDR(n)_A
  * @{
  */
#define IA32_RTIT_ADDR0_A                                            0x00000580
#define IA32_RTIT_ADDR1_A                                            0x00000582
#define IA32_RTIT_ADDR2_A                                            0x00000584
#define IA32_RTIT_ADDR3_A                                            0x00000586
  /**
   * @}
   */

   /**
    * @defgroup ia32_rtit_addr_b \
    *           IA32_RTIT_ADDR(n)_B
    * @{
    */
#define IA32_RTIT_ADDR0_B                                            0x00000581
#define IA32_RTIT_ADDR1_B                                            0x00000583
#define IA32_RTIT_ADDR2_B                                            0x00000585
#define IA32_RTIT_ADDR3_B                                            0x00000587
    /**
     * @}
     */

typedef union {
    struct {
        UINT64 virtual_address : 48;
        UINT64 sign_ext_va : 16;
    };

    UINT64 flags;
} ia32_rtit_addr_register;

/**
 * @}
 */

#define IA32_DS_AREA                                                 0x00000600
#define IA32_U_CET                                                   0x000006A0
typedef union {
    struct {
        UINT64 sh_stk_en : 1;
        UINT64 wr_shstk_en : 1;
        UINT64 endbr_en : 1;
        UINT64 leg_iw_en : 1;
        UINT64 norack_en : 1;
        UINT64 suppress_dis : 1;
        UINT64 reserved_1 : 4;
        UINT64 suppress : 1;
        UINT64 tracker : 1;
        UINT64 eb_leg_bitmap_base : 52;
    };

    UINT64 flags;
} ia32_u_cet_register;

#define IA32_S_CET                                                   0x000006A2
typedef union {
    struct {
        UINT64 sh_stk_en : 1;
        UINT64 wr_shstk_en : 1;
        UINT64 endbr_en : 1;
        UINT64 leg_iw_en : 1;
        UINT64 norack_en : 1;
        UINT64 suppress_dis : 1;
        UINT64 reserved_1 : 4;
        UINT64 suppress : 1;
        UINT64 tracker : 1;
        UINT64 eb_leg_bitmap_base : 52;
    };

    UINT64 flags;
} ia32_s_cet_register;

#define IA32_PL0_SSP                                                 0x000006A4
#define IA32_PL1_SSP                                                 0x000006A5
#define IA32_PL2_SSP                                                 0x000006A6
#define IA32_PL3_SSP                                                 0x000006A7
#define IA32_INTERRUPT_SSPABLE_ADDR                                0x000006A8
#define IA32SC_DEADLINE                                            0x000006E0
#define IA32_PM_ENABLE                                               0x00000770
typedef union {
    struct {
        UINT64 hwp_enable : 1;
    };

    UINT64 flags;
} ia32_pm_enable_register;

#define IA32_HWP_CAPABILITIES                                        0x00000771
typedef union {
    struct {
        UINT64 highest_performance : 8;
        UINT64 guaranteed_performance : 8;
        UINT64 most_efficient_performance : 8;
        UINT64 lowest_performance : 8;
    };

    UINT64 flags;
} ia32_hwp_capabilities_register;

#define IA32_HWP_REQUEST_PKG                                         0x00000772
typedef union {
    struct {
        UINT64 minimum_performance : 8;
        UINT64 maximum_performance : 8;
        UINT64 desired_performance : 8;
        UINT64 energy_performance_preference : 8;
        UINT64 activity_window : 10;
    };

    UINT64 flags;
} ia32_hwp_request_pkg_register;

#define IA32_HWP_INTERRUPT                                           0x00000773
typedef union {
    struct {
        UINT64 en_guaranteed_performance_change : 1;
        UINT64 en_excursion_minimum : 1;
    };

    UINT64 flags;
} ia32_hwp_interrupt_register;

#define IA32_HWP_REQUEST                                             0x00000774
typedef union {
    struct {
        UINT64 minimum_performance : 8;
        UINT64 maximum_performance : 8;
        UINT64 desired_performance : 8;
        UINT64 energy_performance_preference : 8;
        UINT64 activity_window : 10;
        UINT64 package_control : 1;
    };

    UINT64 flags;
} ia32_hwp_request_register;

#define IA32_HWP_STATUS                                              0x00000777
typedef union {
    struct {
        UINT64 guaranteed_performance_change : 1;
        UINT64 reserved_1 : 1;
        UINT64 excursiono_minimum : 1;
    };

    UINT64 flags;
} ia32_hwp_status_register;

#define IA32_X2APIC_APICID                                           0x00000802
#define IA32_X2APIC_VERSION                                          0x00000803
#define IA32_X2APICPR                                              0x00000808
#define IA32_X2APIC_PPR                                              0x0000080A
#define IA32_X2APIC_EOI                                              0x0000080B
#define IA32_X2APIC_LDR                                              0x0000080D
#define IA32_X2APIC_SIVR                                             0x0000080F
/**
 * @defgroup ia32_x2apic_isr \
 *           IA32_X2APIC_ISR(n)
 * @{
 */
#define IA32_X2APIC_ISR0                                             0x00000810
#define IA32_X2APIC_ISR1                                             0x00000811
#define IA32_X2APIC_ISR2                                             0x00000812
#define IA32_X2APIC_ISR3                                             0x00000813
#define IA32_X2APIC_ISR4                                             0x00000814
#define IA32_X2APIC_ISR5                                             0x00000815
#define IA32_X2APIC_ISR6                                             0x00000816
#define IA32_X2APIC_ISR7                                             0x00000817
 /**
  * @}
  */

  /**
   * @defgroup ia32_x2apicmr \
   *           IA32_X2APICMR(n)
   * @{
   */
#define IA32_X2APICMR0                                             0x00000818
#define IA32_X2APICMR1                                             0x00000819
#define IA32_X2APICMR2                                             0x0000081A
#define IA32_X2APICMR3                                             0x0000081B
#define IA32_X2APICMR4                                             0x0000081C
#define IA32_X2APICMR5                                             0x0000081D
#define IA32_X2APICMR6                                             0x0000081E
#define IA32_X2APICMR7                                             0x0000081F
   /**
    * @}
    */

    /**
     * @defgroup ia32_x2apic_irr \
     *           IA32_X2APIC_IRR(n)
     * @{
     */
#define IA32_X2APIC_IRR0                                             0x00000820
#define IA32_X2APIC_IRR1                                             0x00000821
#define IA32_X2APIC_IRR2                                             0x00000822
#define IA32_X2APIC_IRR3                                             0x00000823
#define IA32_X2APIC_IRR4                                             0x00000824
#define IA32_X2APIC_IRR5                                             0x00000825
#define IA32_X2APIC_IRR6                                             0x00000826
#define IA32_X2APIC_IRR7                                             0x00000827
     /**
      * @}
      */

#define IA32_X2APIC_ESR                                              0x00000828
#define IA32_X2APIC_LVT_CMCI                                         0x0000082F
#define IA32_X2APIC_ICR                                              0x00000830
#define IA32_X2APIC_LVTIMER                                        0x00000832
#define IA32_X2APIC_LVTHERMAL                                      0x00000833
#define IA32_X2APIC_LVT_PMI                                          0x00000834
#define IA32_X2APIC_LVT_LINT0                                        0x00000835
#define IA32_X2APIC_LVT_LINT1                                        0x00000836
#define IA32_X2APIC_LVT_ERROR                                        0x00000837
#define IA32_X2APIC_INIT_COUNT                                       0x00000838
#define IA32_X2APIC_CUR_COUNT                                        0x00000839
#define IA32_X2APIC_DIV_CONF                                         0x0000083E
#define IA32_X2APIC_SELF_IPI                                         0x0000083F
#define IA32_DEBUG_INTERFACE                                         0x00000C80
typedef union {
    struct {
        UINT64 enable : 1;
        UINT64 reserved_1 : 29;
        UINT64 lock : 1;
        UINT64 debug_occurred : 1;
    };

    UINT64 flags;
} ia32_debug_interface_register;

#define IA32_L3_QOS_CFG                                              0x00000C81
typedef union {
    struct {
        UINT64 enable : 1;
    };

    UINT64 flags;
} ia32_l3_qos_cfg_register;

#define IA32_L2_QOS_CFG                                              0x00000C82
typedef union {
    struct {
        UINT64 enable : 1;
    };

    UINT64 flags;
} ia32_l2_qos_cfg_register;

#define IA32_QM_EVTSEL                                               0x00000C8D
typedef union {
    struct {
        UINT64 event_id : 8;
        UINT64 reserved_1 : 24;
        UINT64 resource_monitoring_id : 32;
    };

    UINT64 flags;
} ia32_qm_evtsel_register;

#define IA32_QM_CTR                                                  0x00000C8E
typedef union {
    struct {
        UINT64 resource_monitored_data : 62;
        UINT64 unavailable : 1;
        UINT64 error : 1;
    };

    UINT64 flags;
} ia32_qm_ctr_register;

#define IA32_PQR_ASSOC                                               0x00000C8F
typedef union {
    struct {
        UINT64 resource_monitoring_id : 32;
        UINT64 cos : 32;
    };

    UINT64 flags;
} ia32_pqr_assoc_register;

#define IA32_BNDCFGS                                                 0x00000D90
typedef union {
    struct {
        UINT64 enable : 1;
        UINT64 bnd_preserve : 1;
        UINT64 reserved_1 : 10;
        UINT64 bound_directory_base_address : 52;
    };

    UINT64 flags;
} ia32_bndcfgs_register;

#define IA32_XSS                                                     0x00000DA0
typedef union {
    struct {
        UINT64 reserved_1 : 8;
        UINT64 trace_packet_configuration_state : 1;
    };

    UINT64 flags;
} ia32_xss_register;

#define IA32_PKG_HDC_CTL                                             0x00000DB0
typedef union {
    struct {
        UINT64 hdc_pkg_enable : 1;
    };

    UINT64 flags;
} ia32_pkg_hdc_ctl_register;

#define IA32_PM_CTL1                                                 0x00000DB1
typedef union {
    struct {
        UINT64 hdc_allow_block : 1;
    };

    UINT64 flags;
} ia32_pm_ctl1_register;

#define IA32HREAD_STALL                                            0x00000DB2
typedef struct {
    UINT64 stall_cycle_cnt;
} ia32hread_stall_register;

#define IA32_EFER                                                    0xC0000080
typedef union {
    struct {
        UINT64 syscall_enable : 1;
        UINT64 reserved_1 : 7;
        UINT64 ia32e_mode_enable : 1;
        UINT64 reserved_2 : 1;
        UINT64 ia32e_mode_active : 1;
        UINT64 execute_disable_bit_enable : 1;
    };

    UINT64 flags;
} ia32_efer_register;

#define IA32_STAR                                                    0xC0000081
#define IA32_LSTAR                                                   0xC0000082
#define IA32_CSTAR                                                   0xC0000083
#define IA32_FMASK                                                   0xC0000084
#define IA32_FS_BASE                                                 0xC0000100
#define IA32_GS_BASE                                                 0xC0000101
#define IA32_KERNEL_GS_BASE                                          0xC0000102
#define IA32SC_AUX                                                 0xC0000103
typedef union {
    struct {
        UINT64 tsc_auxiliary_signature : 32;
    };

    UINT64 flags;
} ia32sc_aux_register;

/**
 * @}
 */

 /**
  * @defgroup paging \
  *           Paging
  * @{
  */
  /**
   * @defgroup paging_32 \
   *           32-Bit Paging
   * @{
   */
typedef union {
    struct {
        UINT32 present : 1;
        UINT32 write : 1;
        UINT32 supervisor : 1;
        UINT32 page_level_writehrough : 1;
        UINT32 page_level_cache_disable : 1;
        UINT32 accessed : 1;
        UINT32 dirty : 1;
        UINT32 large_page : 1;
        UINT32 global : 1;
        UINT32 ignored_1 : 3;
        UINT32 pat : 1;
        UINT32 page_frame_number_low : 8;
        UINT32 reserved_1 : 1;
        UINT32 page_frame_number_high : 10;
    };

    UINT32 flags;
} pde_4mb_32;

typedef union {
    struct {
        UINT32 present : 1;
        UINT32 write : 1;
        UINT32 supervisor : 1;
        UINT32 page_level_writehrough : 1;
        UINT32 page_level_cache_disable : 1;
        UINT32 accessed : 1;
        UINT32 ignored_1 : 1;
        UINT32 large_page : 1;
        UINT32 ignored_2 : 4;
        UINT32 page_frame_number : 20;
    };

    UINT32 flags;
} pde_32;

typedef union {
    struct {
        UINT32 present : 1;
        UINT32 write : 1;
        UINT32 supervisor : 1;
        UINT32 page_level_writehrough : 1;
        UINT32 page_level_cache_disable : 1;
        UINT32 accessed : 1;
        UINT32 dirty : 1;
        UINT32 pat : 1;
        UINT32 global : 1;
        UINT32 ignored_1 : 3;
        UINT32 page_frame_number : 20;
    };

    UINT32 flags;
} pte_32;

typedef union {
    struct {
        UINT32 present : 1;
        UINT32 write : 1;
        UINT32 supervisor : 1;
        UINT32 page_level_writehrough : 1;
        UINT32 page_level_cache_disable : 1;
        UINT32 accessed : 1;
        UINT32 dirty : 1;
        UINT32 large_page : 1;
        UINT32 global : 1;
        UINT32 ignored_1 : 3;
        UINT32 page_frame_number : 20;
    };

    UINT32 flags;
} pt_entry_32;

/**
 * @defgroup paging_structures_entry_count_32 \
 *           Paging structures entry counts
 * @{
 */
#define PDE_ENTRY_COUNT_32                                           0x00000400
#define PTE_ENTRY_COUNT_32                                           0x00000400
 /**
  * @}
  */

  /**
   * @}
   */

   /**
    * @defgroup paging_64 \
    *           64-Bit (4-Level) Paging
    * @{
    */
typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 reserved_1 : 1;
        UINT64 must_be_zero : 1;
        UINT64 ignored_1 : 3;
        UINT64 restart : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_2 : 4;
        UINT64 ignored_2 : 11;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pml4e_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 large_page : 1;
        UINT64 global : 1;
        UINT64 ignored_1 : 2;
        UINT64 restart : 1;
        UINT64 pat : 1;
        UINT64 reserved_1 : 17;
        UINT64 page_frame_number : 18;
        UINT64 reserved_2 : 4;
        UINT64 ignored_2 : 7;
        UINT64 protection_key : 4;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pdpte_1gb_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 reserved_1 : 1;
        UINT64 large_page : 1;
        UINT64 ignored_1 : 3;
        UINT64 restart : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_2 : 4;
        UINT64 ignored_2 : 11;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pdpte_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 large_page : 1;
        UINT64 global : 1;
        UINT64 ignored_1 : 2;
        UINT64 restart : 1;
        UINT64 pat : 1;
        UINT64 reserved_1 : 8;
        UINT64 page_frame_number : 27;
        UINT64 reserved_2 : 4;
        UINT64 ignored_2 : 7;
        UINT64 protection_key : 4;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pde_2mb_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 reserved_1 : 1;
        UINT64 large_page : 1;
        UINT64 ignored_1 : 3;
        UINT64 restart : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_2 : 4;
        UINT64 ignored_2 : 11;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pde_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 pat : 1;
        UINT64 global : 1;
        UINT64 ignored_1 : 2;
        UINT64 restart : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_1 : 4;
        UINT64 ignored_2 : 7;
        UINT64 protection_key : 4;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pte_64;

typedef union {
    struct {
        UINT64 present : 1;
        UINT64 write : 1;
        UINT64 supervisor : 1;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 large_page : 1;
        UINT64 global : 1;
        UINT64 ignored_1 : 2;
        UINT64 restart : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_1 : 4;
        UINT64 ignored_2 : 7;
        UINT64 protection_key : 4;
        UINT64 execute_disable : 1;
    };

    UINT64 flags;
} pt_entry_64;

/**
 * @defgroup paging_structures_entry_count_64 \
 *           Paging structures entry counts
 * @{
 */
#define PML4_ENTRY_COUNT_64                                          0x00000200
#define PDPTE_ENTRY_COUNT_64                                         0x00000200
#define PDE_ENTRY_COUNT_64                                           0x00000200
#define PTE_ENTRY_COUNT_64                                           0x00000200
 /**
  * @}
  */

  /**
   * @}
   */

   /**
    * @}
    */

typedef enum {
    invpcid_individual_address = 0x00000000,
    invpcid_single_context = 0x00000001,
    invpcid_all_context_with_globals = 0x00000002,
    invpcid_all_context = 0x00000003,
} invpcidype;

typedef union {
    struct {
        UINT64 pcid : 12;
        UINT64 reserved1 : 52;
        UINT64 linear_address : 64;
    };

    UINT64 flags;
} invpcid_descriptor;

/**
 * @defgroup segment_descriptors \
 *           Segment descriptors
 * @{
 */
#pragma pack(push, 1)
typedef struct {
    UINT16 limit;
    UINT32 base_address;
} segment_descriptor_register_32;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    UINT16 limit;
    UINT64 base_address;
} segment_descriptor_register_64;
#pragma pack(pop)

typedef union {
    struct {
        UINT32 reserved_1 : 8;
        UINT32 type : 4;
        UINT32 descriptorype : 1;
        UINT32 descriptor_privilege_level : 2;
        UINT32 present : 1;
        UINT32 reserved_2 : 4;
        UINT32 available_bit : 1;
        UINT32 long_mode : 1;
        UINT32 default_big : 1;
        UINT32 granularity : 1;
    };

    UINT32 flags;
} segment_access_rights;

typedef struct {
    UINT16 segment_limit_low;
    UINT16 base_address_low;
    union {
        struct {
            UINT32 base_address_middle : 8;
            UINT32 type : 4;
            UINT32 descriptorype : 1;
            UINT32 descriptor_privilege_level : 2;
            UINT32 present : 1;
            UINT32 segment_limit_high : 4;
            UINT32 available_bit : 1;
            UINT32 long_mode : 1;
            UINT32 default_big : 1;
            UINT32 granularity : 1;
            UINT32 base_address_high : 8;
        };

        UINT32 flags;
    };

} segment_descriptor_32;

typedef struct {
    UINT16 segment_limit_low;
    UINT16 base_address_low;
    union {
        struct {
            UINT32 base_address_middle : 8;
            UINT32 type : 4;
            UINT32 descriptorype : 1;
            UINT32 descriptor_privilege_level : 2;
            UINT32 present : 1;
            UINT32 segment_limit_high : 4;
            UINT32 available_bit : 1;
            UINT32 long_mode : 1;
            UINT32 default_big : 1;
            UINT32 granularity : 1;
            UINT32 base_address_high : 8;
        };

        UINT32 flags;
    };

    UINT32 base_address_upper;
    UINT32 must_be_zero;
} segment_descriptor_64;

typedef struct {
    UINT16 offset_low;
    UINT16 segment_selector;
    union {
        struct {
            UINT32 interrupt_stackable : 3;
            UINT32 must_be_zero_0 : 5;
            UINT32 type : 4;
            UINT32 must_be_zero_1 : 1;
            UINT32 descriptor_privilege_level : 2;
            UINT32 present : 1;
            UINT32 offset_middle : 16;
        };

        UINT32 flags;
    };

    UINT32 offset_high;
    UINT32 reserved;
} segment_descriptor_interrupt_gate_64;

#define SEGMENT_DESCRIPTORYPE_SYSTEM                               0x00000000
#define SEGMENT_DESCRIPTORYPE_CODE_OR_DATA                         0x00000001
/**
 * @defgroup segment_descriptor_code_and_dataype \
 *           Code- and Data-Segment Descriptor Types
 * @{
 */
#define SEGMENT_DESCRIPTORYPE_DATA_R                               0x00000000
#define SEGMENT_DESCRIPTORYPE_DATA_RA                              0x00000001
#define SEGMENT_DESCRIPTORYPE_DATA_RW                              0x00000002
#define SEGMENT_DESCRIPTORYPE_DATA_RWA                             0x00000003
#define SEGMENT_DESCRIPTORYPE_DATA_RE                              0x00000004
#define SEGMENT_DESCRIPTORYPE_DATA_REA                             0x00000005
#define SEGMENT_DESCRIPTORYPE_DATA_RWE                             0x00000006
#define SEGMENT_DESCRIPTORYPE_DATA_RWEA                            0x00000007
#define SEGMENT_DESCRIPTORYPE_CODE_E                               0x00000008
#define SEGMENT_DESCRIPTORYPE_CODE_EA                              0x00000009
#define SEGMENT_DESCRIPTORYPE_CODE_ER                              0x0000000A
#define SEGMENT_DESCRIPTORYPE_CODE_ERA                             0x0000000B
#define SEGMENT_DESCRIPTORYPE_CODE_EC                              0x0000000C
#define SEGMENT_DESCRIPTORYPE_CODE_ECA                             0x0000000D
#define SEGMENT_DESCRIPTORYPE_CODE_ERC                             0x0000000E
#define SEGMENT_DESCRIPTORYPE_CODE_ERCA                            0x0000000F
 /**
  * @}
  */

  /**
   * @defgroup segment_descriptor_systemype \
   *           System Descriptor Types
   * @{
   */
#define SEGMENT_DESCRIPTORYPE_RESERVED_1                           0x00000000
#define SEGMENT_DESCRIPTORYPESS_16_AVAILABLE                     0x00000001
#define SEGMENT_DESCRIPTORYPE_LDT                                  0x00000002
#define SEGMENT_DESCRIPTORYPESS_16_BUSY                          0x00000003
#define SEGMENT_DESCRIPTORYPE_CALL_GATE_16                         0x00000004
#define SEGMENT_DESCRIPTORYPEASK_GATE                            0x00000005
#define SEGMENT_DESCRIPTORYPE_INTERRUPT_GATE_16                    0x00000006
#define SEGMENT_DESCRIPTORYPERAP_GATE_16                         0x00000007
#define SEGMENT_DESCRIPTORYPE_RESERVED_2                           0x00000008
#define SEGMENT_DESCRIPTORYPESS_AVAILABLE                        0x00000009
#define SEGMENT_DESCRIPTORYPE_RESERVED_3                           0x0000000A
#define SEGMENT_DESCRIPTORYPESS_BUSY                             0x0000000B
#define SEGMENT_DESCRIPTORYPE_CALL_GATE                            0x0000000C
#define SEGMENT_DESCRIPTORYPE_RESERVED_4                           0x0000000D
#define SEGMENT_DESCRIPTORYPE_INTERRUPT_GATE                       0x0000000E
#define SEGMENT_DESCRIPTORYPERAP_GATE                            0x0000000F
   /**
    * @}
    */

typedef union {
    struct {
        UINT16 request_privilege_level : 2;
        UINT16 table_indicator : 1;
        UINT16 index : 13;
    };

    UINT16 flags;
} segment_selector;

/**
 * @}
 */

#pragma pack(push, 1)
typedef struct {
    UINT32 reserved_0;
    UINT64 rsp0;
    UINT64 rsp1;
    UINT64 rsp2;
    UINT64 reserved_1;
    UINT64 ist1;
    UINT64 ist2;
    UINT64 ist3;
    UINT64 ist4;
    UINT64 ist5;
    UINT64 ist6;
    UINT64 ist7;
    UINT64 reserved_2;
    UINT16 reserved_3;
    UINT16 io_map_base;
} task_state_segment_64;
#pragma pack(pop)

/**
 * @defgroup vmx \
 *           VMX
 * @{
 */
 /**
  * @{
  */
  /**
   * @defgroup vmx_basic_exit_reasons \
   *           VMX Basic Exit Reasons
   * @{
   */
#define VMX_EXIT_REASON_XCPT_OR_NMI                                  0x00000000
#define VMX_EXIT_REASON_EXT_INT                                      0x00000001
#define VMX_EXIT_REASONRIPLE_FAULT                                 0x00000002
#define VMX_EXIT_REASON_INIT_SIGNAL                                  0x00000003
#define VMX_EXIT_REASON_SIPI                                         0x00000004
#define VMX_EXIT_REASON_IO_SMI                                       0x00000005
#define VMX_EXIT_REASON_SMI                                          0x00000006
#define VMX_EXIT_REASON_INT_WINDOW                                   0x00000007
#define VMX_EXIT_REASON_NMI_WINDOW                                   0x00000008
#define VMX_EXIT_REASONASK_SWITCH                                  0x00000009
#define VMX_EXIT_REASON_CPUID                                        0x0000000A
#define VMX_EXIT_REASON_GETSEC                                       0x0000000B
#define VMX_EXIT_REASON_HLT                                          0x0000000C
#define VMX_EXIT_REASON_INVD                                         0x0000000D
#define VMX_EXIT_REASON_INVLPG                                       0x0000000E
#define VMX_EXIT_REASON_RDPMC                                        0x0000000F
#define VMX_EXIT_REASON_RDTSC                                        0x00000010
#define VMX_EXIT_REASON_RSM                                          0x00000011
#define VMX_EXIT_REASON_VMCALL                                       0x00000012
#define VMX_EXIT_REASON_VMCLEAR                                      0x00000013
#define VMX_EXIT_REASON_VMLAUNCH                                     0x00000014
#define VMX_EXIT_REASON_VMPTRLD                                      0x00000015
#define VMX_EXIT_REASON_VMPTRST                                      0x00000016
#define VMX_EXIT_REASON_VMREAD                                       0x00000017
#define VMX_EXIT_REASON_VMRESUME                                     0x00000018
#define VMX_EXIT_REASON_VMWRITE                                      0x00000019
#define VMX_EXIT_REASON_VMXOFF                                       0x0000001A
#define VMX_EXIT_REASON_VMXON                                        0x0000001B
#define VMX_EXIT_REASON_MOV_CRX                                      0x0000001C
#define VMX_EXIT_REASON_MOV_DRX                                      0x0000001D
#define VMX_EXIT_REASON_IO_INSTR                                     0x0000001E
#define VMX_EXIT_REASON_RDMSR                                        0x0000001F
#define VMX_EXIT_REASON_WRMSR                                        0x00000020
#define VMX_EXIT_REASON_ERR_INVALID_GUEST_STATE                      0x00000021
#define VMX_EXIT_REASON_ERR_MSR_LOAD                                 0x00000022
#define VMX_EXIT_REASON_MWAIT                                        0x00000024
#define VMX_EXIT_REASON_MTF                                          0x00000025
#define VMX_EXIT_REASON_MONITOR                                      0x00000027
#define VMX_EXIT_REASON_PAUSE                                        0x00000028
#define VMX_EXIT_REASON_ERR_MACHINE_CHECK                            0x00000029
#define VMX_EXIT_REASONPR_BELOWHRESHOLD                          0x0000002B
#define VMX_EXIT_REASON_APIC_ACCESS                                  0x0000002C
#define VMX_EXIT_REASON_VIRTUALIZED_EOI                              0x0000002D
#define VMX_EXIT_REASON_XDTR_ACCESS                                  0x0000002E
#define VMX_EXIT_REASONR_ACCESS                                    0x0000002F
#define VMX_EXIT_REASON_EPT_VIOLATION                                0x00000030
#define VMX_EXIT_REASON_EPT_MISCONFIG                                0x00000031
#define VMX_EXIT_REASON_INVEPT                                       0x00000032
#define VMX_EXIT_REASON_RDTSCP                                       0x00000033
#define VMX_EXIT_REASON_PREEMPTIMER                                0x00000034
#define VMX_EXIT_REASON_INVVPID                                      0x00000035
#define VMX_EXIT_REASON_WBINVD                                       0x00000036
#define VMX_EXIT_REASON_XSETBV                                       0x00000037
#define VMX_EXIT_REASON_APIC_WRITE                                   0x00000038
#define VMX_EXIT_REASON_RDRAND                                       0x00000039
#define VMX_EXIT_REASON_INVPCID                                      0x0000003A
#define VMX_EXIT_REASON_VMFUNC                                       0x0000003B
#define VMX_EXIT_REASON_ENCLS                                        0x0000003C
#define VMX_EXIT_REASON_RDSEED                                       0x0000003D
#define VMX_EXIT_REASON_PML_FULL                                     0x0000003E
#define VMX_EXIT_REASON_XSAVES                                       0x0000003F
#define VMX_EXIT_REASON_XRSTORS                                      0x00000040
#define VMX_EXIT_REASON_PCONFIG                                      0x00000041
#define VMX_EXIT_REASON_SPP_EVENT                                    0x00000042
#define VMX_EXIT_REASON_UMWAIT                                       0x00000043
#define VMX_EXIT_REASONPAUSE                                       0x00000044
#define VMX_EXIT_REASON_LOADIWKEY                                    0x00000045
#define VMX_EXIT_REASON_ENCLV                                        0x00000046
#define VMX_EXIT_REASON_ENQCMD                                       0x00000048
#define VMX_EXIT_REASON_ENQCMDS                                      0x00000049
#define VMX_EXIT_REASON_BUS_LOCK                                     0x0000004A
#define VMX_EXIT_REASON_INSTRUCTIONIMEOUT                          0x0000004B
#define VMX_EXIT_REASON_SEAMCALL                                     0x0000004C
#define VMX_EXIT_REASONDCALL                                       0x0000004D
#define VMX_EXIT_REASON_RDMSRLIST                                    0x0000004E
#define VMX_EXIT_REASON_WRMSRLIST                                    0x0000004F
   /**
    * @}
    */

    /**
     * @defgroup vmx_instruction_error_numbers \
     *           VM-Instruction Error Numbers
     * @{
     */
#define VMX_ERROR_VMCALL                                             0x00000001
#define VMX_ERROR_VMCLEAR_INVALID_PHYS_ADDR                          0x00000002
#define VMX_ERROR_VMCLEAR_INVALID_VMXON_PTR                          0x00000003
#define VMX_ERROR_VMLAUCH_NON_CLEAR_VMCS                             0x00000004
#define VMX_ERROR_VMRESUME_NON_LAUNCHED_VMCS                         0x00000005
#define VMX_ERROR_VMRESUME_CORRUPTED_VMCS                            0x00000006
#define VMX_ERROR_VMENTRY_INVALID_CONTROL_FIELDS                     0x00000007
#define VMX_ERROR_VMENTRY_INVALID_HOST_STATE                         0x00000008
#define VMX_ERROR_VMPTRLD_INVALID_PHYS_ADDR                          0x00000009
#define VMX_ERROR_VMPTRLD_VMXON_PTR                                  0x0000000A
#define VMX_ERROR_VMPTRLD_WRONG_VMCS_REVISION                        0x0000000B
#define VMX_ERROR_VMREAD_VMWRITE_INVALID_COMPONENT                   0x0000000C
#define VMX_ERROR_VMWRITE_READONLY_COMPONENT                         0x0000000D
#define VMX_ERROR_VMXON_IN_VMX_ROOT_OP                               0x0000000F
#define VMX_ERROR_VMENTRY_INVALID_VMCS_EXEC_PTR                      0x00000010
#define VMX_ERROR_VMENTRY_NON_LAUNCHED_EXEC_VMCS                     0x00000011
#define VMX_ERROR_VMENTRY_EXEC_VMCS_PTR                              0x00000012
#define VMX_ERROR_VMCALL_NON_CLEAR_VMCS                              0x00000013
#define VMX_ERROR_VMCALL_INVALID_VMEXIT_FIELDS                       0x00000014
#define VMX_ERROR_VMCALL_INVALID_MSEG_REVISION                       0x00000016
#define VMX_ERROR_VMXOFF_DUAL_MONITOR                                0x00000017
#define VMX_ERROR_VMCALL_INVALID_SMM_MONITOR                         0x00000018
#define VMX_ERROR_VMENTRY_INVALID_VM_EXEC_CTRL                       0x00000019
#define VMX_ERROR_VMENTRY_MOV_SS                                     0x0000001A
#define VMX_ERROR_INVEPTVPID_INVALID_OPERAND                         0x0000001C
     /**
      * @}
      */

      /**
       * @defgroup vmx_exceptions \
       *           Virtualization Exceptions
       * @{
       */
typedef struct {
    UINT32 reason;
    UINT32 exception_mask;
    UINT64 exit;
    UINT64 guest_linear_address;
    UINT64 guest_physical_address;
    UINT16 current_eptp_index;
} vmx_ve_except_info;

/**
 * @}
 */

 /**
  * @defgroup vmx_basic_exit_information \
  *           Basic VM-Exit Information
  * @{
  */
typedef union {
    struct {
        UINT64 breakpoint_condition : 4;
        UINT64 reserved_1 : 9;
        UINT64 debug_register_access_detected : 1;
        UINT64 single_instruction : 1;
    };

    UINT64 flags;
} vmx_exit_qualification_debug_exception;

typedef union {
    struct {
        UINT64 selector : 16;
        UINT64 reserved_1 : 14;
        UINT64 type : 2;
#define VMX_EXIT_QUALIFICATIONYPE_CALL                             0x00000000
#define VMX_EXIT_QUALIFICATIONYPE_IRET                             0x00000001
#define VMX_EXIT_QUALIFICATIONYPE_JMP                              0x00000002
#define VMX_EXIT_QUALIFICATIONYPE_IDT                              0x00000003
    };

    UINT64 flags;
} vmx_exit_qualificationask_switch;

typedef union {
    struct {
        UINT64 cr_number : 4;
#define VMX_EXIT_QUALIFICATION_REGISTER_CR0                          0x00000000
#define VMX_EXIT_QUALIFICATION_REGISTER_CR2                          0x00000002
#define VMX_EXIT_QUALIFICATION_REGISTER_CR3                          0x00000003
#define VMX_EXIT_QUALIFICATION_REGISTER_CR4                          0x00000004
#define VMX_EXIT_QUALIFICATION_REGISTER_CR8                          0x00000008
        UINT64 accessype : 2;
#define VMX_EXIT_QUALIFICATION_ACCESS_MOVO_CR                      0x00000000
#define VMX_EXIT_QUALIFICATION_ACCESS_MOV_FROM_CR                    0x00000001
#define VMX_EXIT_QUALIFICATION_ACCESS_CLTS                           0x00000002
#define VMX_EXIT_QUALIFICATION_ACCESS_LMSW                           0x00000003
        UINT64 lmsw_operandype : 1;
#define VMX_EXIT_QUALIFICATION_LMSW_OP_REGISTER                      0x00000000
#define VMX_EXIT_QUALIFICATION_LMSW_OP_MEMORY                        0x00000001
        UINT64 reserved_1 : 1;
        UINT64 gp_register : 4;
#define VMX_EXIT_QUALIFICATION_GENREG_RAX                            0x00000000
#define VMX_EXIT_QUALIFICATION_GENREG_RCX                            0x00000001
#define VMX_EXIT_QUALIFICATION_GENREG_RDX                            0x00000002
#define VMX_EXIT_QUALIFICATION_GENREG_RBX                            0x00000003
#define VMX_EXIT_QUALIFICATION_GENREG_RSP                            0x00000004
#define VMX_EXIT_QUALIFICATION_GENREG_RBP                            0x00000005
#define VMX_EXIT_QUALIFICATION_GENREG_RSI                            0x00000006
#define VMX_EXIT_QUALIFICATION_GENREG_RDI                            0x00000007
#define VMX_EXIT_QUALIFICATION_GENREG_R8                             0x00000008
#define VMX_EXIT_QUALIFICATION_GENREG_R9                             0x00000009
#define VMX_EXIT_QUALIFICATION_GENREG_R10                            0x0000000A
#define VMX_EXIT_QUALIFICATION_GENREG_R11                            0x0000000B
#define VMX_EXIT_QUALIFICATION_GENREG_R12                            0x0000000C
#define VMX_EXIT_QUALIFICATION_GENREG_R13                            0x0000000D
#define VMX_EXIT_QUALIFICATION_GENREG_R14                            0x0000000E
#define VMX_EXIT_QUALIFICATION_GENREG_R15                            0x0000000F
        UINT64 reserved_2 : 4;
        UINT64 lmsw_source_data : 16;
    };

    UINT64 flags;
} vmx_exit_qualification_cr_access;

typedef union {
    struct {
        UINT64 dr_number : 3;
#define VMX_EXIT_QUALIFICATION_REGISTER_DR0                          0x00000000
#define VMX_EXIT_QUALIFICATION_REGISTER_DR1                          0x00000001
#define VMX_EXIT_QUALIFICATION_REGISTER_DR2                          0x00000002
#define VMX_EXIT_QUALIFICATION_REGISTER_DR3                          0x00000003
#define VMX_EXIT_QUALIFICATION_REGISTER_DR6                          0x00000006
#define VMX_EXIT_QUALIFICATION_REGISTER_DR7                          0x00000007
        UINT64 reserved_1 : 1;
        UINT64 direction_of_access : 1;
#define VMX_EXIT_QUALIFICATION_DIRECTION_MOVO_DR                   0x00000000
#define VMX_EXIT_QUALIFICATION_DIRECTION_MOV_FROM_DR                 0x00000001
        UINT64 reserved_2 : 3;
        UINT64 gp_register : 4;
    };

    UINT64 flags;
} vmx_exit_qualification_dr_access;

typedef union {
    struct {
        UINT64 size_of_access : 3;
#define VMX_EXIT_QUALIFICATION_WIDTH_1B                              0x00000000
#define VMX_EXIT_QUALIFICATION_WIDTH_2B                              0x00000001
#define VMX_EXIT_QUALIFICATION_WIDTH_4B                              0x00000003
        UINT64 direction_of_access : 1;
#define VMX_EXIT_QUALIFICATION_DIRECTION_OUT                         0x00000000
#define VMX_EXIT_QUALIFICATION_DIRECTION_IN                          0x00000001
        UINT64 string_instruction : 1;
#define VMX_EXIT_QUALIFICATION_IS_STRING_NOT_STRING                  0x00000000
#define VMX_EXIT_QUALIFICATION_IS_STRING_STRING                      0x00000001
        UINT64 rep_prefixed : 1;
#define VMX_EXIT_QUALIFICATION_IS_REP_NOT_REP                        0x00000000
#define VMX_EXIT_QUALIFICATION_IS_REP_REP                            0x00000001
        UINT64 operand_encoding : 1;
#define VMX_EXIT_QUALIFICATION_ENCODING_DX                           0x00000000
#define VMX_EXIT_QUALIFICATION_ENCODING_IMM                          0x00000001
        UINT64 reserved_1 : 9;
        UINT64 port_number : 16;
    };

    UINT64 flags;
} vmx_exit_qualification_io_inst;

typedef union {
    struct {
        UINT64 page_offset : 12;
        UINT64 accessype : 4;
#define VMX_EXIT_QUALIFICATIONYPE_LINEAR_READ                      0x00000000
#define VMX_EXIT_QUALIFICATIONYPE_LINEAR_WRITE                     0x00000001
#define VMX_EXIT_QUALIFICATIONYPE_LINEAR_INSTR_FETCH               0x00000002
#define VMX_EXIT_QUALIFICATIONYPE_LINEAR_EVENT_DELIVERY            0x00000003
#define VMX_EXIT_QUALIFICATIONYPE_PHYSICAL_EVENT_DELIVERY          0x0000000A
#define VMX_EXIT_QUALIFICATIONYPE_PHYSICAL_INSTR                   0x0000000F
    };

    UINT64 flags;
} vmx_exit_qualification_apic_access;

typedef union {
    struct {
        UINT64 data_read : 1;
        UINT64 data_write : 1;
        UINT64 instruction_fetch : 1;
        UINT64 entry_present : 1;
        UINT64 entry_write : 1;
        UINT64 entry_execute : 1;
        UINT64 entry_execute_for_user_mode : 1;
        UINT64 valid_guest_linear_address : 1;
        UINT64 eptranslated_access : 1;
        UINT64 user_mode_linear_address : 1;
        UINT64 readable_writable_page : 1;
        UINT64 execute_disable_page : 1;
        UINT64 nmi_unblocking : 1;
        UINT64 shadow_stack_access : 1;
        UINT64 supervisor_shadow_stack : 1;
        UINT64 guest_paging_verification : 1;
        UINT64 asynchronouso_instruction : 1;
    };

    UINT64 flags;
} vmx_exit_qualification_ept_violation;

/**
 * @}
 */

 /**
  * @defgroup vmx_vmexit_instruction_information \
  *           Information for VM Exits Due to Instruction Execution
  * @{
  */
typedef union {
    struct {
        UINT64 reserved_1 : 7;
        UINT64 address_size : 3;
        UINT64 reserved_2 : 5;
        UINT64 segment_register : 3;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_ins_outs;

typedef union {
    struct {
        UINT64 scaling : 2;
        UINT64 reserved_1 : 5;
        UINT64 address_size : 3;
        UINT64 reserved_2 : 5;
        UINT64 segment_register : 3;
        UINT64 gp_register : 4;
        UINT64 gp_register_invalid : 1;
        UINT64 base_register : 4;
        UINT64 base_register_invalid : 1;
        UINT64 register_2 : 4;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_invalidate;

typedef union {
    struct {
        UINT64 scaling : 2;
        UINT64 reserved_1 : 5;
        UINT64 address_size : 3;
        UINT64 reserved_2 : 1;
        UINT64 operand_size : 1;
        UINT64 reserved_3 : 3;
        UINT64 segment_register : 3;
        UINT64 gp_register : 4;
        UINT64 gp_register_invalid : 1;
        UINT64 base_register : 4;
        UINT64 base_register_invalid : 1;
        UINT64 instruction_identity : 2;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_gdtr_idtr_access;

typedef union {
    struct {
        UINT64 scaling : 2;
        UINT64 reserved_1 : 1;
        UINT64 reg_1 : 4;
        UINT64 address_size : 3;
        UINT64 memory_register : 1;
        UINT64 reserved_2 : 4;
        UINT64 segment_register : 3;
        UINT64 gp_register : 4;
        UINT64 gp_register_invalid : 1;
        UINT64 base_register : 4;
        UINT64 base_register_invalid : 1;
        UINT64 instruction_identity : 2;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_ldtrr_access;

typedef union {
    struct {
        UINT64 reserved_1 : 3;
        UINT64 destination_register : 4;
        UINT64 reserved_2 : 4;
        UINT64 operand_size : 2;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_rdrand_rdseed;

typedef union {
    struct {
        UINT64 scaling : 2;
        UINT64 reserved_1 : 5;
        UINT64 address_size : 3;
        UINT64 reserved_2 : 5;
        UINT64 segment_register : 3;
        UINT64 gp_register : 4;
        UINT64 gp_register_invalid : 1;
        UINT64 base_register : 4;
        UINT64 base_register_invalid : 1;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_vmx_and_xsaves;

typedef union {
    struct {
        UINT64 scaling : 2;
        UINT64 reserved_1 : 1;
        UINT64 register_1 : 4;
        UINT64 address_size : 3;
        UINT64 memory_register : 1;
        UINT64 reserved_2 : 4;
        UINT64 segment_register : 3;
        UINT64 gp_register : 4;
        UINT64 gp_register_invalid : 1;
        UINT64 base_register : 4;
        UINT64 base_register_invalid : 1;
        UINT64 register_2 : 4;
    };

    UINT64 flags;
} vmx_vmexit_instruction_info_vmread_vmwrite;

/**
 * @}
 */

typedef union {
    struct {
        UINT32 type : 4;
        UINT32 descriptorype : 1;
        UINT32 descriptor_privilege_level : 2;
        UINT32 present : 1;
        UINT32 reserved_1 : 4;
        UINT32 available_bit : 1;
        UINT32 long_mode : 1;
        UINT32 default_big : 1;
        UINT32 granularity : 1;
        UINT32 unusable : 1;
    };

    UINT32 flags;
} vmx_segment_access_rights;

typedef union {
    struct {
        UINT32 blocking_by_sti : 1;
        UINT32 blocking_by_mov_ss : 1;
        UINT32 blocking_by_smi : 1;
        UINT32 blocking_by_nmi : 1;
        UINT32 enclave_interruption : 1;
    };

    UINT32 flags;
} vmx_interruptibility_state;

typedef enum {
    vmx_active = 0x00000000,
    vmx_hlt = 0x00000001,
    vmx_shutdown = 0x00000002,
    vmx_wait_for_sipi = 0x00000003,
} vmx_guest_activity_state;

typedef union {
    struct {
        UINT64 b0 : 1;
        UINT64 b1 : 1;
        UINT64 b2 : 1;
        UINT64 b3 : 1;
        UINT64 reserved_1 : 8;
        UINT64 enabled_breakpoint : 1;
        UINT64 reserved_2 : 1;
        UINT64 bs : 1;
        UINT64 reserved_3 : 1;
        UINT64 rtm : 1;
    };

    UINT64 flags;
} vmx_pending_debug_exceptions;

/**
 * @}
 */

typedef union {
    struct {
        UINT32 basic_exit_reason : 16;
        UINT32 always0 : 1;
        UINT32 reserved1 : 10;
        UINT32 enclave_mode : 1;
        UINT32 pending_mtf_vm_exit : 1;
        UINT32 vm_exit_from_vmx_root : 1;
        UINT32 reserved2 : 1;
        UINT32 vm_entry_failure : 1;
    };

    UINT32 flags;
} vmx_vmexit_reason;

typedef struct {
#define IO_BITMAP_A_MIN                                              0x00000000
#define IO_BITMAP_A_MAX                                              0x00007FFF
#define IO_BITMAP_B_MIN                                              0x00008000
#define IO_BITMAP_B_MAX                                              0x0000FFFF
    UINT8 io_a[4096];
    UINT8 io_b[4096];
} vmx_io_bitmap;

typedef struct {
#define MSR_ID_LOW_MIN                                               0x00000000
#define MSR_ID_LOW_MAX                                               0x00001FFF
#define MSR_ID_HIGH_MIN                                              0xC0000000
#define MSR_ID_HIGH_MAX                                              0xC0001FFF
    UINT8 rdmsr_low[1024];
    UINT8 rdmsr_high[1024];
    UINT8 wrmsr_low[1024];
    UINT8 wrmsr_high[1024];
} vmx_msr_bitmap;

/**
 * @defgroup ept \
 *           The extended page-table mechanism
 * @{
 */
typedef union {
    struct {
        UINT64 memoryype : 3;
        UINT64 page_walk_length : 3;
#define EPT_PAGE_WALK_LENGTH_4                                       0x00000003
        UINT64 enable_access_and_dirty_flags : 1;
        UINT64 enable_supervisor_shadow_stack_pages : 1;
        UINT64 reserved_1 : 4;
        UINT64 page_frame_number : 36;
    };

    UINT64 flags;
} eptp;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 reserved_1 : 5;
        UINT64 accessed : 1;
        UINT64 reserved_2 : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_3 : 1;
        UINT64 page_frame_number : 36;
    };

    UINT64 flags;
} epml4e;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 memoryype : 3;
        UINT64 ignore_pat : 1;
        UINT64 large_page : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_1 : 19;
        UINT64 page_frame_number : 18;
        UINT64 reserved_2 : 9;
        UINT64 verify_guest_paging : 1;
        UINT64 paging_write_access : 1;
        UINT64 reserved_3 : 1;
        UINT64 supervisor_shadow_stack : 1;
        UINT64 reserved_4 : 2;
        UINT64 suppress_ve : 1;
    };

    UINT64 flags;
} epdpte_1gb;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 reserved_1 : 5;
        UINT64 accessed : 1;
        UINT64 reserved_2 : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_3 : 1;
        UINT64 page_frame_number : 36;
    };

    UINT64 flags;
} epdpte;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 memoryype : 3;
        UINT64 ignore_pat : 1;
        UINT64 large_page : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_1 : 10;
        UINT64 page_frame_number : 27;
        UINT64 reserved_2 : 9;
        UINT64 verify_guest_paging : 1;
        UINT64 paging_write_access : 1;
        UINT64 reserved_3 : 1;
        UINT64 supervisor_shadow_stack : 1;
        UINT64 reserved_4 : 2;
        UINT64 suppress_ve : 1;
    };

    UINT64 flags;
} epde_2mb;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 reserved_1 : 5;
        UINT64 accessed : 1;
        UINT64 reserved_2 : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_3 : 1;
        UINT64 page_frame_number : 36;
    };

    UINT64 flags;
} epde;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 memoryype : 3;
        UINT64 ignore_pat : 1;
        UINT64 reserved_1 : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_2 : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_3 : 9;
        UINT64 verify_guest_paging : 1;
        UINT64 paging_write_access : 1;
        UINT64 reserved_4 : 1;
        UINT64 supervisor_shadow_stack : 1;
        UINT64 sub_page_write_permissions : 1;
        UINT64 reserved_5 : 1;
        UINT64 suppress_ve : 1;
    };

    UINT64 flags;
} epte;

typedef union {
    struct {
        UINT64 read_access : 1;
        UINT64 write_access : 1;
        UINT64 execute_access : 1;
        UINT64 memoryype : 3;
        UINT64 ignore_pat : 1;
        UINT64 large_page : 1;
        UINT64 accessed : 1;
        UINT64 dirty : 1;
        UINT64 user_mode_execute : 1;
        UINT64 reserved_1 : 1;
        UINT64 page_frame_number : 36;
        UINT64 reserved_2 : 15;
        UINT64 suppress_ve : 1;
    };

    UINT64 flags;
} ept_entry;

/**
 * @defgroup eptable_level \
 *           EPT Table level numbers
 * @{
 */
#define EPT_LEVEL_PML4E                                              0x00000003
#define EPT_LEVEL_PDPTE                                              0x00000002
#define EPT_LEVEL_PDE                                                0x00000001
#define EPT_LEVEL_PTE                                                0x00000000
 /**
  * @}
  */

  /**
   * @defgroup ept_entry_count \
   *           EPT Entry counts
   * @{
   */
#define EPML4_ENTRY_COUNT                                            0x00000200
#define EPDPTE_ENTRY_COUNT                                           0x00000200
#define EPDE_ENTRY_COUNT                                             0x00000200
#define EPTE_ENTRY_COUNT                                             0x00000200
   /**
    * @}
    */

    /**
     * @}
     */

typedef enum {
    invept_single_context = 0x00000001,
    invept_all_context = 0x00000002,
} inveptype;

typedef enum {
    invvpid_individual_address = 0x00000000,
    invvpid_single_context = 0x00000001,
    invvpid_all_context = 0x00000002,
    invvpid_single_context_retaining_globals = 0x00000003,
} invvpidype;

typedef struct {
    UINT64 ept_pointer;
    UINT64 reserved;
} invept_descriptor;

typedef struct {
    UINT16 vpid;
    UINT16 reserved1;
    UINT32 reserved2;
    UINT64 linear_address;
} invvpid_descriptor;

typedef union {
    struct {
        UINT64 reserved_1 : 3;
        UINT64 page_level_writehrough : 1;
        UINT64 page_level_cache_disable : 1;
        UINT64 reserved_2 : 7;
        UINT64 page_frame_number : 36;
    };

    UINT64 flags;
} hlatp;

typedef struct {
    struct {
        UINT32 revision_id : 31;
        UINT32 shadow_vmcs_indicator : 1;
    };

    UINT32 abort_indicator;
    UINT8 data[4088];
} vmcs;

typedef struct {
    struct {
        UINT32 revision_id : 31;
        UINT32 must_be_zero : 1;
    };

    UINT8 data[4092];
} vmxon;

/**
 * @defgroup vmcs_fields \
 *           VMCS (VM Control Structure)
 * @{
 */
typedef union {
    struct {
        UINT16 accessype : 1;
        UINT16 index : 9;
        UINT16 type : 2;
        UINT16 must_be_zero : 1;
        UINT16 width : 2;
    };

    UINT16 flags;
} vmcs_component_encoding;

/**
 * @defgroup vmcs_16_bit \
 *           16-Bit Fields
 * @{
 */
 /**
  * @defgroup vmcs_16_bit_control_fields \
  *           16-Bit Control Fields
  * @{
  */
#define VMCS_CTRL_VPID                                               0x00000000
#define VMCS_CTRL_POSTED_INTR_NOTIFY_VECTOR                          0x00000002
#define VMCS_CTRL_EPTP_INDEX                                         0x00000004
#define VMCS_CTRL_HLAT_PREFIX_SIZE                                   0x00000006
#define VMCS_CTRL_LAST_PID_PTR_INDEX                                 0x00000008
  /**
   * @}
   */

   /**
    * @defgroup vmcs_16_bit_guest_state_fields \
    *           16-Bit Guest-State Fields
    * @{
    */
#define VMCS_GUEST_ES_SEL                                            0x00000800
#define VMCS_GUEST_CS_SEL                                            0x00000802
#define VMCS_GUEST_SS_SEL                                            0x00000804
#define VMCS_GUEST_DS_SEL                                            0x00000806
#define VMCS_GUEST_FS_SEL                                            0x00000808
#define VMCS_GUEST_GS_SEL                                            0x0000080A
#define VMCS_GUEST_LDTR_SEL                                          0x0000080C
#define VMCS_GUESTR_SEL                                            0x0000080E
#define VMCS_GUEST_INTR_STATUS                                       0x00000810
#define VMCS_GUEST_PML_INDEX                                         0x00000812
#define VMCS_GUEST_UINV                                              0x00000814
    /**
     * @}
     */

     /**
      * @defgroup vmcs_16_bit_host_state_fields \
      *           16-Bit Host-State Fields
      * @{
      */
#define VMCS_HOST_ES_SEL                                             0x00000C00
#define VMCS_HOST_CS_SEL                                             0x00000C02
#define VMCS_HOST_SS_SEL                                             0x00000C04
#define VMCS_HOST_DS_SEL                                             0x00000C06
#define VMCS_HOST_FS_SEL                                             0x00000C08
#define VMCS_HOST_GS_SEL                                             0x00000C0A
#define VMCS_HOSTR_SEL                                             0x00000C0C
      /**
       * @}
       */

       /**
        * @}
        */

        /**
         * @defgroup vmcs_64_bit \
         *           64-Bit Fields
         * @{
         */
         /**
          * @defgroup vmcs_64_bit_control_fields \
          *           64-Bit Control Fields
          * @{
          */
#define VMCS_CTRL_IO_BITMAP_A                                        0x00002000
#define VMCS_CTRL_IO_BITMAP_B                                        0x00002002
#define VMCS_CTRL_MSR_BITMAP                                         0x00002004
#define VMCS_CTRL_VMEXIT_MSR_STORE                                   0x00002006
#define VMCS_CTRL_VMEXIT_MSR_LOAD                                    0x00002008
#define VMCS_CTRL_VMENTRY_MSR_LOAD                                   0x0000200A
#define VMCS_CTRL_EXEC_VMCS_PTR                                      0x0000200C
#define VMCS_CTRL_PML_ADDR                                           0x0000200E
#define VMCS_CTRLSC_OFFSET                                         0x00002010
#define VMCS_CTRL_VAPIC_PAGEADDR                                     0x00002012
#define VMCS_CTRL_APIC_ACCESSADDR                                    0x00002014
#define VMCS_CTRL_POSTED_INTR_DESC                                   0x00002016
#define VMCS_CTRL_VMFUNC_CTRLS                                       0x00002018
#define VMCS_CTRL_EPTP                                               0x0000201A
#define VMCS_CTRL_EOI_BITMAP_0                                       0x0000201C
#define VMCS_CTRL_EOI_BITMAP_1                                       0x0000201E
#define VMCS_CTRL_EOI_BITMAP_2                                       0x00002020
#define VMCS_CTRL_EOI_BITMAP_3                                       0x00002022
#define VMCS_CTRL_EPTP_LIST                                          0x00002024
#define VMCS_CTRL_VMREAD_BITMAP                                      0x00002026
#define VMCS_CTRL_VMWRITE_BITMAP                                     0x00002028
#define VMCS_CTRL_VIRTXCPT_INFO_ADDR                                 0x0000202A
#define VMCS_CTRL_XSS_EXITING_BITMAP                                 0x0000202C
#define VMCS_CTRL_ENCLS_EXITING_BITMAP                               0x0000202E
#define VMCS_CTRL_SPPABLE_POINTER                                  0x00002030
#define VMCS_CTRLSC_MULTIPLIER                                     0x00002032
#define VMCS_CTRL_PROC_EXEC3                                         0x00002034
#define VMCS_CTRL_ENCLV_EXITING_BITMAP                               0x00002036
#define VMCS_CTRL_LOW_PASID_DIR_ADDR                                 0x00002038
#define VMCS_CTRL_HIGH_PASID_DIR_ADDR                                0x0000203A
#define VMCS_CTRL_SHARED_EPTP                                        0x0000203C
#define VMCS_CTRL_PCONFIG_BITMAP                                     0x0000203E
#define VMCS_CTRL_HLATP                                              0x00002040
#define VMCS_CTRL_PID_PTRABLE                                      0x00002042
#define VMCS_CTRL_SECONDARY_EXIT                                     0x00002044
#define VMCS_CTRL_SPEC_CTRL_MASK                                     0x0000204A
#define VMCS_CTRL_SPEC_CTRL_SHADOW                                   0x0000204C
          /**
           * @}
           */

           /**
            * @defgroup vmcs_64_bit_read_only_data_fields \
            *           64-Bit Read-Only Data Field
            * @{
            */
#define VMCS_GUEST_PHYS_ADDR                                         0x00002400
            /**
             * @}
             */

             /**
              * @defgroup vmcs_64_bit_guest_state_fields \
              *           64-Bit Guest-State Fields
              * @{
              */
#define VMCS_GUEST_VMCS_LINK_PTR                                     0x00002800
#define VMCS_GUEST_DEBUGCTL                                          0x00002802
#define VMCS_GUEST_PAT                                               0x00002804
#define VMCS_GUEST_EFER                                              0x00002806
#define VMCS_GUEST_PERF_GLOBAL_CTRL                                  0x00002808
#define VMCS_GUEST_PDPTE0                                            0x0000280A
#define VMCS_GUEST_PDPTE1                                            0x0000280C
#define VMCS_GUEST_PDPTE2                                            0x0000280E
#define VMCS_GUEST_PDPTE3                                            0x00002810
#define VMCS_GUEST_BNDCFGS                                           0x00002812
#define VMCS_GUEST_RTIT_CTL                                          0x00002814
#define VMCS_GUEST_LBR_CTL                                           0x00002816
#define VMCS_GUEST_PKRS                                              0x00002818
              /**
               * @}
               */

               /**
                * @defgroup vmcs_64_bit_host_state_fields \
                *           64-Bit Host-State Fields
                * @{
                */
#define VMCS_HOST_PAT                                                0x00002C00
#define VMCS_HOST_EFER                                               0x00002C02
#define VMCS_HOST_PERF_GLOBAL_CTRL                                   0x00002C04
#define VMCS_HOST_PKRS                                               0x00002C06
                /**
                 * @}
                 */

                 /**
                  * @}
                  */

                  /**
                   * @defgroup vmcs_32_bit \
                   *           32-Bit Fields
                   * @{
                   */
                   /**
                    * @defgroup vmcs_32_bit_control_fields \
                    *           32-Bit Control Fields
                    * @{
                    */
#define VMCS_CTRL_PIN_EXEC                                           0x00004000
#define VMCS_CTRL_PROC_EXEC                                          0x00004002
#define VMCS_CTRL_EXCEPTION_BITMAP                                   0x00004004
#define VMCS_CTRL_PAGEFAULT_ERROR_MASK                               0x00004006
#define VMCS_CTRL_PAGEFAULT_ERROR_MATCH                              0x00004008
#define VMCS_CTRL_CR3ARGET_COUNT                                   0x0000400A
#define VMCS_CTRL_PRIMARY_EXIT                                       0x0000400C
#define VMCS_CTRL_EXIT_MSR_STORE_COUNT                               0x0000400E
#define VMCS_CTRL_EXIT_MSR_LOAD_COUNT                                0x00004010
#define VMCS_CTRL_ENTRY                                              0x00004012
#define VMCS_CTRL_ENTRY_MSR_LOAD_COUNT                               0x00004014
#define VMCS_CTRL_ENTRY_INTERRUPTION_INFO                            0x00004016
#define VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE                            0x00004018
#define VMCS_CTRL_ENTRY_INSTR_LENGTH                                 0x0000401A
#define VMCS_CTRLPRHRESHOLD                                      0x0000401C
#define VMCS_CTRL_PROC_EXEC2                                         0x0000401E
#define VMCS_CTRL_PLE_GAP                                            0x00004020
#define VMCS_CTRL_PLE_WINDOW                                         0x00004022
                    /**
                     * @}
                     */

                     /**
                      * @defgroup vmcs_32_bit_read_only_data_fields \
                      *           32-Bit Read-Only Data Fields
                      * @{
                      */
#define VMCS_VM_INSTR_ERROR                                          0x00004400
#define VMCS_EXIT_REASON                                             0x00004402
#define VMCS_EXIT_INTERRUPTION_INFO                                  0x00004404
#define VMCS_EXIT_INTERRUPTION_ERROR_CODE                            0x00004406
#define VMCS_IDT_VECTORING_INFO                                      0x00004408
#define VMCS_IDT_VECTORING_ERROR_CODE                                0x0000440A
#define VMCS_EXIT_INSTR_LENGTH                                       0x0000440C
#define VMCS_EXIT_INSTR_INFO                                         0x0000440E
                      /**
                       * @}
                       */

                       /**
                        * @defgroup vmcs_32_bit_guest_state_fields \
                        *           32-Bit Guest-State Fields
                        * @{
                        */
#define VMCS_GUEST_ES_LIMIT                                          0x00004800
#define VMCS_GUEST_CS_LIMIT                                          0x00004802
#define VMCS_GUEST_SS_LIMIT                                          0x00004804
#define VMCS_GUEST_DS_LIMIT                                          0x00004806
#define VMCS_GUEST_FS_LIMIT                                          0x00004808
#define VMCS_GUEST_GS_LIMIT                                          0x0000480A
#define VMCS_GUEST_LDTR_LIMIT                                        0x0000480C
#define VMCS_GUESTR_LIMIT                                          0x0000480E
#define VMCS_GUEST_GDTR_LIMIT                                        0x00004810
#define VMCS_GUEST_IDTR_LIMIT                                        0x00004812
#define VMCS_GUEST_ES_ACCESS_RIGHTS                                  0x00004814
#define VMCS_GUEST_CS_ACCESS_RIGHTS                                  0x00004816
#define VMCS_GUEST_SS_ACCESS_RIGHTS                                  0x00004818
#define VMCS_GUEST_DS_ACCESS_RIGHTS                                  0x0000481A
#define VMCS_GUEST_FS_ACCESS_RIGHTS                                  0x0000481C
#define VMCS_GUEST_GS_ACCESS_RIGHTS                                  0x0000481E
#define VMCS_GUEST_LDTR_ACCESS_RIGHTS                                0x00004820
#define VMCS_GUESTR_ACCESS_RIGHTS                                  0x00004822
#define VMCS_GUEST_INTERRUPTIBILITY_STATE                            0x00004824
#define VMCS_GUEST_ACTIVITY_STATE                                    0x00004826
#define VMCS_GUEST_SMBASE                                            0x00004828
#define VMCS_GUEST_SYSENTER_CS                                       0x0000482A
#define VMCS_GUEST_PREEMPTIMER_VALUE                               0x0000482E
                        /**
                         * @}
                         */

                         /**
                          * @defgroup vmcs_32_bit_host_state_fields \
                          *           32-Bit Host-State Field
                          * @{
                          */
#define VMCS_HOST_SYSENTER_CS                                        0x00004C00
                          /**
                           * @}
                           */

                           /**
                            * @}
                            */

                            /**
                             * @defgroup vmcs_natural_width \
                             *           Natural-Width Fields
                             * @{
                             */
                             /**
                              * @defgroup vmcs_natural_width_control_fields \
                              *           Natural-Width Control Fields
                              * @{
                              */
#define VMCS_CTRL_CR0_MASK                                           0x00006000
#define VMCS_CTRL_CR4_MASK                                           0x00006002
#define VMCS_CTRL_CR0_READ_SHADOW                                    0x00006004
#define VMCS_CTRL_CR4_READ_SHADOW                                    0x00006006
#define VMCS_CTRL_CR3ARGET_VAL0                                    0x00006008
#define VMCS_CTRL_CR3ARGET_VAL1                                    0x0000600A
#define VMCS_CTRL_CR3ARGET_VAL2                                    0x0000600C
#define VMCS_CTRL_CR3ARGET_VAL3                                    0x0000600E
                              /**
                               * @}
                               */

                               /**
                                * @defgroup vmcs_natural_width_read_only_data_fields \
                                *           Natural-Width Read-Only Data Fields
                                * @{
                                */
#define VMCS_EXIT_QUALIFICATION                                      0x00006400
#define VMCS_IO_RCX                                                  0x00006402
#define VMCS_IO_RSI                                                  0x00006404
#define VMCS_IO_RDI                                                  0x00006406
#define VMCS_IO_RIP                                                  0x00006408
#define VMCS_EXIT_GUEST_LINEAR_ADDR                                  0x0000640A
                                /**
                                 * @}
                                 */

                                 /**
                                  * @defgroup vmcs_natural_width_guest_state_fields \
                                  *           Natural-Width Guest-State Fields
                                  * @{
                                  */
#define VMCS_GUEST_CR0                                               0x00006800
#define VMCS_GUEST_CR3                                               0x00006802
#define VMCS_GUEST_CR4                                               0x00006804
#define VMCS_GUEST_ES_BASE                                           0x00006806
#define VMCS_GUEST_CS_BASE                                           0x00006808
#define VMCS_GUEST_SS_BASE                                           0x0000680A
#define VMCS_GUEST_DS_BASE                                           0x0000680C
#define VMCS_GUEST_FS_BASE                                           0x0000680E
#define VMCS_GUEST_GS_BASE                                           0x00006810
#define VMCS_GUEST_LDTR_BASE                                         0x00006812
#define VMCS_GUESTR_BASE                                           0x00006814
#define VMCS_GUEST_GDTR_BASE                                         0x00006816
#define VMCS_GUEST_IDTR_BASE                                         0x00006818
#define VMCS_GUEST_DR7                                               0x0000681A
#define VMCS_GUEST_RSP                                               0x0000681C
#define VMCS_GUEST_RIP                                               0x0000681E
#define VMCS_GUEST_RFLAGS                                            0x00006820
#define VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS                          0x00006822
#define VMCS_GUEST_SYSENTER_ESP                                      0x00006824
#define VMCS_GUEST_SYSENTER_EIP                                      0x00006826
#define VMCS_GUEST_S_CET                                             0x00006828
#define VMCS_GUEST_SSP                                               0x0000682A
#define VMCS_GUEST_INTERRUPT_SSPABLE_ADDR                          0x0000682C
                                  /**
                                   * @}
                                   */

                                   /**
                                    * @defgroup vmcs_natural_width_host_state_fields \
                                    *           Natural-Width Host-State Fields
                                    * @{
                                    */
#define VMCS_HOST_CR0                                                0x00006C00
#define VMCS_HOST_CR3                                                0x00006C02
#define VMCS_HOST_CR4                                                0x00006C04
#define VMCS_HOST_FS_BASE                                            0x00006C06
#define VMCS_HOST_GS_BASE                                            0x00006C08
#define VMCS_HOSTR_BASE                                            0x00006C0A
#define VMCS_HOST_GDTR_BASE                                          0x00006C0C
#define VMCS_HOST_IDTR_BASE                                          0x00006C0E
#define VMCS_HOST_SYSENTER_ESP                                       0x00006C10
#define VMCS_HOST_SYSENTER_EIP                                       0x00006C12
#define VMCS_HOST_RSP                                                0x00006C14
#define VMCS_HOST_RIP                                                0x00006C16
#define VMCS_HOST_S_CET                                              0x00006C18
#define VMCS_HOST_SSP                                                0x00006C1A
#define VMCS_HOST_INTERRUPT_SSPABLE_ADDR                           0x00006C1C
                                    /**
                                     * @}
                                     */

                                     /**
                                      * @}
                                      */

                                      /**
                                       * @}
                                       */

typedef enum {
    external_interrupt = 0x00000000,
    non_maskable_interrupt = 0x00000002,
    hardware_exception = 0x00000003,
    software_interrupt = 0x00000004,
    privileged_software_exception = 0x00000005,
    software_exception = 0x00000006,
    other_event = 0x00000007,
} interruptionype;

typedef union {
    struct {
        UINT32 vector : 8;
        UINT32 interruptionype : 3;
        UINT32 deliver_error_code : 1;
        UINT32 reserved_1 : 19;
        UINT32 valid : 1;
    };

    UINT32 flags;
} vmentry_interrupt_info;

typedef union {
    struct {
        UINT32 vector : 8;
        UINT32 interruptionype : 3;
        UINT32 error_code_valid : 1;
        UINT32 nmi_unblocking : 1;
        UINT32 reserved_1 : 18;
        UINT32 valid : 1;
    };

    UINT32 flags;
} vmexit_interrupt_info;

/**
 * @}
 */

 /**
  * @defgroup apic \
  *           Advanced Programmable Interrupt Controller (APIC)
  * @{
  */
#define APIC_BASE                                                    0xFEE00000
#define APIC_ID                                                      0x00000020
#define APIC_VERSION                                                 0x00000030
#define APICPR                                                     0x00000080
#define APIC_APR                                                     0x00000090
#define APIC_PPR                                                     0x000000A0
#define APIC_EOI                                                     0x000000B0
#define APIC_REMOTE_READ                                             0x000000C0
#define APIC_LOGICAL_DESTINATION                                     0x000000D0
#define APIC_DESTINATION_FORMAT                                      0x000000E0
#define APIC_SIV                                                     0x000000F0
#define APIC_ISR_31_0                                                0x00000100
#define APIC_ISR_63_32                                               0x00000110
#define APIC_ISR_95_64                                               0x00000120
#define APIC_ISR_127_96                                              0x00000130
#define APIC_ISR_159_128                                             0x00000140
#define APIC_ISR_191_160                                             0x00000150
#define APIC_ISR_223_192                                             0x00000160
#define APIC_ISR_255_224                                             0x00000170
#define APICMR_31_0                                                0x00000180
#define APICMR_63_32                                               0x00000190
#define APICMR_95_64                                               0x000001A0
#define APICMR_127_96                                              0x000001B0
#define APICMR_159_128                                             0x000001C0
#define APICMR_191_160                                             0x000001D0
#define APICMR_223_192                                             0x000001E0
#define APICMR_255_224                                             0x000001F0
#define APIC_IRR_31_0                                                0x00000200
#define APIC_IRR_63_32                                               0x00000210
#define APIC_IRR_95_64                                               0x00000220
#define APIC_IRR_127_96                                              0x00000230
#define APIC_IRR_159_128                                             0x00000240
#define APIC_IRR_191_160                                             0x00000250
#define APIC_IRR_223_192                                             0x00000260
#define APIC_IRR_255_224                                             0x00000270
#define APIC_ERROR_STATUS                                            0x00000280
#define APIC_CMCI                                                    0x000002F0
#define APIC_ICR_0_31                                                0x00000300
#define APIC_ICR_32_63                                               0x00000310
#define APIC_LVTIMER                                               0x00000320
#define APIC_LVTHERMAL_SENSOR                                      0x00000330
#define APIC_LVT_PERFORMANCE_MONITORING_COUNTERS                     0x00000340
#define APIC_LVT_LINT0                                               0x00000350
#define APIC_LVT_LINT1                                               0x00000360
#define APIC_LVT_ERROR                                               0x00000370
#define APIC_INITIAL_COUNT                                           0x00000380
#define APIC_CURRENT_COUNT                                           0x00000390
#define APIC_DIVIDE_CONFIGURATION                                    0x000003E0
  /**
   * @}
   */

typedef union {
    struct {
        UINT32 carry_flag : 1;
        UINT32 read_as_1 : 1;
        UINT32 parity_flag : 1;
        UINT32 reserved_1 : 1;
        UINT32 auxiliary_carry_flag : 1;
        UINT32 reserved_2 : 1;
        UINT32 zero_flag : 1;
        UINT32 sign_flag : 1;
        UINT32 trap_flag : 1;
        UINT32 interrupt_enable_flag : 1;
        UINT32 direction_flag : 1;
        UINT32 overflow_flag : 1;
        UINT32 io_privilege_level : 2;
        UINT32 nestedask_flag : 1;
        UINT32 reserved_3 : 1;
        UINT32 resume_flag : 1;
        UINT32 virtual_8086_mode_flag : 1;
        UINT32 alignment_check_flag : 1;
        UINT32 virtual_interrupt_flag : 1;
        UINT32 virtual_interrupt_pending_flag : 1;
        UINT32 identification_flag : 1;
    };

    UINT32 flags;
} efl;

typedef union {
    struct {
        UINT64 carry_flag : 1;
        UINT64 read_as_1 : 1;
        UINT64 parity_flag : 1;
        UINT64 reserved_1 : 1;
        UINT64 auxiliary_carry_flag : 1;
        UINT64 reserved_2 : 1;
        UINT64 zero_flag : 1;
        UINT64 sign_flag : 1;
        UINT64 trap_flag : 1;
        UINT64 interrupt_enable_flag : 1;
        UINT64 direction_flag : 1;
        UINT64 overflow_flag : 1;
        UINT64 io_privilege_level : 2;
        UINT64 nestedask_flag : 1;
        UINT64 reserved_3 : 1;
        UINT64 resume_flag : 1;
        UINT64 virtual_8086_mode_flag : 1;
        UINT64 alignment_check_flag : 1;
        UINT64 virtual_interrupt_flag : 1;
        UINT64 virtual_interrupt_pending_flag : 1;
        UINT64 identification_flag : 1;
    };

    UINT64 flags;
} rfl;

/**
 * @defgroup exceptions \
 *           Exceptions
 * @{
 */
typedef union {
    struct {
        UINT32 cpec : 15;
        UINT32 encl : 1;
    };

    UINT32 flags;
} control_protection_exception;

typedef enum {
    divide_error = 0x00000000,
    debug = 0x00000001,
    nmi = 0x00000002,
    breakpoint = 0x00000003,
    overflow = 0x00000004,
    bound_range_exceeded = 0x00000005,
    invalid_opcode = 0x00000006,
    device_not_available = 0x00000007,
    double_fault = 0x00000008,
    coprocessor_segment_overrun = 0x00000009,
    invalidss = 0x0000000A,
    segment_not_present = 0x0000000B,
    stack_segment_fault = 0x0000000C,
    general_protection = 0x0000000D,
    page_fault = 0x0000000E,
    x87_floating_point_error = 0x00000010,
    alignment_check = 0x00000011,
    machine_check = 0x00000012,
    simd_floating_point_error = 0x00000013,
    virtualization_exception = 0x00000014,
    control_protection = 0x00000015,
} exception_vector;

typedef union {
    struct {
        UINT32 external_event : 1;
        UINT32 descriptor_location : 1;
        UINT32 gdt_ldt : 1;
        UINT32 index : 13;
    };

    UINT32 flags;
} exception_error_code;

typedef union {
    struct {
        UINT32 present : 1;
        UINT32 write : 1;
        UINT32 user_mode_access : 1;
        UINT32 reserved_bit_violation : 1;
        UINT32 execute : 1;
        UINT32 protection_key_violation : 1;
        UINT32 shadow_stack : 1;
        UINT32 hlat : 1;
        UINT32 reserved_1 : 7;
        UINT32 sgx : 1;
    };

    UINT32 flags;
} page_fault_exception;

/**
 * @}
 */

 /**
  * @defgroup memoryype \
  *           Memory caching type
  * @{
  */
#define MEMORYYPE_UC                                               0x00000000
#define MEMORYYPE_WC                                               0x00000001
#define MEMORYYPE_WT                                               0x00000004
#define MEMORYYPE_WP                                               0x00000005
#define MEMORYYPE_WB                                               0x00000006
#define MEMORYYPE_UC_MINUS                                         0x00000007
#define MEMORYYPE_INVALID                                          0x000000FF
  /**
   * @}
   */

   /**
    * @defgroup vtd \
    *           VTD
    * @{
    */
typedef struct {
    union {
        struct {
            UINT64 present : 1;
            UINT64 reserved_1 : 11;
            UINT64 contextable_pointer : 52;
        };

        UINT64 flags;
    } lower64;

    union {
        struct {
            UINT64 reserved : 64;
        };

        UINT64 flags;
    } upper64;

} vtd_root_entry;

typedef struct {
    union {
        struct {
            UINT64 present : 1;
            UINT64 fault_processing_disable : 1;
            UINT64 translationype : 2;
            UINT64 reserved_1 : 8;
            UINT64 second_level_pageranslation_pointer : 52;
        };

        UINT64 flags;
    } lower64;

    union {
        struct {
            UINT64 address_width : 3;
            UINT64 ignored : 4;
            UINT64 reserved_1 : 1;
            UINT64 domain_identifier : 10;
        };

        UINT64 flags;
    } upper64;

} vtd_context_entry;

/**
 * @defgroup vtd_entry_count \
 *           Table entry counts
 * @{
 */
#define VTD_ROOT_ENTRY_COUNT                                         0x00000100
#define VTD_CONTEXT_ENTRY_COUNT                                      0x00000100
 /**
  * @}
  */

#define VTD_VERSION                                                  0x00000000
typedef union {
    struct {
        UINT32 minor : 4;
        UINT32 major : 4;
    };

    UINT32 flags;
} vtd_version_register;

#define VTD_CAPABILITY                                               0x00000008
typedef union {
    struct {
        UINT64 number_of_domains_supported : 3;
        UINT64 advanced_fault_logging : 1;
        UINT64 required_write_buffer_flushing : 1;
        UINT64 protected_low_memory_region : 1;
        UINT64 protected_high_memory_region : 1;
        UINT64 caching_mode : 1;
        UINT64 supported_adjusted_guest_address_widths : 5;
        UINT64 reserved_1 : 3;
        UINT64 maximum_guest_address_width : 6;
        UINT64 zero_length_read : 1;
        UINT64 deprecated : 1;
        UINT64 fault_recording_register_offset : 10;
        UINT64 second_level_large_page_support : 4;
        UINT64 reserved_2 : 1;
        UINT64 page_selective_invalidation : 1;
        UINT64 number_of_fault_recording_registers : 8;
        UINT64 maximum_address_mask_value : 6;
        UINT64 write_draining : 1;
        UINT64 read_draining : 1;
        UINT64 first_level_1gbyte_page_support : 1;
        UINT64 reserved_3 : 2;
        UINT64 posted_interrupts_support : 1;
        UINT64 first_level_5level_paging_support : 1;
        UINT64 reserved_4 : 1;
        UINT64 enhanced_set_interrupt_remapable_pointer_support : 1;
        UINT64 enhanced_set_rootable_pointer_support : 1;
    };

    UINT64 flags;
} vtd_capability_register;

#define VTD_EXTENDED_CAPABILITY                                      0x00000010
typedef union {
    struct {
        UINT64 page_walk_coherency : 1;
        UINT64 queued_invalidation_support : 1;
        UINT64 devicelb_support : 1;
        UINT64 interrupt_remapping_support : 1;
        UINT64 extended_interrupt_mode : 1;
        UINT64 deprecated1 : 1;
        UINT64 passhrough : 1;
        UINT64 snoop_control : 1;
        UINT64 iotlb_register_offset : 10;
        UINT64 reserved_1 : 2;
        UINT64 maximum_handle_mask_value : 4;
        UINT64 deprecated2 : 1;
        UINT64 memoryype_support : 1;
        UINT64 nestedranslation_support : 1;
        UINT64 reserved_2 : 1;
        UINT64 deprecated3 : 1;
        UINT64 page_request_support : 1;
        UINT64 execute_request_support : 1;
        UINT64 reserved_3 : 2;
        UINT64 no_write_flag_support : 1;
        UINT64 extended_accessed_flag_support : 1;
        UINT64 pasid_size_supported : 5;
        UINT64 process_address_space_id_support : 1;
        UINT64 devicelb_invalidationhrottle : 1;
        UINT64 page_request_drain_support : 1;
        UINT64 scalable_moderanslation_support : 1;
        UINT64 virtual_command_support : 1;
        UINT64 second_level_accessed_dirty_support : 1;
        UINT64 second_levelranslation_support : 1;
        UINT64 first_levelranslation_support : 1;
        UINT64 scalable_mode_page_walk_coherency : 1;
        UINT64 rid_pasid_support : 1;
        UINT64 reserved_4 : 2;
        UINT64 abort_dma_mode_support : 1;
        UINT64 rid_priv_support : 1;
    };

    UINT64 flags;
} vtd_extended_capability_register;

#define VTD_GLOBAL_COMMAND                                           0x00000018
typedef union {
    struct {
        UINT32 reserved_1 : 23;
        UINT32 compatibility_format_interrupt : 1;
        UINT32 set_interrupt_remapable_pointer : 1;
        UINT32 interrupt_remapping_enable : 1;
        UINT32 queued_invalidation_enable : 1;
        UINT32 write_buffer_flush : 1;
        UINT32 enable_advanced_fault_logging : 1;
        UINT32 set_fault_log : 1;
        UINT32 set_rootable_pointer : 1;
        UINT32 translation_enable : 1;
    };

    UINT32 flags;
} vtd_global_command_register;

#define VTD_GLOBAL_STATUS                                            0x0000001C
typedef union {
    struct {
        UINT32 reserved_1 : 23;
        UINT32 compatibility_format_interrupt_status : 1;
        UINT32 interrupt_remappingable_pointer_status : 1;
        UINT32 interrupt_remapping_enable_status : 1;
        UINT32 queued_invalidation_enable_status : 1;
        UINT32 write_buffer_flush_status : 1;
        UINT32 advanced_fault_logging_status : 1;
        UINT32 fault_log_status : 1;
        UINT32 rootable_pointer_status : 1;
        UINT32 translation_enable_status : 1;
    };

    UINT32 flags;
} vtd_global_status_register;

#define VTD_ROOTABLE_ADDRESS                                       0x00000020
typedef union {
    struct {
        UINT64 reserved_1 : 10;
        UINT64 translationable_mode : 2;
        UINT64 rootable_address : 52;
    };

    UINT64 flags;
} vtd_rootable_address_register;

#define VTD_CONTEXT_COMMAND                                          0x00000028
typedef union {
    struct {
        UINT64 domain_id : 16;
        UINT64 source_id : 16;
        UINT64 function_mask : 2;
        UINT64 reserved_1 : 25;
        UINT64 context_actual_invalidation_granularity : 2;
        UINT64 context_invalidation_request_granularity : 2;
        UINT64 invalidate_context_cache : 1;
    };

    UINT64 flags;
} vtd_context_command_register;

#define VTD_INVALIDATE_ADDRESS                                       0x00000000
typedef union {
    struct {
        UINT64 address_mask : 6;
        UINT64 invalidation_hint : 1;
        UINT64 reserved_1 : 5;
        UINT64 page_address : 52;
    };

    UINT64 flags;
} vtd_invalidate_address_register;

#define VTD_IOTLB_INVALIDATE                                         0x00000008
typedef union {
    struct {
        UINT64 reserved_1 : 32;
        UINT64 domain_id : 16;
        UINT64 drain_writes : 1;
        UINT64 drain_reads : 1;
        UINT64 reserved_2 : 7;
        UINT64 iotlb_actual_invalidation_granularity : 2;
        UINT64 reserved_3 : 1;
        UINT64 iotlb_invalidation_request_granularity : 2;
        UINT64 reserved_4 : 1;
        UINT64 invalidate_iotlb : 1;
    };

    UINT64 flags;
} vtd_iotlb_invalidate_register;

/**
 * @}
 */

typedef union {
    struct {
        UINT64 x87 : 1;
        UINT64 sse : 1;
        UINT64 avx : 1;
        UINT64 bndreg : 1;
        UINT64 bndcsr : 1;
        UINT64 opmask : 1;
        UINT64 zmm_hi256 : 1;
        UINT64 zmm_hi16 : 1;
        UINT64 reserved_1 : 1;
        UINT64 pkru : 1;
    };

    UINT64 flags;
} xcr0;

/**
 * @}
 */

#if defined(_MSC_EXTENSIONS)
#pragma warning(pop)
#endif
