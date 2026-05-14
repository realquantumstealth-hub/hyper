#include <iostream>
#include <iomanip>
#include <Windows.h>
#include "hypercall/hypercall.h"

// Helper function to print page location as string
const char* get_page_location_string(uint8_t page_location) {
    switch (page_location) {
        case 0: return "ZeroedPageList";
        case 1: return "FreePageList";
        case 2: return "StandbyPageList";
        case 3: return "ModifiedPageList";
        case 4: return "ModifiedNoWritePageList";
        case 5: return "BadPageList";
        case 6: return "ActiveAndValid";
        case 7: return "TransitionPage";
        default: return "Unknown";
    }
}

// Helper function to print cache attribute as string
const char* get_cache_attribute_string(uint8_t cache_attr) {
    switch (cache_attr) {
        case 0: return "MiNonCached";
        case 1: return "MiCached";
        case 2: return "MiWriteCombined";
        case 3: return "MiHardwareCoherentCached";
        default: return "Unknown";
    }
}

void demo_query_hypervisor_pfn_info() {
    std::cout << "\n=== Hypervisor PFN Query Demo ===\n" << std::endl;
    
    // First, get the ntoskrnl base address to show it's working
    std::uint64_t ntoskrnl_base = hypercall::get_ntoskrnl_base_address();
    std::cout << "ntoskrnl.exe base address: 0x" << std::hex << ntoskrnl_base << std::endl;
    
    if (!ntoskrnl_base) {
        std::cout << "Failed to get ntoskrnl base address!" << std::endl;
        return;
    }
    
    // Get the actual hypervisor memory layout
    hypervisor_memory_info_t memory_info = {};
    std::uint64_t result = hypercall::get_hypervisor_memory_info(&memory_info);
    
    if (result != 0) {
        std::cout << "Failed to get hypervisor memory info! Status: 0x" << std::hex << result << std::endl;
        return;
    }
    
    if (!memory_info.is_valid) {
        std::cout << "Hypervisor memory info is not valid!" << std::endl;
        return;
    }
    
    std::cout << "\nHypervisor Memory Layout:" << std::endl;
    std::cout << "  Main Attachment: 0x" << std::hex << memory_info.physical_base 
              << " - 0x" << std::hex << (memory_info.physical_base + memory_info.size_bytes) 
              << " (" << std::dec << memory_info.page_count << " pages, " 
              << (memory_info.size_bytes / 1024) << " KB)" << std::endl;
    
    if (memory_info.heap_physical_base != 0) {
        std::cout << "  Heap Memory: 0x" << std::hex << memory_info.heap_physical_base 
                  << " - 0x" << std::hex << (memory_info.heap_physical_base + memory_info.heap_size_bytes) 
                  << " (" << std::dec << memory_info.heap_page_count << " pages, " 
                  << (memory_info.heap_size_bytes / 1024) << " KB)" << std::endl;
    }
    
    if (memory_info.uefi_boot_base != 0) {
        std::cout << "  UEFI Boot: 0x" << std::hex << memory_info.uefi_boot_base 
                  << " - 0x" << std::hex << (memory_info.uefi_boot_base + memory_info.uefi_boot_size) 
                  << " (" << std::dec << (memory_info.uefi_boot_size / 4096) << " pages, " 
                  << (memory_info.uefi_boot_size / 1024) << " KB)" << std::endl;
    }
    
    // Create a list of actual hypervisor memory addresses to query
    std::vector<std::uint64_t> hypervisor_addresses;
    
    // Add some pages from the main hypervisor attachment
    if (memory_info.physical_base != 0 && memory_info.page_count > 0) {
        hypervisor_addresses.push_back(memory_info.physical_base); // First page
        
        if (memory_info.page_count > 1) {
            hypervisor_addresses.push_back(memory_info.physical_base + 0x1000); // Second page
        }
        
        if (memory_info.page_count > 10) {
            hypervisor_addresses.push_back(memory_info.physical_base + (10 * 0x1000)); // 10th page
        }
        
        if (memory_info.page_count > 1) {
            // Last page
            hypervisor_addresses.push_back(memory_info.physical_base + ((memory_info.page_count - 1) * 0x1000));
        }
    }
    
    // Add some pages from the heap if available
    if (memory_info.heap_physical_base != 0 && memory_info.heap_page_count > 0) {
        hypervisor_addresses.push_back(memory_info.heap_physical_base); // First heap page
        
        if (memory_info.heap_page_count > 1) {
            hypervisor_addresses.push_back(memory_info.heap_physical_base + 0x1000); // Second heap page
        }
    }
    
    std::cout << "\nQuerying " << hypervisor_addresses.size() << " actual hypervisor memory pages...\n" << std::endl;
    
    for (auto physical_addr : hypervisor_addresses) {
        hypervisor_pfn_info_t pfn_info = {};
        
        // Determine which memory region this address belongs to
        std::string region_type = "Unknown";
        if (physical_addr >= memory_info.physical_base && 
            physical_addr < (memory_info.physical_base + memory_info.size_bytes)) {
            region_type = "Hypervisor Attachment";
        } else if (memory_info.heap_physical_base != 0 && 
                   physical_addr >= memory_info.heap_physical_base && 
                   physical_addr < (memory_info.heap_physical_base + memory_info.heap_size_bytes)) {
            region_type = "Hypervisor Heap";
        } else if (memory_info.uefi_boot_base != 0 && 
                   physical_addr >= memory_info.uefi_boot_base && 
                   physical_addr < (memory_info.uefi_boot_base + memory_info.uefi_boot_size)) {
            region_type = "UEFI Boot";
        }
        
        std::cout << "\n--- Querying Physical Address: 0x" << std::hex << physical_addr 
                  << " (" << region_type << ") ---" << std::endl;
        
        std::uint64_t result = hypercall::query_hypervisor_pfn_info(physical_addr, &pfn_info);
        
        if (result == 0) { // STATUS_SUCCESS
            std::cout << "Query Status: SUCCESS" << std::endl;
            std::cout << "PFN Index: 0x" << std::hex << pfn_info.pfn_index << std::endl;
            std::cout << "Is Hypervisor Page: " << (pfn_info.is_hypervisor_page ? "YES" : "NO") << std::endl;
            std::cout << "Is Valid: " << (pfn_info.is_valid ? "YES" : "NO") << std::endl;
            
            std::cout << "\nPFN Details:" << std::endl;
            std::cout << "  Reference Count: " << std::dec << pfn_info.reference_count << std::endl;
            std::cout << "  Share Count: " << std::dec << pfn_info.share_count << std::endl;
            std::cout << "  Page Location: " << get_page_location_string(pfn_info.e1_flags.page_location) 
                      << " (" << static_cast<int>(pfn_info.e1_flags.page_location) << ")" << std::endl;
            std::cout << "  Cache Attribute: " << get_cache_attribute_string(pfn_info.e1_flags.cache_attribute) 
                      << " (" << static_cast<int>(pfn_info.e1_flags.cache_attribute) << ")" << std::endl;
            
            std::cout << "\nFlags:" << std::endl;
            std::cout << "  Modified: " << (pfn_info.e1_flags.modified ? "YES" : "NO") << std::endl;
            std::cout << "  Write In Progress: " << (pfn_info.e1_flags.write_in_progress ? "YES" : "NO") << std::endl;
            std::cout << "  Read In Progress: " << (pfn_info.e1_flags.read_in_progress ? "YES" : "NO") << std::endl;
            std::cout << "  Priority: " << static_cast<int>(pfn_info.e3_flags.priority) << std::endl;
            std::cout << "  On Prototype PTE: " << (pfn_info.e3_flags.on_prototype_pte ? "YES" : "NO") << std::endl;
            std::cout << "  Removal Requested: " << (pfn_info.e3_flags.removal_requested ? "YES" : "NO") << std::endl;
            std::cout << "  Parity Error: " << (pfn_info.e3_flags.parity_error ? "YES" : "NO") << std::endl;
            
            std::cout << "\nPTE Information:" << std::endl;
            std::cout << "  PTE Address: 0x" << std::hex << pfn_info.pte_address << std::endl;
            std::cout << "  PTE Frame: 0x" << std::hex << pfn_info.pte_frame << std::endl;
            
            std::cout << "\nList Entry:" << std::endl;
            std::cout << "  Flink: 0x" << std::hex << pfn_info.list_entry.flink << std::endl;
            std::cout << "  Blink: 0x" << std::hex << pfn_info.list_entry.blink << std::endl;
            
            std::cout << "\nOther:" << std::endl;
            std::cout << "  Page Color: " << std::dec << pfn_info.page_color << std::endl;
            std::cout << "  Node Blink: " << std::dec << pfn_info.node_blink << std::endl;
            
            // Print first few bytes of raw PFN data for debugging
            std::cout << "\nRaw PFN Data (first 16 bytes): ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::hex << std::setfill('0') << std::setw(2) 
                         << static_cast<int>(pfn_info.raw_pfn_data[i]) << " ";
            }
            std::cout << std::endl;
            
        } else {
            std::cout << "Query failed with status: 0x" << std::hex << result << std::endl;
        }
    }
    
    std::cout << "\n=== Analysis Summary ===" << std::endl;
    std::cout << "This shows you exactly how Windows sees your hypervisor memory pages!" << std::endl;
    std::cout << "Key things to look for:" << std::endl;
    std::cout << "  - Reference Count: Should be > 0 for active pages" << std::endl;
    std::cout << "  - Page Location: Shows which memory list the page is on" << std::endl;
    std::cout << "  - Modified Bit: Indicates if the page has been written to" << std::endl;
    std::cout << "  - Cache Attributes: How the page is cached by the CPU" << std::endl;
    std::cout << "  - Is Hypervisor Page: Whether our detection logic identifies it" << std::endl;
    std::cout << "\n=== Demo Complete ===" << std::endl;
}

// Example of how to use this in your main application
void example_usage() {
    std::cout << "Example: How to query hypervisor memory in MmPfnDatabase" << std::endl;
    std::cout << "This shows you exactly how Windows sees your hypervisor memory pages!" << std::endl;
    
    // Run the demo
    demo_query_hypervisor_pfn_info();
}
