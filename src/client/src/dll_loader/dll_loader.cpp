#include "dll_loader.h"
#include "../hypercall/hypercall.h"
#include "../system/system.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#undef min
#undef max

namespace dll_loader {

    std::string resolve_api_set_name(const std::string& module_name)
    {
        std::string lower_name = module_name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        // Common API set redirections for Windows 10/11
        if (lower_name.find("api-ms-win-crt-runtime") != std::string::npos ||
            lower_name.find("api-ms-win-crt-stdio") != std::string::npos ||
            lower_name.find("api-ms-win-crt-heap") != std::string::npos ||
            lower_name.find("api-ms-win-crt-string") != std::string::npos ||
            lower_name.find("api-ms-win-crt-convert") != std::string::npos ||
            lower_name.find("api-ms-win-crt-environment") != std::string::npos ||
            lower_name.find("api-ms-win-crt-filesystem") != std::string::npos ||
            lower_name.find("api-ms-win-crt-locale") != std::string::npos ||
            lower_name.find("api-ms-win-crt-math") != std::string::npos ||
            lower_name.find("api-ms-win-crt-multibyte") != std::string::npos ||
            lower_name.find("api-ms-win-crt-process") != std::string::npos ||
            lower_name.find("api-ms-win-crt-time") != std::string::npos ||
            lower_name.find("api-ms-win-crt-utility") != std::string::npos) {
            return "ucrtbase.dll";
        }

        if (lower_name.find("api-ms-win-core-") != std::string::npos ||
            lower_name.find("api-ms-win-security-") != std::string::npos) {
            return "kernelbase.dll";
        }

        return module_name; // Return original if no redirection needed
    }

    std::uint64_t find_target_module_base(std::uint32_t target_pid, std::uint64_t target_cr3, const char* module_name, std::uint64_t ps_initial_system_process)
    {
        std::cout << "Searching for module: " << module_name << std::endl;

        // Resolve API set names first
        std::string resolved_name = resolve_api_set_name(module_name);

        // 1. Get the target process base address  
        std::uint64_t process_base = hypercall::get_process_base(target_pid, ps_initial_system_process, sys::current_cr3);
        if (process_base == 0) {
            std::cout << "Failed to get process base for PID: " << target_pid << std::endl;
            return 0;
        }

        std::cout << "Target process base: 0x" << std::hex << process_base << std::endl;

        // 2. Read DOS header from target process
        IMAGE_DOS_HEADER dos_header;
        if (hypercall::read_guest_virtual_memory(&dos_header, process_base, target_cr3, sizeof(dos_header)) != sizeof(dos_header)) {
            std::cout << "Failed to read DOS header" << std::endl;
            return 0;
        }

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
            std::cout << "Invalid DOS signature" << std::endl;
            return 0;
        }

        // 3. Read NT headers from target process
        IMAGE_NT_HEADERS nt_headers;
        std::uint64_t nt_headers_addr = process_base + dos_header.e_lfanew;
        if (hypercall::read_guest_virtual_memory(&nt_headers, nt_headers_addr, target_cr3, sizeof(nt_headers)) != sizeof(nt_headers)) {
            std::cout << "Failed to read NT headers" << std::endl;
            return 0;
        }

        if (nt_headers.Signature != IMAGE_NT_SIGNATURE) {
            std::cout << "Invalid NT signature" << std::endl;
            return 0;
        }

        // 4. Get import directory from data directory
        IMAGE_DATA_DIRECTORY& import_dir = nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir.VirtualAddress == 0 || import_dir.Size == 0) {
            std::cout << "No import directory found" << std::endl;
            return 0;
        }

        std::cout << "Import directory at RVA: 0x" << std::hex << import_dir.VirtualAddress << std::endl;

        // 5. Walk the import table to find our module
        std::uint64_t import_table_addr = process_base + import_dir.VirtualAddress;

        for (int i = 0; i < 100; i++) { // Safety limit
            IMAGE_IMPORT_DESCRIPTOR import_desc;
            std::uint64_t desc_addr = import_table_addr + (i * sizeof(IMAGE_IMPORT_DESCRIPTOR));

            if (hypercall::read_guest_virtual_memory(&import_desc, desc_addr, target_cr3, sizeof(import_desc)) != sizeof(import_desc)) {
                break;
            }

            if (import_desc.Name == 0) {
                break; // End of import table
            }

            // Read module name from target process
            char current_module_name[256] = {};
            std::uint64_t name_addr = process_base + import_desc.Name;
            if (hypercall::read_guest_virtual_memory(current_module_name, name_addr, target_cr3, sizeof(current_module_name) - 1) == 0) {
                continue;
            }

            current_module_name[255] = '\0'; // Ensure null termination
            std::cout << "Found imported module: " << current_module_name << std::endl;

            // Compare names (case insensitive)
            std::string current_name = current_module_name;
            std::string search_name = resolved_name;
            std::transform(current_name.begin(), current_name.end(), current_name.begin(), ::tolower);
            std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);

            if (current_name == search_name) {
                std::cout << "Found matching module: " << current_module_name << std::endl;

                // Found the module! Now get its actual loaded base address
                std::uint64_t iat_addr = process_base + import_desc.FirstThunk;
                std::uint64_t first_import_addr;

                if (hypercall::read_guest_virtual_memory(&first_import_addr, iat_addr, target_cr3, sizeof(first_import_addr)) == sizeof(first_import_addr)) {
                    std::cout << "First import address: 0x" << std::hex << first_import_addr << std::endl;

                    // Find the module base by checking PE headers at aligned addresses
                    std::uint64_t potential_base = first_import_addr & ~0xFFFF; // Align to 64KB boundary

                    // Search backwards up to 16MB
                    for (int j = 0; j < 256; j++) {
                        IMAGE_DOS_HEADER test_dos;
                        if (hypercall::read_guest_virtual_memory(&test_dos, potential_base, target_cr3, sizeof(test_dos)) == sizeof(test_dos)) {
                            if (test_dos.e_magic == IMAGE_DOS_SIGNATURE) {
                                IMAGE_NT_HEADERS test_nt;
                                std::uint64_t test_nt_addr = potential_base + test_dos.e_lfanew;
                                if (hypercall::read_guest_virtual_memory(&test_nt, test_nt_addr, target_cr3, sizeof(test_nt)) == sizeof(test_nt)) {
                                    if (test_nt.Signature == IMAGE_NT_SIGNATURE) {
                                        // Verify this is actually our module
                                        std::uint64_t module_end = potential_base + test_nt.OptionalHeader.SizeOfImage;
                                        if (first_import_addr >= potential_base && first_import_addr < module_end) {
                                            std::cout << "Found " << module_name << " at base: 0x" << std::hex << potential_base << std::endl;
                                            return potential_base;
                                        }
                                    }
                                }
                            }
                        }
                        potential_base -= 0x10000; // Go back 64KB

                        if (potential_base < 0x10000) {
                            break;
                        }
                    }
                }
            }
        }

        std::cout << "Module " << module_name << " not found in import table" << std::endl;
        return 0;
    }

    std::vector<std::uint8_t> read_dll_file(const std::string& dll_path)
    {
        std::ifstream file(dll_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cout << "Failed to open DLL file: " << dll_path << std::endl;
            return {};
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            std::cout << "Failed to read DLL file" << std::endl;
            return {};
        }
        return buffer;
    }

    IMAGE_DOS_HEADER* get_dos_header(const std::vector<std::uint8_t>& dll_data)
    {
        if (dll_data.size() < sizeof(IMAGE_DOS_HEADER)) {
            return nullptr;
        }
        IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(const_cast<std::uint8_t*>(dll_data.data()));
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
            return nullptr;
        }
        return dos_header;
    }

    IMAGE_NT_HEADERS* get_nt_headers(const std::vector<std::uint8_t>& dll_data)
    {
        IMAGE_DOS_HEADER* dos_header = get_dos_header(dll_data);
        if (!dos_header) {
            return nullptr;
        }
        if (dos_header->e_lfanew >= dll_data.size()) {
            return nullptr;
        }
        IMAGE_NT_HEADERS* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
            const_cast<std::uint8_t*>(dll_data.data()) + dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
            return nullptr;
        }
        return nt_headers;
    }

    IMAGE_SECTION_HEADER* get_sections(const std::vector<std::uint8_t>& dll_data)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            return nullptr;
        }
        return reinterpret_cast<IMAGE_SECTION_HEADER*>(
            reinterpret_cast<std::uint8_t*>(nt_headers) +
            offsetof(IMAGE_NT_HEADERS, OptionalHeader) +
            nt_headers->FileHeader.SizeOfOptionalHeader);
    }

    std::uint32_t calculate_required_pages(const std::vector<std::uint8_t>& dll_data)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            return 0;
        }
        std::uint32_t size_of_image = nt_headers->OptionalHeader.SizeOfImage;
        return (size_of_image + 0xFFF) / 0x1000; // Round up to page boundary
    }

    std::uint64_t get_dll_preferred_base(const std::vector<std::uint8_t>& dll_data)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            return 0;
        }
        return nt_headers->OptionalHeader.ImageBase;
    }

    std::uint64_t get_dll_entry_point(const std::vector<std::uint8_t>& dll_data, std::uint64_t base_address)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            return 0;
        }
        return base_address + nt_headers->OptionalHeader.AddressOfEntryPoint;
    }

    std::uint8_t relocate_dll(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t old_base,
        std::uint64_t new_base,
        std::uint32_t target_pid,
        std::uint64_t target_cr3)
    {
        if (old_base == new_base) {
            return 1; // No relocation needed
        }

        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            return 0;
        }

        IMAGE_DATA_DIRECTORY& reloc_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (reloc_dir.VirtualAddress == 0 || reloc_dir.Size == 0) {
            std::cout << "No relocation data found" << std::endl;
            return 1; // No relocations, assume success
        }

        std::int64_t delta = new_base - old_base;
        std::uint32_t reloc_offset = 0;
        int safety_counter = 0;

        while (reloc_offset < reloc_dir.Size && safety_counter < 1000) {
            safety_counter++;
            std::uint64_t block_address = new_base + reloc_dir.VirtualAddress + reloc_offset;
            IMAGE_BASE_RELOCATION reloc_block;

            if (hypercall::read_guest_virtual_memory(
                &reloc_block,
                block_address,
                target_cr3,
                sizeof(reloc_block)) != sizeof(reloc_block)) {
                return 0;
            }

            if (reloc_block.SizeOfBlock == 0) {
                break;
            }

            std::uint32_t entries_count = (reloc_block.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(std::uint16_t);
            if (entries_count > 1000) { // Prevent huge blocks
                return 0;
            }

            std::vector<std::uint16_t> entries(entries_count);
            if (hypercall::read_guest_virtual_memory(
                entries.data(),
                block_address + sizeof(IMAGE_BASE_RELOCATION),
                target_cr3,
                entries.size() * sizeof(std::uint16_t)) != entries.size() * sizeof(std::uint16_t)) {
                return 0;
            }

            for (std::uint16_t entry : entries) {
                std::uint16_t type = entry >> 12;
                std::uint16_t offset = entry & 0xFFF;

                if (type == IMAGE_REL_BASED_ABSOLUTE) {
                    continue;
                }

                std::uint64_t reloc_address = new_base + reloc_block.VirtualAddress + offset;

                if (type == IMAGE_REL_BASED_DIR64) {
                    std::uint64_t value;
                    if (hypercall::read_guest_virtual_memory(
                        &value,
                        reloc_address,
                        target_cr3,
                        sizeof(value)) != sizeof(value)) {
                        continue;
                    }
                    value += delta;
                    if (hypercall::write_guest_virtual_memory(
                        &value,
                        reloc_address,
                        target_cr3,
                        sizeof(value)) != sizeof(value)) {
                        return 0;
                    }
                }
            }
            reloc_offset += reloc_block.SizeOfBlock;
        }

        std::cout << "Successfully applied relocations (delta: 0x" << std::hex << delta << ")" << std::endl;
        return 1;
    }

    std::uint8_t resolve_imports(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t injected_dll_base,
        std::uint32_t target_pid,
        std::uint64_t target_cr3,
        std::uint64_t ps_initial_system_process)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) return 0;

        IMAGE_DATA_DIRECTORY& import_dir = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir.VirtualAddress == 0 || import_dir.Size == 0) {
            std::cout << "No import data found" << std::endl;
            return 1;
        }

        // CRITICAL FIX: Read import descriptors from the INJECTED DLL, not the original buffer
        std::uint64_t import_table_address = injected_dll_base + import_dir.VirtualAddress;
        int module_counter = 0;

        while (module_counter < 100) // Safety limit
        {
            IMAGE_IMPORT_DESCRIPTOR import_descriptor;

            // Read import descriptor from target process memory
            if (hypercall::read_guest_virtual_memory(
                &import_descriptor,
                import_table_address + (module_counter * sizeof(IMAGE_IMPORT_DESCRIPTOR)),
                target_cr3,
                sizeof(import_descriptor)) != sizeof(import_descriptor)) {
                break;
            }

            if (import_descriptor.Name == 0) {
                break; // End of import table
            }

            module_counter++;

            // CRITICAL FIX: Read module name from injected DLL memory
            std::uint64_t module_name_address = injected_dll_base + import_descriptor.Name;
            char module_name[256] = {};

            if (hypercall::read_guest_virtual_memory(
                module_name,
                module_name_address,
                target_cr3,
                sizeof(module_name) - 1) == 0) {
                std::cout << "Failed to read module name at RVA: 0x" << std::hex << import_descriptor.Name << std::endl;
                continue;
            }

            // Ensure null termination
            module_name[255] = '\0';

    std::cout << "Resolving imports from: " << module_name << std::endl;

    uint64_t target_module_base = find_target_module_base(target_pid, target_cr3, module_name, ps_initial_system_process);
    if (target_module_base == 0) {
        std::cout << "WARNING: Failed to find " << module_name << " in target process." << std::endl;
        // Check if it's a critical module
        std::string lower_module = module_name;
        std::transform(lower_module.begin(), lower_module.end(), lower_module.begin(), ::tolower);
        if (lower_module.find("api-ms-win-crt") != std::string::npos) {
            std::cout << "INFO: This is an API set, imports may be resolved through ucrtbase.dll" << std::endl;
        }
        continue;
    }

            uint64_t iat_rva = import_descriptor.FirstThunk;
            uint64_t int_rva = import_descriptor.OriginalFirstThunk ? import_descriptor.OriginalFirstThunk : import_descriptor.FirstThunk;

            // Read thunk data from injected DLL memory
            std::uint64_t thunk_address = injected_dll_base + int_rva;
            int function_counter = 0;

            while (function_counter < 1000) // Safety limit
            {
                IMAGE_THUNK_DATA64 thunk_data;

                if (hypercall::read_guest_virtual_memory(
                    &thunk_data,
                    thunk_address + (function_counter * sizeof(IMAGE_THUNK_DATA64)),
                    target_cr3,
                    sizeof(thunk_data)) != sizeof(thunk_data)) {
                    break;
                }

                if (thunk_data.u1.AddressOfData == 0) {
                    break; // End of thunk array
                }

                function_counter++;
                uint64_t function_address = 0;

                if (IMAGE_SNAP_BY_ORDINAL64(thunk_data.u1.Ordinal)) {
                    // Ordinal import
                    std::uint16_t ordinal = IMAGE_ORDINAL64(thunk_data.u1.Ordinal);
                    std::cout << "Ordinal import: " << ordinal << " (not implemented)" << std::endl;
                }
                else {
                    // Import by name - read from injected DLL memory
                    std::uint64_t import_by_name_address = injected_dll_base + thunk_data.u1.AddressOfData;

                    IMAGE_IMPORT_BY_NAME import_by_name;
                    if (hypercall::read_guest_virtual_memory(
                        &import_by_name,
                        import_by_name_address,
                        target_cr3,
                        sizeof(IMAGE_IMPORT_BY_NAME)) == sizeof(IMAGE_IMPORT_BY_NAME)) {

                        // Read the function name
                        char function_name[256] = {};
                        if (hypercall::read_guest_virtual_memory(
                            function_name,
                            import_by_name_address + offsetof(IMAGE_IMPORT_BY_NAME, Name),
                            target_cr3,
                            sizeof(function_name) - 1) > 0) {

                            function_name[255] = '\0';
                            function_address = sys::find_export_address_in_target(target_module_base, target_cr3, function_name);

                            if (function_address == 0) {
                                std::cout << "Warning: Failed to resolve import for " << function_name << std::endl;
                            }
                        }
                    }
                }

                // Write resolved address to IAT
                if (function_address != 0) {
                    uint64_t iat_entry_address = injected_dll_base + iat_rva + ((function_counter - 1) * sizeof(uint64_t));
                    if (hypercall::write_guest_virtual_memory(
                        &function_address,
                        iat_entry_address,
                        target_cr3,
                        sizeof(function_address)) != sizeof(function_address)) {
                        std::cout << "Failed to write IAT entry" << std::endl;
                    }
                }
            }
        }

        std::cout << "Import resolution completed" << std::endl;
        return 1;
    }

    std::uint8_t map_dll_to_hidden_memory(
        const std::vector<std::uint8_t>& dll_data,
        std::uint64_t base_address,
        std::uint32_t target_pid,
        std::uint64_t target_cr3)
    {
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        IMAGE_SECTION_HEADER* sections = get_sections(dll_data);
        if (!nt_headers || !sections) {
            std::cout << "[DEBUG] Failed to get NT headers or sections" << std::endl;
            return 0;
        }

        std::cout << "[DEBUG] Mapping DLL sections to hidden memory at 0x" << std::hex << base_address << std::endl;
        std::cout << "[DEBUG] SizeOfHeaders: " << std::dec << nt_headers->OptionalHeader.SizeOfHeaders << std::endl;
        std::cout << "[DEBUG] NumberOfSections: " << nt_headers->FileHeader.NumberOfSections << std::endl;
        std::cout << "[DEBUG] Target CR3: 0x" << std::hex << target_cr3 << std::endl;

        // First, write headers
        std::vector<std::uint8_t> headers(nt_headers->OptionalHeader.SizeOfHeaders);
        std::memcpy(headers.data(), dll_data.data(), headers.size());

        std::cout << "[DEBUG] Writing PE headers (" << std::dec << headers.size() << " bytes) to 0x" 
                  << std::hex << base_address << std::endl;

        std::uint64_t bytes_written = hypercall::write_guest_virtual_memory(
            headers.data(), base_address, target_cr3, headers.size());
        
        if (bytes_written != headers.size()) {
            std::cout << "[ERROR] Failed to write PE headers. Expected: " << std::dec << headers.size() 
                      << ", Actual: " << bytes_written << std::endl;
            return 0;
        }

        // Verify headers were written correctly
        std::vector<std::uint8_t> verify_headers(headers.size());
        std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
            verify_headers.data(), base_address, target_cr3, headers.size());
        
        if (bytes_read != headers.size()) {
            std::cout << "[ERROR] Failed to read back headers for verification. Read: " 
                      << std::dec << bytes_read << std::endl;
        } else {
            bool headers_match = std::memcmp(headers.data(), verify_headers.data(), headers.size()) == 0;
            std::cout << "[DEBUG] Headers verification: " << (headers_match ? "PASSED" : "FAILED") << std::endl;
            
            if (!headers_match) {
                std::cout << "[DEBUG] First 16 bytes written: ";
                for (int i = 0; i < 16 && i < headers.size(); i++) {
                    std::cout << std::hex << (int)headers[i] << " ";
                }
                std::cout << std::endl << "[DEBUG] First 16 bytes read back: ";
                for (int i = 0; i < 16 && i < verify_headers.size(); i++) {
                    std::cout << std::hex << (int)verify_headers[i] << " ";
                }
                std::cout << std::endl;
            }
        }

        // Then write sections
        for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
            IMAGE_SECTION_HEADER& section = sections[i];
            
            char section_name[9] = {0};
            std::memcpy(section_name, section.Name, 8);
            std::cout << "[DEBUG] Processing section " << i << ": " << section_name << std::endl;
            std::cout << "[DEBUG]   VirtualAddress: 0x" << std::hex << section.VirtualAddress << std::endl;
            std::cout << "[DEBUG]   SizeOfRawData: " << std::dec << section.SizeOfRawData << std::endl;
            std::cout << "[DEBUG]   PointerToRawData: 0x" << std::hex << section.PointerToRawData << std::endl;
            std::cout << "[DEBUG]   Characteristics: 0x" << std::hex << section.Characteristics << std::endl;

            // Validate section data
            if (section.PointerToRawData + section.SizeOfRawData > dll_data.size()) {
                std::cout << "[ERROR] Invalid section data for " << section_name 
                          << ". File offset + size (" << std::hex << section.PointerToRawData 
                          << " + " << std::dec << section.SizeOfRawData 
                          << ") > DLL size (" << dll_data.size() << ")" << std::endl;
                continue;
            }

            if (section.SizeOfRawData == 0) {
                std::cout << "[DEBUG] Skipping section " << section_name << " (zero size)" << std::endl;
                continue;
            }

            std::uint64_t section_va = base_address + section.VirtualAddress;
            std::cout << "[DEBUG] Writing section " << section_name << " to hidden memory at 0x"
                << std::hex << section_va << " (Size: " << std::dec << section.SizeOfRawData << ")" << std::endl;

            // Show first few bytes of section data
            const std::uint8_t* section_data = dll_data.data() + section.PointerToRawData;
            std::cout << "[DEBUG] First 16 bytes of section data: ";
            for (int j = 0; j < 16 && j < section.SizeOfRawData; j++) {
                std::cout << std::hex << (int)section_data[j] << " ";
            }
            std::cout << std::endl;

            bytes_written = hypercall::write_guest_virtual_memory(
                (void*)(dll_data.data() + section.PointerToRawData),
                section_va, target_cr3, section.SizeOfRawData);
                
            if (bytes_written != section.SizeOfRawData) {
                std::cout << "[ERROR] Failed to write section " << section_name
                    << " to hidden memory. Expected: " << std::dec << section.SizeOfRawData
                    << ", Actual: " << bytes_written << std::endl;
                return 0;
            }

            // Verify section was written correctly
            std::vector<std::uint8_t> verify_section(std::min((size_t)64, (size_t)section.SizeOfRawData)); // Just verify first 64 bytes
            bytes_read = hypercall::read_guest_virtual_memory(
                verify_section.data(), section_va, target_cr3, verify_section.size());
            
            if (bytes_read != verify_section.size()) {
                std::cout << "[ERROR] Failed to read back section " << section_name 
                          << " for verification. Expected: " << std::dec << verify_section.size()
                          << ", Actual: " << bytes_read << std::endl;
            } else {
                bool section_match = std::memcmp(section_data, verify_section.data(), verify_section.size()) == 0;
                std::cout << "[DEBUG] Section " << section_name << " verification: " 
                          << (section_match ? "PASSED" : "FAILED") << std::endl;
                          
                if (!section_match) {
                    std::cout << "[DEBUG] Expected first 16 bytes: ";
                    for (int j = 0; j < 16 && j < verify_section.size(); j++) {
                        std::cout << std::hex << (int)section_data[j] << " ";
                    }
                    std::cout << std::endl << "[DEBUG] Actual first 16 bytes: ";
                    for (int j = 0; j < 16 && j < verify_section.size(); j++) {
                        std::cout << std::hex << (int)verify_section[j] << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }

        std::cout << "[DEBUG] All sections successfully mapped to hidden memory" << std::endl;
        
        // Final verification - check DOS and NT headers
        IMAGE_DOS_HEADER verify_dos;
        bytes_read = hypercall::read_guest_virtual_memory(
            &verify_dos, base_address, target_cr3, sizeof(verify_dos));
            
        if (bytes_read == sizeof(verify_dos) && verify_dos.e_magic == IMAGE_DOS_SIGNATURE) {
            std::cout << "[DEBUG] DOS header verification: PASSED (signature: 0x" 
                      << std::hex << verify_dos.e_magic << ")" << std::endl;
                      
            IMAGE_NT_HEADERS verify_nt;
            bytes_read = hypercall::read_guest_virtual_memory(
                &verify_nt, base_address + verify_dos.e_lfanew, target_cr3, sizeof(verify_nt));
                
            if (bytes_read == sizeof(verify_nt) && verify_nt.Signature == IMAGE_NT_SIGNATURE) {
                std::cout << "[DEBUG] NT header verification: PASSED (signature: 0x" 
                          << std::hex << verify_nt.Signature << ")" << std::endl;
                std::cout << "[DEBUG] Entry point RVA: 0x" << std::hex << verify_nt.OptionalHeader.AddressOfEntryPoint << std::endl;
                std::cout << "[DEBUG] Entry point VA: 0x" << std::hex << (base_address + verify_nt.OptionalHeader.AddressOfEntryPoint) << std::endl;
            } else {
                std::cout << "[ERROR] NT header verification: FAILED" << std::endl;
            }
        } else {
            std::cout << "[ERROR] DOS header verification: FAILED" << std::endl;
        }
        
        return 1;
    }

    std::uint64_t load_dll_stealthily(
        std::uint32_t target_pid,
        const std::string& dll_path,
        std::uint64_t ps_initial_system_process,
        std::uint64_t initial_kernel_cr3)
    {
        std::cout << "[STEALTH] Loading DLL: " << dll_path << " into PID: " << target_pid << std::endl;

        // Read DLL file
        std::vector<std::uint8_t> dll_data = read_dll_file(dll_path);
        if (dll_data.empty()) {
            std::cout << "Failed to read DLL file" << std::endl;
            return 0;
        }

        // Get NT headers
        IMAGE_NT_HEADERS* nt_headers = get_nt_headers(dll_data);
        if (!nt_headers) {
            std::cout << "Failed to get NT headers" << std::endl;
            return 0;
        }

        std::uint32_t total_size = nt_headers->OptionalHeader.SizeOfImage;

        // Get system process CR3 (PID 4)
        constexpr std::uint32_t system_pid = 4;
        std::uint64_t kernel_cr3 = hypercall::get_process_cr3(system_pid, ps_initial_system_process, initial_kernel_cr3);
        if (kernel_cr3 == 0) {
            std::cout << "Failed to get System process CR3" << std::endl;
            return 0;
        }

        // Get target process CR3
        std::uint64_t target_cr3 = hypercall::get_process_cr3(target_pid, ps_initial_system_process, kernel_cr3);
        if (target_cr3 == 0) {
            std::cout << "Failed to get target process CR3" << std::endl;
            return 0;
        }

        // Try to allocate at preferred base first
        std::uint64_t preferred_base = get_dll_preferred_base(dll_data);
        std::uint64_t base_address = hypercall::allocate_hidden_memory(
            preferred_base, total_size, target_pid, ps_initial_system_process, kernel_cr3);

        // If preferred base failed, try fallback address
        if (base_address == 0) {
            std::cout << "[STEALTH] Preferred base not available, trying fallback address" << std::endl;
            base_address = hypercall::allocate_hidden_memory(
                0x20000000, total_size, target_pid, ps_initial_system_process, kernel_cr3);
        }

        if (base_address == 0) {
            std::cout << "[STEALTH] Failed to allocate hidden memory" << std::endl;
            return 0;
        }

        std::cout << "[STEALTH] Allocated hidden memory at: 0x" << std::hex << base_address << std::endl;

        // Map DLL to hidden memory
        if (!map_dll_to_hidden_memory(dll_data, base_address, target_pid, target_cr3)) {
            std::cout << "Failed to map DLL to hidden memory" << std::endl;
            hypercall::free_hidden_memory(base_address, total_size, target_pid, ps_initial_system_process, kernel_cr3);
            return 0;
        }

        // Apply relocations if needed
        if (preferred_base != base_address) {
            if (!relocate_dll(dll_data, preferred_base, base_address, target_pid, target_cr3)) {
                std::cout << "Failed to apply relocations" << std::endl;
                hypercall::free_hidden_memory(base_address, total_size, target_pid, ps_initial_system_process, kernel_cr3);
                return 0;
            }
        }

        // Resolve imports
        if (!resolve_imports(dll_data, base_address, target_pid, target_cr3, ps_initial_system_process)) {
            std::cout << "Failed to resolve imports" << std::endl;
            hypercall::free_hidden_memory(base_address, total_size, target_pid, ps_initial_system_process, kernel_cr3);
            return 0;
        }

        std::cout << "[STEALTH] DLL successfully loaded in hidden memory at: 0x" << std::hex << base_address << std::endl;

    // Execute DllMain if present
    std::uint64_t entry_point = get_dll_entry_point(dll_data, base_address);
    if (entry_point != 0) {
        std::cout << "[STEALTH] DllMain entry point found at: 0x" << std::hex << entry_point << std::endl;
        
        // Verify the entry point is accessible
        std::uint8_t entry_bytes[16];
        std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
            entry_bytes, entry_point, target_cr3, sizeof(entry_bytes));
            
        if (bytes_read == sizeof(entry_bytes)) {
            std::cout << "[DEBUG] Entry point bytes: ";
            for (int i = 0; i < sizeof(entry_bytes); i++) {
                std::cout << std::hex << (int)entry_bytes[i] << " ";
            }
            std::cout << std::endl;
            
            // Check if it looks like valid x64 code
            bool looks_valid = entry_bytes[0] != 0x00 || entry_bytes[1] != 0x00;
            std::cout << "[DEBUG] Entry point appears " << (looks_valid ? "valid" : "invalid") << std::endl;
        } else {
            std::cout << "[ERROR] Cannot read entry point bytes. Read: " << std::dec << bytes_read << std::endl;
            return base_address; // Return anyway, DLL is loaded
        }
        
        std::cout << "[STEALTH] Calling DllMain with parameters:" << std::endl;
        std::cout << "[DEBUG]   hModule: 0x" << std::hex << base_address << std::endl;
        std::cout << "[DEBUG]   dwReason: " << std::dec << 1 << " (DLL_PROCESS_ATTACH)" << std::endl;
        std::cout << "[DEBUG]   lpvReserved: 0x" << std::hex << 0 << std::endl;
        
        // Execute DllMain using direct approach
        std::uint64_t result = hypercall::call_dllmain_silently(
            entry_point,
            base_address,
            1, // DLL_PROCESS_ATTACH
            0  // lpvReserved
        );

        if (result == 0) {
            std::cout << "[ERROR] Failed to execute DllMain (hypercall returned 0)" << std::endl;
        } else {
            std::cout << "[STEALTH] DllMain call initiated successfully (hypercall returned 1)" << std::endl;
            std::cout << "[DEBUG] If DllMain shows a MessageBox, it should appear now..." << std::endl;
        }
    } else {
        std::cout << "[STEALTH] No DllMain entry point found (OK for resource-only DLLs)" << std::endl;
    }

        return base_address;
    }
}
