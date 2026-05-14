#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <Windows.h>

namespace dll_loader {
    // Structure to represent a loaded DLL in target process
    struct loaded_dll_t {
        std::uint64_t base_address;        // Base address in target process
        std::uint64_t entry_point;         // DLL entry point
        std::uint64_t size_of_image;       // Total size of loaded image
        std::string dll_path;              // Path to original DLL file
        std::uint32_t target_pid;          // Target process ID
    };

    // Section permissions
    enum class section_permissions : std::uint32_t {
        read_only = PAGE_READONLY,
        read_write = PAGE_READWRITE,
        read_execute = PAGE_EXECUTE_READ,
        read_write_execute = PAGE_EXECUTE_READWRITE
    };


    std::uint8_t relocate_dll(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t old_base,
        std::uint64_t new_base,
        std::uint32_t target_pid,
        std::uint64_t target_cr3
    );

    std::uint8_t resolve_imports(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t base_address,
        std::uint32_t target_pid,
        std::uint64_t target_cr3
    );

    // Helper functions
    std::vector<std::uint8_t> read_dll_file(const std::string& dll_path);
    std::uint32_t calculate_required_pages(const std::vector<std::uint8_t>& dll_data);
    std::uint64_t get_dll_preferred_base(const std::vector<std::uint8_t>& dll_data);
    std::uint64_t get_dll_entry_point(const std::vector<std::uint8_t>& dll_data, std::uint64_t base_address);

    // PE parsing helpers
    IMAGE_DOS_HEADER* get_dos_header(const std::vector<std::uint8_t>& dll_data);
    IMAGE_NT_HEADERS* get_nt_headers(const std::vector<std::uint8_t>& dll_data);
    IMAGE_SECTION_HEADER* get_sections(const std::vector<std::uint8_t>& dll_data);

    // Memory allocation via hypervisor
    std::uint64_t allocate_process_memory(
        std::uint32_t target_pid,
        std::uint32_t page_count,
        std::uint64_t ps_initial_system_process,
        std::uint64_t kernel_cr3
    );

    // Stealth DLL injection using hidden memory
    std::uint64_t load_dll_stealthily(
        std::uint32_t target_pid,
        const std::string& dll_path,
        std::uint64_t ps_initial_system_process,
        std::uint64_t kernel_cr3
    );

    // Map DLL to hidden memory
    std::uint8_t map_dll_to_hidden_memory(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t base_address,
        std::uint32_t target_pid,
        std::uint64_t target_cr3
    );
}