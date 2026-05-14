#include "../common/hypercall/hypercall_def.h"
#include <iostream>

int main() {
    std::cout << "Enum values:" << std::endl;
    std::cout << "guest_physical_memory_operation: " << static_cast<int>(hypercall_type_t::guest_physical_memory_operation) << std::endl;
    std::cout << "guest_virtual_memory_operation: " << static_cast<int>(hypercall_type_t::guest_virtual_memory_operation) << std::endl;
    std::cout << "translate_guest_virtual_address: " << static_cast<int>(hypercall_type_t::translate_guest_virtual_address) << std::endl;
    std::cout << "read_guest_cr3: " << static_cast<int>(hypercall_type_t::read_guest_cr3) << std::endl;
    std::cout << "add_slat_code_hook: " << static_cast<int>(hypercall_type_t::add_slat_code_hook) << std::endl;
    std::cout << "remove_slat_code_hook: " << static_cast<int>(hypercall_type_t::remove_slat_code_hook) << std::endl;
    std::cout << "hide_guest_physical_page: " << static_cast<int>(hypercall_type_t::hide_guest_physical_page) << std::endl;
    std::cout << "log_current_state: " << static_cast<int>(hypercall_type_t::log_current_state) << std::endl;
    std::cout << "flush_logs: " << static_cast<int>(hypercall_type_t::flush_logs) << std::endl;
    std::cout << "get_heap_free_page_count: " << static_cast<int>(hypercall_type_t::get_heap_free_page_count) << std::endl;
    std::cout << "get_process_base_by_pid: " << static_cast<int>(hypercall_type_t::get_process_base_by_pid) << std::endl;
    std::cout << "get_process_cr3: " << static_cast<int>(hypercall_type_t::get_process_cr3) << std::endl;
    std::cout << "check_hyperv_attachment_memory_mapping: " << static_cast<int>(hypercall_type_t::check_hyperv_attachment_memory_mapping) << std::endl;
    std::cout << "allocate_hidden_memory: " << static_cast<int>(hypercall_type_t::allocate_hidden_memory) << std::endl;
    std::cout << "free_hidden_memory: " << static_cast<int>(hypercall_type_t::free_hidden_memory) << std::endl;
    std::cout << "get_process_eprocess_base: " << static_cast<int>(hypercall_type_t::get_process_eprocess_base) << std::endl;
    std::cout << "call_dllmain_silently: " << static_cast<int>(hypercall_type_t::call_dllmain_silently) << std::endl;
    
    // Test the union
    hypercall_info_t test_info = {};
    test_info.call_type = hypercall_type_t::call_dllmain_silently;
    test_info.primary_key = hypercall_primary_key;
    test_info.secondary_key = hypercall_secondary_key;
    
    std::cout << std::endl << "Union test:" << std::endl;
    std::cout << "Original enum value: " << static_cast<int>(hypercall_type_t::call_dllmain_silently) << std::endl;
    std::cout << "Value stored in union: " << static_cast<int>(test_info.call_type) << std::endl;
    std::cout << "Union value (hex): 0x" << std::hex << test_info.value << std::endl;
    
    return 0;
}
