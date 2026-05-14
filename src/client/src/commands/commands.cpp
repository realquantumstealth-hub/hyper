#include "commands.h"
#include <CLI/CLI.hpp>
#include <hypercall/hypercall_def.h>
#include "../hook/hook.h"
#include "../hypercall/hypercall.h"
#include "../system/system.h"
#include "../dll_loader/dll_loader.h"

#include <print>
#include <array>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <Windows.h>
#include <thread>
#include <winternl.h>
#undef min

// Vector3 struct for 3D coordinates
struct Vector3
{
    double x, y, z;
    
    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}
};

// TArray struct for reading UE4 arrays
struct TArray
{
    uintptr_t Array;
    uint32_t Count;
    uint32_t MaxCount;

    uintptr_t operator[](uint32_t index) const
    {
        if (Array && index < Count) {
            uintptr_t result = 0;
            hypercall::read_guest_virtual_memory(&result, Array + (index * 8), hypercall::get_cached_cr3(), sizeof(result));
            return result;
        }
        return 0;
    }

    uint32_t Size() const {
        return Count;
    }

    bool IsValid() const
    {
        constexpr uint32_t MAX_ARRAY_COUNT = 1000000;
        if (!Array)
            return false;

        if (Count > MaxCount)
            return false;

        if (MaxCount > MAX_ARRAY_COUNT)
            return false;

        return true;
    }

    uintptr_t GetAddress() const
    {
        return Array;
    }
};

// Offsets namespace
namespace Offsets
{
    inline DWORD UWorld = (0xB589C1D + 0x3);
    inline DWORD FNamePool = 0xB6FCD40;
    inline DWORD ObjectState = 0xB50C000;
 
    inline DWORD Gameinstance = 0x01D8;
    inline DWORD OwningWorld = 0x00C0;
    inline DWORD Ulevel = 0x0038;
    inline DWORD LocalPlayers = 0x0040;
    inline DWORD PlayerController = 0x0038;
    inline DWORD PlayerCameraManager = 0x0520;
    inline DWORD MyHUD = 0x0518;
    inline DWORD AcknowledgedPawn = 0x0510;
    inline DWORD ControlRotation = 0x04E0;
    inline DWORD AActorArray = 0x00A0;
    inline DWORD RootComponent = 0x0288;
    inline DWORD CameraCachePrivate = 0x17B0;
    inline DWORD RelativeLocation = 0x0170;
    inline DWORD RelativeRotation = 0x0188;
    inline DWORD MeshComponent = 0x04E8;
    inline DWORD DamageHandler = 0x0C68;
    inline DWORD Health = 0x01E0;
    inline DWORD bAlive = 0x01D9;
    inline DWORD WasAlly = 0x0F29;
 
    inline DWORD ComponentToWorld = 0x2D0;
 
    inline DWORD BoneArray = 0x730;
    inline DWORD BoneArrayCache = BoneArray + 0x10;
 
    inline DWORD BoundsScale = 0x0474;
    inline DWORD LastSubmitTime = BoundsScale + 0x4;
    inline DWORD LastRenderTime = LastSubmitTime + 0x4;
 
    inline DWORD LocalBounds = 0x0730;
    inline DWORD BoneCount = LocalBounds + 0x8;
}

#define d_invoke_command_processor(command) process_##command(##command)
#define d_initial_process_command(command) if (*##command) d_invoke_command_processor(command)
#define d_process_command(command) else if (*##command) d_invoke_command_processor(command)

template <class t>
t get_command_option(CLI::App* app, std::string option_name)
{
	auto option = app->get_option(option_name);

	return option->empty() == false ? option->as<t>() : t{};
}

CLI::Option* add_command_option(CLI::App* app, std::string option_name)
{
	return app->add_option(option_name);
}

CLI::Option* add_transformed_command_option(CLI::App* app, std::string option_name, CLI::Transformer& transformer)
{
	CLI::Option* option = add_command_option(app, option_name);

	return option->transform(transformer);
}

std::uint8_t get_command_flag(CLI::App* app, std::string flag_name)
{
	auto option = app->get_option(flag_name);

	return !option->empty();
}

CLI::Option* add_command_flag(CLI::App* app, std::string flag_name)
{
	return app->add_flag(flag_name);
}

CLI::App* init_rgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* rgpm = app.add_subcommand("rgpm", "reads memory from a given guest physical address")->ignore_case();

	add_transformed_command_option(rgpm, "physical_address", aliases_transformer)->required();
	add_command_option(rgpm, "size")->check(CLI::Range(0, 8))->required();

	return rgpm;
}

void process_rgpm(CLI::App* rgpm)
{
	const std::uint64_t guest_physical_address = get_command_option<std::uint64_t>(rgpm, "physical_address");
	const std::uint64_t size = get_command_option<std::uint64_t>(rgpm, "size");

	std::uint64_t value = 0;

	const std::uint64_t bytes_read = hypercall::read_guest_physical_memory(&value, guest_physical_address, size);

	if (bytes_read == size)
	{
		std::println("value: 0x{:x}", value);
	}
	else
	{
		std::println("failed to read");
	}
}

CLI::App* init_wgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* wgpm = app.add_subcommand("wgpm", "writes memory to a given guest physical address")->ignore_case();

	add_transformed_command_option(wgpm, "physical_address", aliases_transformer)->required();
	add_command_option(wgpm, "value")->required();
	add_command_option(wgpm, "size")->check(CLI::Range(0, 8))->required();

	return wgpm;
}

void process_wgpm(CLI::App* wgpm)
{
	const std::uint64_t guest_physical_address = get_command_option<std::uint64_t>(wgpm, "physical_address");
	const std::uint64_t size = get_command_option<std::uint64_t>(wgpm, "size");

	std::uint64_t value = get_command_option<std::uint64_t>(wgpm, "value");

	const std::uint64_t bytes_written = hypercall::write_guest_physical_memory(&value, guest_physical_address, size);

	if (bytes_written == size)
	{
		std::println("success in write");
	}
	else
	{
		std::println("failed to write");
	}
}

CLI::App* init_cgpm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* cgpm = app.add_subcommand("cgpm", "copies memory from a given source to a destination (guest physical addresses)")->ignore_case();

	add_transformed_command_option(cgpm, "destination_physical_address", aliases_transformer)->required();
	add_transformed_command_option(cgpm, "source_physical_address", aliases_transformer)->required();
	add_command_option(cgpm, "size")->required();

	return cgpm;
}

void process_cgpm(CLI::App* cgpm)
{
	const std::uint64_t guest_destination_physical_address = get_command_option<std::uint64_t>(cgpm, "destination_physical_address");
	const std::uint64_t guest_source_physical_address = get_command_option<std::uint64_t>(cgpm, "source_physical_address");
	const std::uint64_t size = get_command_option<std::uint64_t>(cgpm, "size");

	std::vector<std::uint8_t> buffer(size);

	const std::uint64_t bytes_read = hypercall::read_guest_physical_memory(buffer.data(), guest_source_physical_address, size);
	const std::uint64_t bytes_written = hypercall::write_guest_physical_memory(buffer.data(), guest_destination_physical_address, size);

	if ((bytes_read == size) && (bytes_written == size))
	{
		std::println("success in copy");
	}
	else
	{
		std::println("failed to copy");
	}
}

CLI::App* init_gvat(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* gvat = app.add_subcommand("gvat", "translates a guest virtual address to its corresponding guest physical address, with the given guest cr3 value")->ignore_case();

	add_transformed_command_option(gvat, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(gvat, "cr3", aliases_transformer)->required();

	return gvat;
}

void process_gvat(CLI::App* gvat)
{
	const std::uint64_t virtual_address = get_command_option<std::uint64_t>(gvat, "virtual_address");
	const std::uint64_t cr3 = get_command_option<std::uint64_t>(gvat, "cr3");

	const std::uint64_t physical_address = hypercall::translate_guest_virtual_address(virtual_address, cr3);

	std::println("physical address: 0x{:x}", physical_address);
}

CLI::App* init_rgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* rgvm = app.add_subcommand("rgvm", "reads memory from a given guest virtual address (when given the corresponding guest cr3 value)")->ignore_case();

	add_transformed_command_option(rgvm, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(rgvm, "cr3", aliases_transformer)->required();
	add_command_option(rgvm, "size")->check(CLI::Range(0, 8))->required();

	return rgvm;
}

void process_rgvm(CLI::App* rgvm)
{
	const std::uint64_t guest_virtual_address = get_command_option<std::uint64_t>(rgvm, "virtual_address");
	const std::uint64_t cr3 = get_command_option<std::uint64_t>(rgvm, "cr3");
	const std::uint64_t size = get_command_option<std::uint64_t>(rgvm, "size");

	std::uint64_t value = 0;

	const std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(&value, guest_virtual_address, cr3, size);

	if (bytes_read == size)
	{
		std::println("value: 0x{:x}", value);
	}
	else
	{
		std::println("failed to read");
	}
}

CLI::App* init_wgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* wgvm = app.add_subcommand("wgvm", "writes memory from a given guest virtual address (when given the corresponding guest cr3 value)")->ignore_case();

	add_transformed_command_option(wgvm, "virtual_address", aliases_transformer)->required();
	add_transformed_command_option(wgvm, "cr3", aliases_transformer)->required();
	add_command_option(wgvm, "value")->required();
	add_command_option(wgvm, "size")->check(CLI::Range(0, 8))->required();

	return wgvm;
}

void process_wgvm(CLI::App* wgvm)
{
	const std::uint64_t guest_virtual_address = get_command_option<std::uint64_t>(wgvm, "virtual_address");
	const std::uint64_t cr3 = get_command_option<std::uint64_t>(wgvm, "cr3");
	const std::uint64_t size = get_command_option<std::uint64_t>(wgvm, "size");

	std::uint64_t value = get_command_option<std::uint64_t>(wgvm, "value");

	const std::uint64_t bytes_written = hypercall::write_guest_virtual_memory(&value, guest_virtual_address, cr3, size);

	if (bytes_written == size)
	{
		std::println("success in write at given address");
	}
	else
	{
		std::println("failed to write at given address");
	}
}

CLI::App* init_cgvm(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* cgvm = app.add_subcommand("cgvm", "copies memory from a given source to a destination (guest virtual addresses) (when given the corresponding guest cr3 values)")->ignore_case();

	add_transformed_command_option(cgvm, "destination_virtual_address", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "destination_cr3", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "source_virtual_address", aliases_transformer)->required();
	add_transformed_command_option(cgvm, "source_cr3", aliases_transformer)->required();
	add_command_option(cgvm, "size")->required();

	return cgvm;
}

void process_cgvm(CLI::App* wgvm)
{
	const std::uint64_t guest_destination_virtual_address = get_command_option<std::uint64_t>(wgvm, "destination_virtual_address");
	const std::uint64_t guest_destination_cr3 = get_command_option<std::uint64_t>(wgvm, "destination_cr3");

	const std::uint64_t guest_source_virtual_address = get_command_option<std::uint64_t>(wgvm, "source_virtual_address");
	const std::uint64_t guest_source_cr3 = get_command_option<std::uint64_t>(wgvm, "source_cr3");

	const std::uint64_t size = get_command_option<std::uint64_t>(wgvm, "size");

	std::vector<std::uint8_t> buffer(size);

	const std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(buffer.data(), guest_source_virtual_address, guest_source_cr3, size);
	const std::uint64_t bytes_written = hypercall::write_guest_virtual_memory(buffer.data(), guest_destination_virtual_address, guest_destination_cr3, size);

	if ((bytes_read == size) && (bytes_written == size))
	{
		std::println("success in copy");
	}
	else
	{
		std::println("failed to copy");
	}
}

CLI::App* init_akh(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* akh = app.add_subcommand("akh", "add a hook on specified kernel code (given the guest virtual address) (asmbytes in form: 0xE8 0x12 0x23 0x34 0x45)")->ignore_case();

	add_transformed_command_option(akh, "virtual_address", aliases_transformer)->required();
	add_command_option(akh, "--asmbytes")->multi_option_policy(CLI::MultiOptionPolicy::TakeAll)->expected(-1);
	add_command_option(akh, "--post_original_asmbytes")->multi_option_policy(CLI::MultiOptionPolicy::TakeAll)->expected(-1);
	add_command_flag(akh, "--monitor");

	return akh;
}

void process_akh(CLI::App* akh)
{
	const std::uint64_t virtual_address = get_command_option<std::uint64_t>(akh, "virtual_address");

	std::vector<uint8_t> asm_bytes = get_command_option<std::vector<uint8_t>>(akh, "--asmbytes");
	const std::vector<uint8_t> post_original_asm_bytes = get_command_option<std::vector<uint8_t>>(akh, "--post_original_asmbytes");

	const std::uint8_t monitor = get_command_flag(akh, "--monitor");

	if (monitor == 1)
	{
		std::array<std::uint8_t, 9> monitor_bytes = {
			0x51, // push rcx
			0xB9, 0x00, 0x00, 0x00, 0x00, // mov ecx, 0
			0x0F, 0xA2, // cpuid
			0x59 // pop rcx
		};

		hypercall_info_t call_info = { };

		call_info.primary_key = hypercall_primary_key;
		call_info.secondary_key = hypercall_secondary_key;
		call_info.call_type = hypercall_type_t::log_current_state;

		*reinterpret_cast<std::uint32_t*>(&monitor_bytes[2]) = static_cast<std::uint32_t>(call_info.value);

		asm_bytes.insert(asm_bytes.end(), monitor_bytes.begin(), monitor_bytes.end());
	}

	const std::uint8_t hook_status = hook::add_kernel_hook(virtual_address, asm_bytes, post_original_asm_bytes);

	if (hook_status == 1)
	{
		std::println("success in hook");
	}
	else
	{
		std::println("failed to hook");
	}
}

CLI::App* init_rkh(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* rkh = app.add_subcommand("rkh", "remove a previously placed hook on specified kernel code (given the guest virtual address)")->ignore_case();

	add_transformed_command_option(rkh, "virtual_address", aliases_transformer)->required();

	return rkh;
}

void process_rkh(CLI::App* rkh)
{
	const std::uint64_t virtual_address = get_command_option<std::uint64_t>(rkh, "virtual_address");

	const std::uint8_t hook_removal_status = hook::remove_kernel_hook(virtual_address, 1);

	if (hook_removal_status == 1)
	{
		std::println("success in hook removal");
	}
	else
	{
		std::println("failed to remove hook");
	}
}

CLI::App* init_hgpp(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* hgpp = app.add_subcommand("hgpp", "hide a physical page's real contents from the guest")->ignore_case();

	add_transformed_command_option(hgpp, "physical_address", aliases_transformer)->required();

	return hgpp;
}

void process_hgpp(CLI::App* hgpp)
{
	const std::uint64_t physical_address = get_command_option<std::uint64_t>(hgpp, "physical_address");

	const std::uint64_t hide_status = hypercall::hide_guest_physical_page(physical_address);

	if (hide_status == 1)
	{
		std::println("success in hiding page");
	}
	else
	{
		std::println("failed to hide page");
	}
}

CLI::App* init_fl(CLI::App& app)
{
	CLI::App* fl = app.add_subcommand("fl", "flush trap frame logs from hooks")->ignore_case();

	return fl;
}

// Helper function to get hypercall type name
std::string get_hypercall_type_name(std::uint64_t call_type)
{
	switch (call_type) {
		case 0: return "guest_physical_memory_operation";
		case 1: return "guest_virtual_memory_operation";
		case 2: return "translate_guest_virtual_address";
		case 3: return "read_guest_cr3";
		case 4: return "add_slat_code_hook";
		case 5: return "remove_slat_code_hook";
		case 6: return "hide_guest_physical_page";
		case 7: return "log_current_state";
		case 8: return "flush_logs";
		case 9: return "get_heap_free_page_count";
		case 10: return "get_process_base_by_pid";
		case 11: return "get_process_cr3";
		case 12: return "check_hyperv_attachment_memory_mapping";
		case 13: return "allocate_hidden_memory";
		case 14: return "free_hidden_memory";
		case 15: return "get_process_eprocess_base";
		case 16: return "call_dllmain_silently";
		case 17: return "dirbase_from_base_address";
		case 18: return "hide_hypervisor_memory";
		case 19: return "restore_hypervisor_memory";
		case 20: return "get_ntoskrnl_base_address";
		case 21: return "query_hypervisor_pfn_info";
		case 22: return "get_hypervisor_memory_info";
		default: return std::format("unknown_hypercall_{}", call_type);
	}
}

// Helper function to interpret common hypervisor log markers
std::string interpret_log_marker(std::uint64_t r15, std::uint64_t r14, std::uint64_t r13)
{
	switch (r15) {
		case 0xC0DE: return std::format("HYPERCALL: {} (type={})", get_hypercall_type_name(r14), r14);
		case 0xEEE1: return "get_ntoskrnl_base_address hypercall entry";
		case 0xEEE2: return "boot address success";
		case 0xEEE3: return "boot failed, trying dynamic discovery";
		case 0xEEE4: return "dynamic discovery success";
		case 0xEEE5: return "complete failure to find ntoskrnl";
		case 0xDDD1: return "IDT discovery start";
		case 0xDDD2: return "IDT failure - no IDT available";
		case 0xDDD3: return "IDT read failure (should not occur)";
		case 0xDDD4: return "Valid IDT entry found";
		case 0xDDD5: return "Successful ntoskrnl discovery via IDT";
		case 0xDDD6: return "Final failure - ntoskrnl not found";
		case 0xDDD7: return "Fallback: estimated ntoskrnl base from kernel address";
		case 0xDE: return "call_dllmain_silently validation failure";
		case 0xD2: return "call_dllmain_silently safe address failure";
		case 0xDF: return "call_dllmain_silently execution start";
		case 0x141241251: return "call_dllmain_silently entry confirmation";
		case 0xE001: return "PE validation attempt";
		case 0xE002: return "MZ signature found";
		case 0xE003: return "Memory read failure during PE validation";
		case 0xF001: return "PFN initialization start";
		case 0xF002: return "PFN initialization failure";
		case 0xF003: return "PFN initialization success";
		case 0xEEE6: return "update_ntoskrnl_base_address hypercall entry";
		case 0xEEE7: return "ntoskrnl base update success";
		case 0xEEE8: return "ntoskrnl base update failure";
		case 0xE010: return "Export resolution start";
		case 0xE011: return "Invalid ntoskrnl base for export";
		case 0xE012: return "DOS header read failed";
		case 0xE013: return "Invalid DOS signature";
		case 0xE014: return "NT headers read failed";
		case 0xE015: return "Invalid NT signature";
		case 0xE016: return "No export directory found";
		case 0xE017: return "Export directory read failed";
		case 0xE018: return "Export found successfully";
		case 0xEEE9: return "set_kernel_cr3 hypercall entry";
		case 0xEEEA: return "kernel CR3 update success";
		case 0xEEEB: return "kernel CR3 update failure";
		case 0xEEEC: return "get_ntoskrnl_base_from_kpcr hypercall entry";
		case 0xEEED: return "KPCR ntoskrnl discovery success";
		case 0xEEEE: return "KPCR ntoskrnl discovery failure";
		case 0xF100: return "KPCR traversal start (kernel CR3 + SLAT)";
		case 0xF101: return "KPCR base read (GS:0x18)";
		case 0xF102: return "KPCR CurrentThread pointer address";
		case 0xF103: return "KPCR CurrentThread pointer value";
		case 0xF104: return "KPCR KTHREAD->Process pointer address";
		case 0xF105: return "KPCR traversal success (current EPROCESS)";
		case 0xF1E0: return "KPCR traversal aborted: missing CR3 context";
		case 0xF1E1: return "KPCR traversal failed: GS read exception";
		case 0xF1E2: return "KPCR traversal failed: KPCR base is null";
		case 0xF1E3: return "KPCR traversal failed: unable to read CurrentThread pointer";
		case 0xF1E4: return "KPCR traversal failed: CurrentThread pointer is null";
		case 0xF1E5: return "KPCR traversal failed: unable to read EPROCESS pointer";
		case 0xF1E6: return "KPCR traversal failed: EPROCESS pointer is null";
		case 0xF200: return "Debug: about to read memory";
		case 0xF201: return "Debug: memory read result";
		case 1: return "allocate_hidden_memory: input validation";
		case 2: return "allocate_hidden_memory: target process CR3";
		case 3: return "allocate_hidden_memory: pre-flight check failed";
		case 0x10: return "allocate_hidden_memory: success";
		case 0x11: return "allocate_hidden_memory: rollback";
		case 0xC300: return std::format("CR3 Cache: Target PID set to {}", r14);
		case 0xC301: return std::format("CR3 Cache: Sample count {} (ring3: {})", r14, r13);
		case 0xC302: return std::format("CR3 Cache: First ring3 sample (cs=0x{:X}, target_pid={})", r14, r13);
		case 0xC303: return std::format("CR3 Cache: PID check (current={}, target={})", r14, r13);
		case 0xC304: return std::format("CR3 Cache: First target PID hit! (pid={})", r14);
		case 0xC305: return std::format("CR3 Cache: CR3 cached! (0x{:X}, update #{})", r14, r13);
		default:
			if (r15 >= 0x20 && r15 <= 0x2F) {
				return std::format("allocate_hidden_memory: page allocation {}", r15 - 0x20);
			}
			if (r15 >= 0x30 && r15 <= 0x39) {
				return std::format("allocate_hidden_memory: allocation failure type {}", r15 - 0x30);
			}
			if (r15 != 0) {
				return std::format("Unknown marker: 0x{:X}", r15);
			}
			return "";
	}
}

void process_fl(CLI::App* fl)
{
	constexpr std::uint64_t log_count = 1000;  // Increased from 100 to 1000
	constexpr std::uint64_t failed_log_count = -1;

	std::vector<trap_frame_log_t> logs(log_count);

	const std::uint64_t logs_flushed = hypercall::flush_logs(logs);

	if (logs_flushed == failed_log_count)
	{
		std::println("failed to flush logs");
	}
	else if (logs_flushed == 0)
	{
		std::println("there are no logs to flush");
	}
	else
	{
		std::println("success in flushing logs ({}), outputting logs now:\n\n", logs_flushed);

		for (std::uint64_t i = 0; i < logs_flushed; i++)
		{
			const trap_frame_log_t& log = logs[i];

			// Interpret the log marker for better readability
			std::string interpretation = interpret_log_marker(log.r15, log.r14, log.r13);
			
			if (!interpretation.empty()) {
				std::println("{}. [{}]", i, interpretation);
			} else {
				std::println("{}. [Standard execution log]", i);
			}

			// Show key registers with better formatting - hide zero values for clarity
			std::println("  rip=0x{:X} cr3=0x{:X}", log.rip, log.cr3);
			
			// Only show non-zero register values to reduce noise
			if (log.rax != 0) std::println("  rax=0x{:X}", log.rax);
			if (log.rcx != 0) std::println("  rcx=0x{:X}", log.rcx);
			if (log.rdx != 0) std::println("  rdx=0x{:X}", log.rdx);
			if (log.rbx != 0) std::println("  rbx=0x{:X}", log.rbx);
			if (log.rsp != 0) std::println("  rsp=0x{:X}", log.rsp);
			if (log.rbp != 0) std::println("  rbp=0x{:X}", log.rbp);
			if (log.rsi != 0) std::println("  rsi=0x{:X}", log.rsi);
			if (log.rdi != 0) std::println("  rdi=0x{:X}", log.rdi);
			if (log.r8 != 0) std::println("  r8=0x{:X}", log.r8);
			if (log.r9 != 0) std::println("  r9=0x{:X}", log.r9);
			if (log.r10 != 0) std::println("  r10=0x{:X}", log.r10);
			if (log.r11 != 0) std::println("  r11=0x{:X}", log.r11);
			if (log.r12 != 0) std::println("  r12=0x{:X}", log.r12);
			if (log.r13 != 0) std::println("  r13=0x{:X}", log.r13);
			if (log.r14 != 0) std::println("  r14=0x{:X}", log.r14);
			if (log.r15 != 0) std::println("  r15=0x{:X}", log.r15);

			// Show stack data only if it contains non-zero values
			bool has_stack_data = false;
			for (const std::uint64_t stack_value : log.stack_data)
			{
				if (stack_value != 0) {
					if (!has_stack_data) {
						std::println("  stack data:");
						has_stack_data = true;
					}
					std::println("    0x{:X}", stack_value);
				}
			}

			std::println();
		}
	}
}

CLI::App* init_hfpc(CLI::App& app)
{
	CLI::App* hfpc = app.add_subcommand("hfpc", "get hyperv-attachment's heap free page count")->ignore_case();

	return hfpc;
}

void process_hfpc(CLI::App* hfpc)
{
	const std::uint64_t heap_free_page_count = hypercall::get_heap_free_page_count();

	std::println("heap free page count: {}", heap_free_page_count);
}

CLI::App* init_lkm(CLI::App& app)
{
	CLI::App* lkm = app.add_subcommand("lkm", "print list of loaded kernel modules")->ignore_case();

	return lkm;
}

void process_lkm(CLI::App* lkm)
{
	for (const auto& [module_name, module_info] : sys::kernel::modules_list)
	{
		std::println("'{}' has a base address of: 0x{:x}, and a size of: 0x{:X}", module_name, module_info.base_address, module_info.size);
	}
}

CLI::App* init_kme(CLI::App& app)
{
	CLI::App* kme = app.add_subcommand("kme", "list the exports of a loaded kernel module (when given the name)")->ignore_case();

	add_command_option(kme, "module_name")->required();

	return kme;
}

void process_kme(CLI::App* kme)
{
	const std::string module_name = get_command_option<std::string>(kme, "module_name");

	if (sys::kernel::modules_list.contains(module_name) == false)
	{
		std::println("module not found");

		return;
	}

	const sys::kernel_module_t module = sys::kernel::modules_list[module_name];

	for (auto& [export_name, export_address] : module.exports)
	{
		std::println("{} = 0x{:X}", export_name, export_address);
	}
}

CLI::App* init_dkm(CLI::App& app)
{
	CLI::App* dkm = app.add_subcommand("dkm", "dump kernel module to a file on disk")->ignore_case();

	add_command_option(dkm, "module_name")->required();
	add_command_option(dkm, "output_directory")->required();

	return dkm;
}

void process_dkm(CLI::App* dkm)
{
	const std::string module_name = get_command_option<std::string>(dkm, "module_name");

	if (sys::kernel::modules_list.contains(module_name) == false)
	{
		std::println("module not found");

		return;
	}

	const std::string output_directory = get_command_option<std::string>(dkm, "output_directory");

	const std::uint8_t status = sys::kernel::dump_module_to_disk(module_name, output_directory);

	if (status == 1)
	{
		std::println("success in dumping module");
	}
	else
	{
		std::println("failed to dump module");
	}
}

CLI::App* init_gva(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* gva = app.add_subcommand("gva", "get the numerical value of an alias")->ignore_case();

	add_transformed_command_option(gva, "alias_name", aliases_transformer)->required();

	return gva;
}

CLI::App* init_gpb(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* gpb = app.add_subcommand("gpb", "get process base address by PID")->ignore_case();

	add_command_option(gpb, "pid")->required();
	// No longer require kernel_cr3 as an argument, we will use the one found at startup
	// add_transformed_command_option(gpb, "kernel_cr3", aliases_transformer);

	return gpb;
}

void process_gva(CLI::App* gva)
{
	const std::uint64_t alias_value = get_command_option<std::uint64_t>(gva, "alias_name");

	std::println("alias value: 0x{:X}", alias_value);
}



void process_gpb(CLI::App* gpb)
{
	uint64_t target_pid = get_command_option<uint64_t>(gpb, "pid");

	// Check if we found PsInitialSystemProcess during initialization.
	if (sys::ps_initial_system_process == 0) {
		std::println("Error: PsInitialSystemProcess not found. Cannot proceed.");
		return;
	}

	// Use the new, reliable hypercall with PsInitialSystemProcess.
	uint64_t process_base = hypercall::get_process_base(target_pid, sys::ps_initial_system_process, sys::current_cr3);

	if (process_base == 0) {
		std::println("process with PID {} not found or failed to get process base", target_pid);
	}
	else {
		std::println("process base address for PID {}: 0x{:X}", target_pid, process_base);
	}
}

	std::unordered_map<std::string, std::uint64_t> form_aliases()
	{
		std::unordered_map<std::string, std::uint64_t> aliases = { 
			{ "current_cr3", sys::current_cr3 },
			{ "mm_physical_memory_block", sys::mm_physical_memory_block }
		};

		for (auto& [module_name, module_info] : sys::kernel::modules_list)
		{
			aliases.insert({ module_name, module_info.base_address });
			aliases.insert(module_info.exports.begin(), module_info.exports.end());
		}

		return aliases;

	
	}
	


CLI::App* init_chkmap(CLI::App& app, CLI::Transformer& aliases_transformer)
{
	CLI::App* chkmap = app.add_subcommand("chkmap", "check if hyperv attachment memory is mapped by OS")->ignore_case();

	// Make parameters optional - will use discovered values as defaults
	add_transformed_command_option(chkmap, "kernel_cr3", aliases_transformer);
	add_transformed_command_option(chkmap, "memory_map_address", aliases_transformer);

	return chkmap;
}

CLI::App* init_vuworld(CLI::App& app)
{
	CLI::App* vuworld = app.add_subcommand("vuworld", "find VALORANT process and scan for UWorld pattern")->ignore_case();

	return vuworld;
}

CLI::App* init_loaddll(CLI::App& app)
{
	CLI::App* loaddll = app.add_subcommand("loaddll", "load a DLL into a target process using regular hypervisor memory allocation")->ignore_case();

	add_command_option(loaddll, "pid")->required();
	add_command_option(loaddll, "dll_path")->required();

	return loaddll;
}

CLI::App* init_stealthdll(CLI::App& app)
{
	CLI::App* stealthdll = app.add_subcommand("stealthdll", "load a DLL into a target process using hidden memory (stealth injection)")->ignore_case();

	add_command_option(stealthdll, "pid")->required();
	add_command_option(stealthdll, "dll_path")->required();

	return stealthdll;
}

CLI::App* init_rs(CLI::App& app)
{
	CLI::App* rs = app.add_subcommand("rs", "test read speed of uint64_t values from usermode process memory")->ignore_case();

	add_command_option(rs, "--iterations")->default_val("10000000");
	add_command_option(rs, "--duration")->default_val("10");
	add_command_option(rs, "--batch_size")->default_val("1024");
	add_command_option(rs, "--threads")->default_val("4");
	add_command_flag(rs, "--optimized");

	return rs;
}

CLI::App* init_notepad_cr3(CLI::App& app)
{
	CLI::App* notepad_cr3 = app.add_subcommand("notepad_cr3", "get CR3 (directory base) of notepad.exe process")->ignore_case();

	return notepad_cr3;
}

CLI::App* init_hide_hv_memory(CLI::App& app)
{
	CLI::App* hide_hv_memory = app.add_subcommand("hide_hv_memory", "hide hypervisor memory from MmPfnDatabase")->ignore_case();
	
	add_command_option(hide_hv_memory, "physical_base")->required();
	add_command_option(hide_hv_memory, "size")->required();

	return hide_hv_memory;
}

CLI::App* init_test_ntoskrnl(CLI::App& app)
{
	CLI::App* test_ntoskrnl = app.add_subcommand("test_ntoskrnl", "test ntoskrnl base address discovery only")->ignore_case();

	return test_ntoskrnl;
}

void process_test_ntoskrnl(CLI::App* test_ntoskrnl)
{
	std::println("=== Testing ntoskrnl Base Address Discovery ===");
	std::println("This test verifies that the hypervisor can find ntoskrnl.exe without usermode help.\n");

	//// Get hypervisor's current ntoskrnl base
	std::uint64_t hypervisor_base = 0;
	//try {
	//	hypervisor_base = hypercall::get_ntoskrnl_base_address();
	//} catch (...) {
	//	std::println("[ERROR] Exception occurred while calling get_ntoskrnl_base_address()");
	//	hypervisor_base = 0;
	//}

	// Get usermode's ntoskrnl base (for comparison only - hypervisor should NOT rely on this)
	void* usermode_base_ptr = nullptr;
	std::uint64_t usermode_base = 0;
	try {
		usermode_base_ptr = sys::get_ntoskrnl_base();
		usermode_base = reinterpret_cast<std::uint64_t>(usermode_base_ptr);
	} catch (...) {
		std::println("[INFO] Usermode cannot discover ntoskrnl (this is OK - hypervisor uses KPCR method)");
		usermode_base = 0;
	}

	std::println("Current hypervisor ntoskrnl base: 0x{:X}", hypervisor_base);
	if (usermode_base != 0) {
		std::println("Usermode discovered ntoskrnl base: 0x{:X}", usermode_base);
	}

	std::println("");

	// Validate addresses are reasonable
	bool hypervisor_valid = (hypervisor_base >= 0xFFFF000000000000ULL);
	bool usermode_valid = (usermode_base >= 0xFFFF000000000000ULL);

	if (hypervisor_base != 0 && !hypervisor_valid) {
		std::println("[ERROR] Hypervisor base address is invalid (not in kernel space): 0x{:X}", hypervisor_base);
		hypervisor_base = 0;
	}
	if (usermode_base != 0 && !usermode_valid) {
		std::println("[WARNING] Usermode base address is invalid (not in kernel space): 0x{:X}", usermode_base);
		usermode_base = 0;
	}

	// Compare results
	if (hypervisor_base != 0 && usermode_base != 0) {
		if (hypervisor_base == usermode_base) {
			std::println("[SUCCESS] Hypervisor and usermode bases match: 0x{:X}", hypervisor_base);
		} else {
			std::println("[INFO] Hypervisor and usermode bases differ:");
			std::println("  Hypervisor: 0x{:X}", hypervisor_base);
			std::println("  Usermode:   0x{:X}", usermode_base);
			std::println("[NOTE] The hypervisor discovered its own base via KPCR (correct behavior)");
		}
	} else if (hypervisor_base != 0) {
		std::println("[SUCCESS] Hypervisor discovered ntoskrnl base autonomously via KPCR!");
	} else {
		std::println("[WARNING] Hypervisor has not yet discovered ntoskrnl base");
	}
	
	std::println("\n=== Testing KPCR-based Discovery (Direct Call) ===");
	std::println("Explicitly calling hypervisor's KPCR → IDT → ntoskrnl scanner...\n");

	std::uint64_t kpcr_discovered = 0;
	try {
		kpcr_discovered = hypercall::get_ntoskrnl_base_from_kpcr();
	} catch (...) {
		std::println("[ERROR] Exception occurred while calling get_ntoskrnl_base_from_kpcr()");
		kpcr_discovered = 0;
	}

	if (kpcr_discovered != 0) {
		if (kpcr_discovered < 0xFFFF000000000000ULL) {
			std::println("[ERROR] KPCR method returned invalid address: 0x{:X}", kpcr_discovered);
		} else {
			std::println("[SUCCESS] KPCR method discovered ntoskrnl: 0x{:X}", kpcr_discovered);

			if (kpcr_discovered == hypervisor_base) {
				std::println("[VERIFIED] Matches the cached hypervisor base!");
			}

			if (usermode_base != 0 && kpcr_discovered == usermode_base) {
				std::println("[VERIFIED] Matches usermode discovery!");
			}
		}
	} else {
		std::println("[ERROR] KPCR method failed to find ntoskrnl base");
	}

	std::println("\n=== Summary ===");
	if (hypervisor_base >= 0xFFFF000000000000ULL) {
		std::println("[RESULT] Hypervisor has valid ntoskrnl base: 0x{:X}", hypervisor_base);
		std::println("[STATUS] Ready for PFN queries and kernel operations");
	} else {
		std::println("[RESULT] Hypervisor does not have a valid ntoskrnl base");
		std::println("[STATUS] PFN queries will fail until ntoskrnl is discovered");
	}

	std::println("\n[TIP] The hypervisor automatically discovers ntoskrnl via KPCR when needed.");
	std::println("[TIP] Try running 'pfn_query_demo' to test PFN database queries.");
}

CLI::App* init_test_exports(CLI::App& app)
{
	CLI::App* test_exports = app.add_subcommand("test_exports", "test if MmGetVirtualForPhysical and MmPfnDatabase can be found")->ignore_case();

	return test_exports;
}

void process_test_exports(CLI::App* test_exports)
{
	std::println("=== Testing Kernel Export Discovery (Hypervisor) ===");
	std::println("This test asks the HYPERVISOR to find MmGetVirtualForPhysical and MmPfnDatabase.\n");

	std::println("[DEBUG] Allocating result structure...");
	export_discovery_test_result_t result = {};

	std::println("[DEBUG] Calling hypercall...");
	std::uint64_t status = 0;

	try {
		status = hypercall::test_export_discovery(&result);
		std::println("[DEBUG] Hypercall returned: 0x{:X}", status);
	} catch (...) {
		std::println("[ERROR] Exception during hypercall!");
		std::println("[TIP] Check 'fl' for hypervisor logs");
		return;
	}

	if (status != 0) {
		std::println("[ERROR] Hypercall failed with status: 0x{:X}", status);
		return;
	}

	std::println("=== Test Results ===\n");

	// 1. ntoskrnl Base
	std::println("1. ntoskrnl Base Discovery:");
	if (result.ntoskrnl_found) {
		std::println("   [SUCCESS] Found at: 0x{:X}", result.ntoskrnl_base);
	} else {
		std::println("   [FAILED] Status: 0x{:X}", result.ntoskrnl_status);
		return;
	}

	// 2. MmGetVirtualForPhysical Export
	std::println("\n2. MmGetVirtualForPhysical Export:");
	if (result.mm_get_virtual_for_physical_found) {
		std::println("   [SUCCESS] Found at: 0x{:X}", result.mm_get_virtual_for_physical_address);

		std::println("\n   First 128 bytes:");
		for (int i = 0; i < 128; i++) {
			std::print("{:02X} ", result.mm_get_virtual_bytes[i]);
			if ((i + 1) % 16 == 0) std::println("");
		}
	} else {
		std::println("   [FAILED] Status: 0x{:X}", result.export_status);
		return;
	}

	// 3. MmPfnDatabase Pattern
	std::println("\n3. MmPfnDatabase Pattern Search:");
	if (result.pattern_found) {
		std::println("   [SUCCESS] Pattern found at offset: +0x{:X}", result.pattern_offset);

		// Dump bytes around the pattern
		if (result.pattern_offset < 120) {
			std::println("\n   Bytes around pattern (offset +0x{:X}):", result.pattern_offset);
			int start = static_cast<int>(result.pattern_offset);
			int end = std::min(start + 32, 128);
			for (int i = start; i < end; i++) {
				std::print("{:02X} ", result.mm_get_virtual_bytes[i]);
				if ((i - start + 1) % 16 == 0) std::println("");
			}
			std::println("");
		}
	} else {
		std::println("   [FAILED] Pattern not found. Status: 0x{:X}", result.pattern_status);
		std::println("   [INFO] The pattern may have changed in your Windows version.");
		std::println("   [INFO] This means MmPfnDatabase cannot be found automatically.");
		return;
	}

	// 4. MmPfnDatabase Address
	std::println("\n4. MmPfnDatabase Address:");
	if (result.mmpfn_database_found) {
		std::println("   [SUCCESS] MmPfnDatabase: 0x{:X}", result.mmpfn_database_address);
	} else {
		std::println("   [FAILED] Status: 0x{:X}", result.mmpfn_status);
		return;
	}

	std::println("\n=== Summary ===");
	std::println("[SUCCESS] All exports and patterns found!");
	std::println("The hypervisor should be able to initialize MmPfnDatabase successfully.");
	std::println("\n[TIP] Try running 'pfn_query_demo' to test PFN database queries.");
}

CLI::App* init_pfn_query_demo(CLI::App& app)
{
	CLI::App* pfn_query_demo = app.add_subcommand("pfn_query_demo", "demonstrate querying hypervisor memory pages in MmPfnDatabase")->ignore_case();

	return pfn_query_demo;
}

CLI::App* init_cr3_cache(CLI::App& app)
{
	CLI::App* cr3_cache = app.add_subcommand("cr3_cache", "monitor and bypass CR3 shuffling for a target process (EAC bypass)")->ignore_case();

	add_command_option(cr3_cache, "process_name")->required();

	return cr3_cache;
}

CLI::App* init_cr3_monitor(CLI::App& app)
{
	CLI::App* cr3_monitor = app.add_subcommand("cr3_monitor", "continuously monitor CR3 shuffles and test memory reads")->ignore_case();

	add_command_option(cr3_monitor, "process_name")->required();
	add_command_option(cr3_monitor, "duration")->check(CLI::Range(1, 300))->default_val(30);

	return cr3_monitor;
}

CLI::App* init_get_world(CLI::App& app)
{
	CLI::App* get_world = app.add_subcommand("get_world", "find UWorld pointer in VALORANT process using CR3 cache")->ignore_case();

	return get_world;
}

void process_chkmap(CLI::App* chkmap)
{
	// Use discovered values as defaults if parameters not provided
	std::uint64_t kernel_cr3 = get_command_option<std::uint64_t>(chkmap, "kernel_cr3");
	std::uint64_t memory_map_address = get_command_option<std::uint64_t>(chkmap, "memory_map_address");

	// If kernel_cr3 not provided, use the discovered current_cr3
	if (kernel_cr3 == 0) {
		kernel_cr3 = sys::current_cr3;
		std::println("Using discovered kernel CR3: 0x{:X}", kernel_cr3);
	}

	// If memory_map_address not provided, use the discovered mm_physical_memory_block
	if (memory_map_address == 0) {
		memory_map_address = sys::mm_physical_memory_block;
		std::println("Using discovered MmPhysicalMemoryBlock: 0x{:X}", memory_map_address);
	}

	// Validate that we have the required values
	if (kernel_cr3 == 0) {
		std::println("Error: No kernel CR3 available. Please provide kernel_cr3 parameter or ensure system initialization found current_cr3.");
		return;
	}

	if (memory_map_address == 0) {
		std::println("Error: No memory map address available. Please provide memory_map_address parameter or ensure system initialization found mm_physical_memory_block.");
		return;
	}

	memory_mapping_check_result_t result = {};
	std::uint64_t status = hypercall::check_hyperv_attachment_memory_mapping(kernel_cr3, memory_map_address, result);

	if (status == 1) {
		std::println("Check Completed: Mapped by OS = {}", result.is_mapped_by_os);
		std::println("Physical Address Checked: 0x{:X}", result.physical_address_checked);
		std::println("Page Count Checked: {}", result.page_count_checked);
		std::println("Verification Status: 0x{:X}", result.verification_status);
	} else {
		std::println("Check Failed");
	}
}

void process_vuworld(CLI::App* vuworld) {
    std::string process_name = "VALORANT-Win64-Shipping.exe";
    uint32_t pid = sys::get_pid_from_process_name(process_name);

    if (pid == 0) {
        std::println("Process not found: {}", process_name);
        return;
    }

    std::println("Found {} with PID: {}", process_name, pid);

    // Get the target process CR3 using the new hypercall
    std::uint64_t process_cr3 = hypercall::get_process_cr3(pid, sys::ps_initial_system_process, sys::current_cr3);
    if (process_cr3 == 0) {
        std::println("Failed to get process CR3 for PID: {}", pid);
        return;
    }
    
    std::println("Process CR3: 0x{:X}", process_cr3);

    uint64_t base_address = sys::get_process_base_address(pid);
    if (base_address == 0) {
        std::println("Failed to get process base address for PID: {}", pid);
        return;
    }

    // Get ntoskrnl.exe base address
    void* ntoskrnl_base = sys::get_ntoskrnl_base();
    if (!ntoskrnl_base) {
        std::println("Failed to get ntoskrnl.exe base address");
        return;
    }

    // Test the new dirbase_from_base_address function
    std::uint64_t dirbase = hypercall::dirbase_from_base_address(reinterpret_cast<void*>(base_address), ntoskrnl_base);
    if (dirbase != 0) {
        std::println("Directory base from base address: 0x{:X}", dirbase);
        std::println("Comparison - Process CR3: 0x{:X}, Dirbase: 0x{:X}", process_cr3, dirbase);
    } else {
        std::println("Failed to get directory base from base address");
    }

    // Use the new get_image_size function with process CR3
    uint32_t image_size = sys::get_image_size(base_address, process_cr3);
    if (image_size == 0) {
        std::println("Failed to get image size for base address: 0x{:X}", base_address);
        return;
    }

    std::println("Base address: 0x{:X}, Image size: 0x{:X}", base_address, image_size);

    // Pattern to be scanned
    std::uint8_t uworld_pattern[] = {
        0x4C, 0x8D, 0x3D, 0xCC, 0xCC, 0xCC, 0xCC, 0x0F,
        0x10, 0x45, 0xCC, 0x83, 0xE1, 0xCC, 0x4C, 0x89,
        0x7D, 0xCC, 0x0F, 0x11, 0x55, 0xCC, 0x41
    };

    std::println("Scanning {} bytes for UWorld pattern...", image_size);
    
    // Use the new scan_memory function with process CR3
    std::uint64_t pattern_address = sys::scan_memory(base_address, image_size, uworld_pattern, sizeof(uworld_pattern), process_cr3);

    if (pattern_address != 0) {
        std::println("UWorld pattern found at: 0x{:X}", pattern_address);
        
        // Calculate the relative offset from base address
        std::uint64_t offset = pattern_address - base_address;
        std::println("Pattern offset from base: +0x{:X}", offset);
        
        // Read the value at the pattern address
        std::uint64_t pattern_value = 0;
        std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
            &pattern_value, pattern_address + 0x0038, process_cr3, sizeof(pattern_value));
            
        if (bytes_read == sizeof(pattern_value)) {
            std::println("Value at pattern address: 0x{:X}", pattern_value);
        } else {
            std::println("Failed to read value from pattern address");
        }
    } else {
        std::println("UWorld pattern not found in VALORANT process memory.");
    }
}



void process_stealthdll(CLI::App* stealthdll)
{
    const std::uint32_t target_pid = get_command_option<std::uint32_t>(stealthdll, "pid");
    const std::string dll_path = get_command_option<std::string>(stealthdll, "dll_path");
    
    // Check if we have the required system information
    if (sys::ps_initial_system_process == 0) {
        std::println("Error: PsInitialSystemProcess not found. Cannot proceed.");
        return;
    }
    
    if (sys::current_cr3 == 0) {
        std::println("Error: Kernel CR3 not found. Cannot proceed.");
        return;
    }
    
    std::println("[STEALTH] Loading DLL: {} into PID: {} using hidden memory", dll_path, target_pid);
    
    // Load the DLL into the target process using stealth injection
    std::uint64_t base_address = dll_loader::load_dll_stealthily(
        target_pid,
        dll_path,
        sys::ps_initial_system_process,
        sys::current_cr3
    );
    
    if (base_address != 0) {
        std::println("[STEALTH] DLL loaded successfully in hidden memory!");
        std::println("[STEALTH] Base address: 0x{:X}", base_address);
        std::println("[STEALTH] DLL is now invisible to normal process enumeration.");
    } else {
        std::println("[STEALTH] Failed to load DLL into target process.");
    }
}

void process_rs(CLI::App* rs)
{
    const std::uint64_t max_iterations = get_command_option<std::uint64_t>(rs, "--iterations");
    const std::uint64_t max_duration_seconds = get_command_option<std::uint64_t>(rs, "--duration");
    
    // Get current process PID
    const std::uint32_t current_pid = GetCurrentProcessId();
    
    // Check if we have the required system information
    if (sys::ps_initial_system_process == 0) {
        std::println("Error: PsInitialSystemProcess not found. Cannot proceed.");
        return;
    }
    
    if (sys::current_cr3 == 0) {
        std::println("Error: Kernel CR3 not found. Cannot proceed.");
        return;
    }
    
    // Get current process CR3
    std::uint64_t process_cr3 = hypercall::get_process_cr3(current_pid, sys::ps_initial_system_process, sys::current_cr3);
    if (process_cr3 == 0) {
        std::println("Failed to get process CR3 for current PID: {}", current_pid);
        return;
    }
    
    // Create a test variable in our process memory
    static std::uint64_t test_variable = 0xDEADBEEFCAFEBABE;
    std::uint64_t test_address = reinterpret_cast<std::uint64_t>(&test_variable);
    
    std::println("=== Read Speed Test ===");
    std::println("Testing address: 0x{:X}", test_address);
    std::println("Process PID: {}", current_pid);
    std::println("Process CR3: 0x{:X}", process_cr3);
    std::println("Max iterations: {}", max_iterations);
    std::println("Max duration: {} seconds", max_duration_seconds);
    std::println();
    
    // Variables for statistics
    std::vector<double> read_times;
    read_times.reserve(max_iterations);
    
    std::uint64_t successful_reads = 0;
    std::uint64_t failed_reads = 0;
    
    const auto start_time = std::chrono::high_resolution_clock::now();
    const auto max_duration = std::chrono::seconds(max_duration_seconds);
    
    // Perform read speed test
    for (std::uint64_t i = 0; i < max_iterations; i++) {
        const auto current_time = std::chrono::high_resolution_clock::now();
        if (current_time - start_time >= max_duration) {
            std::println("Stopping test due to time limit ({} seconds)", max_duration_seconds);
            break;
        }
        
        std::uint64_t read_value = 0;
        
        const auto read_start = std::chrono::high_resolution_clock::now();
        const std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
            &read_value, test_address, process_cr3, sizeof(std::uint64_t));
        const auto read_end = std::chrono::high_resolution_clock::now();
        
        const auto read_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(read_end - read_start);
        const double read_time_ns = static_cast<double>(read_duration.count());
        
        if (bytes_read == sizeof(std::uint64_t)) {
            successful_reads++;
            read_times.push_back(read_time_ns);
            
            // Verify the read value
            if (read_value != test_variable) {
                std::println("Warning: Read value mismatch at iteration {}. Expected: 0x{:X}, Got: 0x{:X}", 
                           i + 1, test_variable, read_value);
            }
        } else {
            failed_reads++;
        }
        
        // Print progress every 1000 iterations
        if ((i + 1) % 1000 == 0) {
            std::println("Progress: {} / {} iterations ({:.1f}%)", 
                       i + 1, max_iterations, 
                       static_cast<double>(i + 1) / max_iterations * 100.0);
        }
    }
    
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Calculate statistics
    if (successful_reads > 0) {
        // Sort read times for percentile calculations
        std::sort(read_times.begin(), read_times.end());
        
        const double total_reads = static_cast<double>(successful_reads);
        const double total_time_seconds = static_cast<double>(total_duration.count()) / 1000.0;
        
        // Calculate average, min, max
        const double avg_time_ns = std::accumulate(read_times.begin(), read_times.end(), 0.0) / total_reads;
        const double min_time_ns = read_times.front();
        const double max_time_ns = read_times.back();
        
        // Calculate percentiles
        const double p50_time_ns = read_times[read_times.size() / 2];
        const double p95_time_ns = read_times[static_cast<size_t>(read_times.size() * 0.95)];
        const double p99_time_ns = read_times[static_cast<size_t>(read_times.size() * 0.99)];
        
        // Calculate reads per second
        const double reads_per_second = total_reads / total_time_seconds;
        
        std::println();
        std::println("=== Results ===");
        std::println("Total duration: {:.2f} seconds", total_time_seconds);
        std::println("Successful reads: {}", successful_reads);
        std::println("Failed reads: {}", failed_reads);
        std::println("Success rate: {:.2f}%", (total_reads / (total_reads + failed_reads)) * 100.0);
        std::println();
        std::println("=== Performance Metrics ===");
        std::println("Reads per second: {:.2f}", reads_per_second);
        std::println("Average read time: {:.2f} ns ({:.2f} μs)", avg_time_ns, avg_time_ns / 1000.0);
        std::println("Minimum read time: {:.2f} ns ({:.2f} μs)", min_time_ns, min_time_ns / 1000.0);
        std::println("Maximum read time: {:.2f} ns ({:.2f} μs)", max_time_ns, max_time_ns / 1000.0);
        std::println();
        std::println("=== Percentiles ===");
        std::println("50th percentile (median): {:.2f} ns ({:.2f} μs)", p50_time_ns, p50_time_ns / 1000.0);
        std::println("95th percentile: {:.2f} ns ({:.2f} μs)", p95_time_ns, p95_time_ns / 1000.0);
        std::println("99th percentile: {:.2f} ns ({:.2f} μs)", p99_time_ns, p99_time_ns / 1000.0);
    } else {
        std::println();
        std::println("=== Results ===");
        std::println("No successful reads completed!");
        std::println("Failed reads: {}", failed_reads);
    }
}

void process_notepad_cr3(CLI::App* notepad_cr3)
{
    std::string process_name = "notepad.exe";
    
    // Get notepad.exe PID
    uint32_t pid = sys::get_pid_from_process_name(process_name);
    if (pid == 0) {
        std::println("Process not found: {}", process_name);
        std::println("Make sure notepad.exe is running before using this command.");
        return;
    }
    
    std::println("Found {} with PID: {}", process_name, pid);
    
    // Get the process base address
    uint64_t base_address = sys::get_process_base_address(pid);
    if (base_address == 0) {
        std::println("Failed to get process base address for PID: {}", pid);
        return;
    }
    
    std::println("Process base address: 0x{:X}", base_address);
    
    // Get ntoskrnl.exe base address
    void* ntoskrnl_base = sys::get_ntoskrnl_base();
    if (!ntoskrnl_base) {
        std::println("Failed to get ntoskrnl.exe base address");
        return;
    }
    
    std::println("ntoskrnl.exe base address: 0x{:X}", reinterpret_cast<uintptr_t>(ntoskrnl_base));
    
    // Test the dirbase_from_base_address function
    std::uint64_t dirbase = hypercall::dirbase_from_base_address(reinterpret_cast<void*>(base_address), ntoskrnl_base);
    
    if (dirbase != 0) {
        std::println("Successfully found CR3 (directory base) for {}: 0x{:X}", process_name, dirbase);
        
        // Verify the CR3 by trying to read from the process base address using this CR3
        std::uint64_t test_value = 0;
        std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
            &test_value, base_address, dirbase, sizeof(test_value));
            
        if (bytes_read == sizeof(test_value)) {
            std::println("CR3 verification successful - read {} bytes from process memory", bytes_read);
            std::println("Value at base address: 0x{:X}", test_value);
        } else {
            std::println("CR3 verification failed - could not read from process memory using found CR3");
        }
    } else {
        std::println("Failed to find CR3 for {}", process_name);
        std::println("This could indicate:");
        std::println("  - The process is not currently mapped in physical memory");
        std::println("  - The dirbase_from_base_address function needs adjustment");
        std::println("  - The ntoskrnl.exe base address is incorrect");
    }
}

void process_hide_hv_memory(CLI::App* hide_hv_memory)
{
    std::uint64_t physical_base = get_command_option<std::uint64_t>(hide_hv_memory, "physical_base");
    std::uint64_t size = get_command_option<std::uint64_t>(hide_hv_memory, "size");
    
    std::println("Attempting to hide hypervisor memory:");
    std::println("  Physical base: 0x{:X}", physical_base);
    std::println("  Size: 0x{:X} bytes ({} pages)", size, (size + 0xFFF) / 0x1000);
    
    // Call the hypercall to hide the memory
    std::uint64_t result = hypercall::hide_hypervisor_memory(physical_base, size);
    
    if (result == 0) { // STATUS_SUCCESS
        std::println("Successfully hidden hypervisor memory from MmPfnDatabase");
        std::println("The specified physical memory range should now be invisible to:");
        std::println("  - Memory scanning tools");
        std::println("  - Kernel memory managers");
        std::println("  - Anti-cheat systems that scan physical memory");
    } else {
        std::println("Failed to hide hypervisor memory. Error code: 0x{:X}", result);
        std::println("Possible reasons:");
        std::println("  - MmPfnDatabase not initialized");
        std::println("  - Invalid physical address range");
        std::println("  - Memory access denied");
        std::println("  - ntoskrnl.exe exports not found");
    }
}

// Helper function to print page location as string
const char* get_page_location_string(std::uint8_t page_location) {
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
const char* get_cache_attribute_string(std::uint8_t cache_attr) {
    switch (cache_attr) {
        case 0: return "MiNonCached";
        case 1: return "MiCached";
        case 2: return "MiWriteCombined";
        case 3: return "MiHardwareCoherentCached";
        default: return "Unknown";
    }
}

void process_pfn_query_demo(CLI::App* pfn_query_demo)
{
    std::println("\n=== Hypervisor PFN Query Demo ===\n");
    
    // First, get the ntoskrnl base address to show it's working
    std::uint64_t ntoskrnl_base = hypercall::get_ntoskrnl_base_address();
    std::println("ntoskrnl.exe base address: 0x{:X}", ntoskrnl_base);
    
    if (!ntoskrnl_base) {
        std::println("Failed to get ntoskrnl base address!");
        return;
    }
    
    // Get the actual hypervisor memory layout
    hypervisor_memory_info_t memory_info = {};
    std::uint64_t result = hypercall::get_hypervisor_memory_info(&memory_info);
    
    if (result != 0) {
        std::println("Failed to get hypervisor memory info! Status: 0x{:X}", result);
        return;
    }
    
    if (!memory_info.is_valid) {
        std::println("Hypervisor memory info is not valid!");
        return;
    }
    
    std::println("\nHypervisor Memory Layout:");
    std::println("  Main Attachment: 0x{:X} - 0x{:X} ({} pages, {} KB)", 
                memory_info.physical_base, 
                memory_info.physical_base + memory_info.size_bytes,
                memory_info.page_count, 
                memory_info.size_bytes / 1024);
    
    if (memory_info.heap_physical_base != 0) {
        std::println("  Heap Memory: 0x{:X} - 0x{:X} ({} pages, {} KB)", 
                    memory_info.heap_physical_base, 
                    memory_info.heap_physical_base + memory_info.heap_size_bytes,
                    memory_info.heap_page_count, 
                    memory_info.heap_size_bytes / 1024);
    }
    
    if (memory_info.uefi_boot_base != 0) {
        std::println("  UEFI Boot: 0x{:X} - 0x{:X} ({} pages, {} KB)", 
                    memory_info.uefi_boot_base, 
                    memory_info.uefi_boot_base + memory_info.uefi_boot_size,
                    memory_info.uefi_boot_size / 4096, 
                    memory_info.uefi_boot_size / 1024);
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
    
    std::println("\nQuerying {} actual hypervisor memory pages...\n", hypervisor_addresses.size());
    
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
        
        std::println("\n--- Querying Physical Address: 0x{:X} ({}) ---", physical_addr, region_type);
        
        std::uint64_t result = hypercall::query_hypervisor_pfn_info(physical_addr, &pfn_info);
        
        if (result == 0) { // STATUS_SUCCESS
            std::println("Query Status: SUCCESS");
            std::println("PFN Index: 0x{:X}", pfn_info.pfn_index);
            std::println("Is Hypervisor Page: {}", pfn_info.is_hypervisor_page ? "YES" : "NO");
            std::println("Is Valid: {}", pfn_info.is_valid ? "YES" : "NO");
            
            std::println("\nPFN Details:");
            std::println("  Reference Count: {}", pfn_info.reference_count);
            std::println("  Share Count: {}", pfn_info.share_count);
            std::println("  Page Location: {} ({})", 
                        get_page_location_string(pfn_info.e1_flags.page_location),
                        static_cast<int>(pfn_info.e1_flags.page_location));
            std::println("  Cache Attribute: {} ({})", 
                        get_cache_attribute_string(pfn_info.e1_flags.cache_attribute),
                        static_cast<int>(pfn_info.e1_flags.cache_attribute));
            
            std::println("\nFlags:");
            std::println("  Modified: {}", pfn_info.e1_flags.modified ? "YES" : "NO");
            std::println("  Write In Progress: {}", pfn_info.e1_flags.write_in_progress ? "YES" : "NO");
            std::println("  Read In Progress: {}", pfn_info.e1_flags.read_in_progress ? "YES" : "NO");
            std::println("  Priority: {}", static_cast<int>(pfn_info.e3_flags.priority));
            std::println("  On Prototype PTE: {}", pfn_info.e3_flags.on_prototype_pte ? "YES" : "NO");
            std::println("  Removal Requested: {}", pfn_info.e3_flags.removal_requested ? "YES" : "NO");
            std::println("  Parity Error: {}", pfn_info.e3_flags.parity_error ? "YES" : "NO");
            
            std::println("\nPTE Information:");
            std::println("  PTE Address: 0x{:X}", pfn_info.pte_address);
            std::println("  PTE Frame: 0x{:X}", pfn_info.pte_frame);
            
            std::println("\nList Entry:");
            std::println("  Flink: 0x{:X}", pfn_info.list_entry.flink);
            std::println("  Blink: 0x{:X}", pfn_info.list_entry.blink);
            
            std::println("\nOther:");
            std::println("  Page Color: {}", pfn_info.page_color);
            std::println("  Node Blink: {}", pfn_info.node_blink);
            
            // Print first few bytes of raw PFN data for debugging
            std::print("\nRaw PFN Data (first 16 bytes): ");
            for (int i = 0; i < 16; i++) {
                std::print("{:02X} ", static_cast<int>(pfn_info.raw_pfn_data[i]));
            }
            std::println("");
            
        } else {
            std::println("Query failed with status: 0x{:X}", result);
        }
    }
    
    std::println("\n=== Analysis Summary ===");
    std::println("This shows you exactly how Windows sees your hypervisor memory pages!");
    std::println("Key things to look for:");
    std::println("  - Reference Count: Should be > 0 for active pages");
    std::println("  - Page Location: Shows which memory list the page is on");
    std::println("  - Modified Bit: Indicates if the page has been written to");
    std::println("  - Cache Attributes: How the page is cached by the CPU");
    std::println("  - Is Hypervisor Page: Whether our detection logic identifies it");
    std::println("\n=== Demo Complete ===");
}

void process_cr3_cache(CLI::App* cr3_cache)
{
	const std::string process_name = get_command_option<std::string>(cr3_cache, "process_name");

	std::println("\n=== CR3 Cache / Shuffling Bypass ===");
	std::println("Target process: {}", process_name);

	// Get the target process PID
	uint32_t target_pid = sys::get_pid_from_process_name(process_name);
	if (target_pid == 0) {
		std::println("[ERROR] Process '{}' not found!", process_name);
		std::println("[TIP] Make sure the process is running before using this command.");
		return;
	}

	std::println("[SUCCESS] Found process with PID: {}", target_pid);

	// Set the target PID for CR3 caching
	std::println("\n[STEP 1] Setting target PID for CR3 caching...");
	std::uint64_t set_pid_result = hypercall::set_target_pid_for_cr3_caching(target_pid);
	if (set_pid_result == 0) { // STATUS_SUCCESS
		std::println("[SUCCESS] Target PID {} set for CR3 caching", target_pid);
		std::println("  - Hypervisor will opportunistically cache CR3 on ring3 vmexits");
		std::println("  - No performance impact - uses existing vmexit flow");
	} else {
		std::println("[ERROR] Failed to set target PID (0x{:X})", set_pid_result);
		return;
	}

	// Wait for CR3 caching to populate
	std::println("\n[STEP 2] Waiting for CR3 to be cached...");
	std::println("[INFO] Hypervisor will cache CR3 when target process executes in ring3");
	std::println("[INFO] This happens naturally during vmexits, usually within a few seconds");
	std::println("[INFO] Checking every 500ms for up to 30 seconds...\n");

	bool cr3_found = false;
	std::uint64_t cached_cr3 = 0;

	for (int attempt = 0; attempt < 60; attempt++) {
		// Try to get the cached CR3
		cached_cr3 = hypercall::get_cached_cr3();

		if (cached_cr3 != 0) {
			cr3_found = true;
			std::println("\n[SUCCESS] CR3 captured after {} seconds!", attempt / 2);
			break;
		}

		// Print progress
		if (attempt % 2 == 0) {
			std::print(".");
			if (attempt % 20 == 0 && attempt != 0) {
				std::println(" ({} seconds)", attempt / 2);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	std::println("");

	if (!cr3_found) {
		std::println("[WARNING] CR3 not yet cached after 30 seconds");
		std::println("[INFO] This could mean:");
		std::println("  - The process hasn't performed a CR3 write in ring 3 yet");
		std::println("  - The process is not using CR3 shuffling");
		std::println("  - Try interacting with the process to trigger a context switch");
	} else {
		std::println("\n[SUCCESS] Cached CR3 for PID {}: 0x{:X}", target_pid, cached_cr3);
		std::println("[INFO] This CR3 value bypasses any CR3 shuffling protection!");
		std::println("[INFO] You can now use this CR3 to read/write process memory reliably");
	}

	// Get statistics
	std::println("\n[STEP 3] Fetching cache statistics...");
	hypercall::cr3_stats_t stats = {};
	std::uint64_t stats_result = hypercall::get_cr3_cache_stats(&stats);

	if (stats_result == 0) { // STATUS_SUCCESS
		std::println("\n=== CR3 Cache Statistics ===");
		std::println("Total vmexit samples: {}", stats.total_samples);
		std::println("Ring3 samples: {}", stats.ring3_samples);
		std::println("Target PID hits: {}", stats.target_pid_hits);
		std::println("CR3 updates: {}", stats.cr3_updates);

		if (stats.cr3_updates > 0) {
			std::println("\n[SUCCESS] CR3 caching is working!");
			std::println("[INFO] The hypervisor has cached the CR3 {} times", stats.cr3_updates);
		}
	} else {
		std::println("[WARNING] Failed to get statistics (0x{:X})", stats_result);
	}

	// Verify the cached CR3 works
	if (cr3_found) {
		std::println("\n[STEP 5] Verifying cached CR3...");

		// Get process base address
		uint64_t base_address = sys::get_process_base_address(target_pid);
		if (base_address != 0) {
			std::println("[INFO] Process base address: 0x{:X}", base_address);

			// Try to read from the process using the cached CR3
			std::uint64_t test_value = 0;
			std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
				&test_value, base_address, cached_cr3, sizeof(test_value));

			if (bytes_read == sizeof(test_value)) {
				std::println("[SUCCESS] Memory read successful using cached CR3!");
				std::println("[INFO] Read value at base: 0x{:X}", test_value);
				std::println("[INFO] The CR3 is valid and can be used for memory operations");
			} else {
				std::println("[WARNING] Failed to read memory using cached CR3");
				std::println("[INFO] The CR3 might be stale or the address is invalid");
			}
		}
	}

	// Continuous CR3 shuffle monitoring test
	if (cr3_found) {
		std::println("\n[STEP 6] Starting continuous CR3 shuffle monitoring...");
		std::println("[INFO] This will monitor for CR3 shuffles and test memory reads");
		std::println("[INFO] Press Ctrl+C to stop monitoring\n");

		uint64_t base_address = sys::get_process_base_address(target_pid);
		if (base_address == 0) {
			std::println("[ERROR] Could not get process base address for monitoring");
			return;
		}

		std::println("[INFO] Monitoring PID {} with base address 0x{:X}", target_pid, base_address);
		std::println("[INFO] Testing memory reads every 1 second for 30 seconds...\n");

		for (int test_round = 0; test_round < 30; test_round++) {
			// Get current cached CR3
			std::uint64_t current_cr3 = hypercall::get_cached_cr3();

			if (current_cr3 != 0) {
				// Try to read from the process using the current CR3
				std::uint64_t test_value = 0;
				std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
					&test_value, base_address, current_cr3, sizeof(test_value));

				if (bytes_read == sizeof(test_value)) {
					std::println("[SUCCESS] Round {}: CR3=0x{:X}, Read=0x{:X}",
						test_round + 1, current_cr3, test_value);
				} else {
					std::println("[FAILED] Round {}: CR3=0x{:X}, Read failed (bytes={})",
						test_round + 1, current_cr3, bytes_read);
				}
			} else {
				std::println("[WAITING] Round {}: No cached CR3 available yet", test_round + 1);
			}

			// Get updated statistics
			hypercall::cr3_stats_t current_stats = {};
			hypercall::get_cr3_cache_stats(&current_stats);

			if (current_stats.cr3_updates > 0) {
				std::println("[INFO] CR3 updates: {}, Ring3 samples: {}, Target PID hits: {}",
					current_stats.cr3_updates, current_stats.ring3_samples, current_stats.target_pid_hits);
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		std::println("\n[INFO] Continuous monitoring completed");
	}

	std::println("\n=== Summary ===");
	std::println("CR3 caching is now ACTIVE for process: {}", process_name);
	if (cr3_found) {
		std::println("Cached CR3: 0x{:X}", cached_cr3);
		std::println("[TIP] Use this CR3 with rgvm/wgvm commands to bypass CR3 shuffling");
		std::println("[TIP] Example: rgvm 0xADDRESS 0x{:X} 8", cached_cr3);
	} else {
		std::println("[INFO] CR3 not yet cached - wait for the process to perform CR3 writes");
	}
	std::println("\n[INFO] CR3 monitoring will continue in the background");
	std::println("[INFO] Use 'cr3_cache {}' again to check for updates", process_name);
	std::println("[INFO] Use 'fl' to see detailed hypervisor logs");
}

void process_cr3_monitor(CLI::App* cr3_monitor)
{
	const std::string process_name = get_command_option<std::string>(cr3_monitor, "process_name");
	const int duration = get_command_option<int>(cr3_monitor, "duration");

	std::println("\n=== CR3 Shuffle Monitor ===");
	std::println("Target process: {}", process_name);
	std::println("Monitoring duration: {} seconds", duration);

	// Get the target process PID
	uint32_t target_pid = sys::get_pid_from_process_name(process_name);
	if (target_pid == 0) {
		std::println("[ERROR] Process '{}' not found!", process_name);
		std::println("[TIP] Make sure the process is running before using this command.");
		return;
	}

	std::println("[SUCCESS] Found process with PID: {}", target_pid);

	// Set target PID for CR3 caching
	std::println("\n[STEP 1] Setting target PID for CR3 caching...");
	std::uint64_t set_pid_result = hypercall::set_target_pid_for_cr3_caching(target_pid);
	if (set_pid_result != 0) {
		std::println("[ERROR] Failed to set target PID (0x{:X})", set_pid_result);
		return;
	}
	std::println("[SUCCESS] Target PID set! Hypervisor will cache CR3 on next vmexit in ring3");

	// Get process base address
	uint64_t base_address = sys::get_process_base_address(target_pid);
	if (base_address == 0) {
		std::println("[ERROR] Could not get process base address");
		return;
	}

	std::println("[INFO] Process base address: 0x{:X}", base_address);
	std::println("\n[STEP 2] Starting continuous monitoring...");
	std::println("[INFO] Testing memory reads every 1 second for {} seconds...\n", duration);

	int successful_reads = 0;
	int failed_reads = 0;
	std::uint64_t last_cr3 = 0;
	int cr3_changes = 0;

	for (int test_round = 0; test_round < duration; test_round++) {
		// Get current cached CR3
		std::uint64_t current_cr3 = hypercall::get_cached_cr3();
		
		// Check if CR3 changed
		if (current_cr3 != 0 && current_cr3 != last_cr3) {
			cr3_changes++;
			last_cr3 = current_cr3;
		}
		
		if (current_cr3 != 0) {
			// Try to read from the process using the current CR3
			std::uint64_t test_value = 0;
			std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
				&test_value, base_address, current_cr3, sizeof(test_value));

			if (bytes_read == sizeof(test_value)) {
				std::println("[SUCCESS] Round {}: CR3=0x{:X}, Read=0x{:X}", 
					test_round + 1, current_cr3, test_value);
				successful_reads++;
			} else {
				std::println("[FAILED] Round {}: CR3=0x{:X}, Read failed (bytes={})", 
					test_round + 1, current_cr3, bytes_read);
				failed_reads++;
			}
		} else {
			std::println("[WAITING] Round {}: No cached CR3 available yet", test_round + 1);
		}

		// Get updated statistics every 5 rounds
		if (test_round % 5 == 0) {
			hypercall::cr3_stats_t current_stats = {};
			hypercall::get_cr3_cache_stats(&current_stats);

			if (current_stats.cr3_updates > 0) {
				std::println("[INFO] CR3 cache updates: {}, Ring3 samples: {}, Target PID hits: {}",
					current_stats.cr3_updates, current_stats.ring3_samples, current_stats.target_pid_hits);
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// Final statistics
	std::println("\n=== Monitoring Results ===");
	std::println("Total test rounds: {}", duration);
	std::println("Successful memory reads: {}", successful_reads);
	std::println("Failed memory reads: {}", failed_reads);
	std::println("CR3 changes detected: {}", cr3_changes);

	if (successful_reads > 0) {
		std::println("\n[SUCCESS] CR3 shuffle bypass is working!");
		std::println("[INFO] Successfully read memory {} times using cached CR3 values", successful_reads);
		if (cr3_changes > 0) {
			std::println("[INFO] Detected {} CR3 changes, proving shuffling is being bypassed", cr3_changes);
		}
	} else {
		std::println("\n[WARNING] No successful memory reads completed");
		std::println("[INFO] This could mean:");
		std::println("  - The process hasn't performed CR3 writes in ring 3 yet");
		std::println("  - The process is not using CR3 shuffling");
		std::println("  - Try interacting with the process to trigger context switches");
	}

	std::println("\n[INFO] Monitoring completed");
}

typedef struct _SYSTEM_MODULE {
	PVOID  Reserved1;
	PVOID  Reserved2;
	PVOID  ImageBase;
	ULONG  ImageSize;
	ULONG  Flags;
	USHORT Index;
	USHORT Unknown;
	USHORT LoadCount;
	USHORT ModuleNameOffset;
	CHAR   ImageName[256];
} SYSTEM_MODULE, * PSYSTEM_MODULE;

typedef struct _SYSTEM_MODULE_INFORMATION {
	ULONG NumberOfModules;
	SYSTEM_MODULE Modules[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

// Helper function to get module base address
uintptr_t get_module_base(const char* module) {
	ULONG size = 0;
	NTSTATUS status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)0x0B, nullptr, 0, &size);

	if (status != 0xC0000004)
		return 0;

	std::vector<BYTE> buffer(size);
	PSYSTEM_MODULE_INFORMATION moduleInfo = reinterpret_cast<PSYSTEM_MODULE_INFORMATION>(buffer.data());

	status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)0x0B, moduleInfo, size, &size);
	if (!NT_SUCCESS(status)) {
		return 0;
	}

	for (ULONG i = 0; i < moduleInfo->NumberOfModules; i++) {
		SYSTEM_MODULE& mod = moduleInfo->Modules[i];
		const char* moduleName = strrchr(mod.ImageName, '\\');
		if (moduleName) moduleName++; else moduleName = mod.ImageName;
		if (_stricmp(moduleName, module) == 0)
			return (uintptr_t)mod.ImageBase;
	}
	return 0;
}

// ShadowRegionsDataStructure structure
typedef struct ShadowRegionsDataStructure
{
	uintptr_t OriginalPML4_t;
	uintptr_t ClonedPML4_t;
	uintptr_t GameCr3;
	uintptr_t ClonedCr3;
	uintptr_t FreeIndex;

} ShadowRegionsDataStructure;

// Helper

uintptr_t decrypted_cloned_cr3(uintptr_t vgkbase, bool hvci) {
	uint64_t _RAX = 1;
	__cpuid((int*)&_RAX, 1); // Execute CPUID with EAX=1

	uint8_t byte_mask;
	uintptr_t key;

	if (hvci) {
		// Read byte_mask from vgkbase + 0x839C0
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			&byte_mask, vgkbase + 0x839C0, sys::current_cr3, sizeof(byte_mask));
		if (bytes_read != sizeof(byte_mask)) {
			std::println("[ERROR] Failed to read byte_mask");
			return 0;
		}
		
		// Read key from vgkbase + 0x83910
		bytes_read = hypercall::read_guest_virtual_memory(
			&key, vgkbase + 0x83910, sys::current_cr3, sizeof(key));
		if (bytes_read != sizeof(key)) {
			std::println("[ERROR] Failed to read key");
			return 0;
		}
	}
	else {
		// Read byte_mask from vgkbase + 0x839C0
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			&byte_mask, vgkbase + 0x839C0, sys::current_cr3, sizeof(byte_mask));
		if (bytes_read != sizeof(byte_mask)) {
			std::println("[ERROR] Failed to read byte_mask");
			return 0;
		}
		
		// Read key from vgkbase + 0x83910
		bytes_read = hypercall::read_guest_virtual_memory(
			&key, vgkbase + 0x83910, sys::current_cr3, sizeof(key));
		if (bytes_read != sizeof(key)) {
			std::println("[ERROR] Failed to read key");
			return 0;
		}
	}

	uint64_t mask = byte_mask & 0x73;
	uint64_t v11_neg = ~(int64_t)(int)_RAX;

	// Updated v12 calculation with new constants and operations
	uint64_t v12 =
		(0x8000000000000001ULL * (int)_RAX +
			0xAFDBF65F8A4AC9C9ULL * ~key +
			0x2FDBF65F8A4AC9C9ULL * (key + 1) +
			(((key ^ (int)_RAX) << 63) ^ 0x8000000000000000ULL))
		* (0x7D90DC33C620C593ULL * key * (0x13D0F34E00000000ULL * key + 0x483C4F8900000000ULL) +
			0xFD90DC33C620C592ULL * ~(key * (0x13D0F34E00000000ULL * key + 0x483C4F8900000000ULL)) +
			(key *
				(0xCE3CE5E180000000ULL * ~key +
					0x55494E5B80000000ULL * (int)_RAX +
					0xC83B18136241A38DULL * v11_neg +
					0x72F1C9B7E241A38DULL *
					(((int)_RAX | 0x3F71D992FBB2CCEBULL) - (0x3F71D992FBB2CCEAULL - ((int)_RAX & 0x3F71D992FBB2CCEBULL))))
				+ 0x71C31A1E80000000ULL)
			* (0x99BF7D2380CF6EC3ULL * (int)_RAX +
				0x664082DC7F30913ELL * (mask | key) +
				0x19BF7D2380CF6EC2ULL * v11_neg +
				0xE64082DC7F30913ELL * (~key & ~mask) +
				((key + (mask | (int)_RAX) + (mask & (key ^ (int)_RAX))) << 63))
			+ 0x2183995CC620C592ULL);

	// Updated expr calculation
	uint64_t expr =
		0x49B74B6480000000ULL * key +
		0xC2D8B464B4418C6CULL * ~mask +
		0x66B8CDC1FFFFFFFFULL * mask +
		0x5C1FE6A2B4418C6DULL * ((mask & key) - (key | ~mask));

	// Updated logic calculation with new constant
	uint64_t logic =
		-3 * ~key +
		(mask ^ key) +
		-2 * (mask ^ (mask | key)) +
		2 * (((byte_mask & 0x10 | 0x3F71D992FBB2CCEBULL) ^ 0xC08E266D044D3314ULL) + (~key | (mask ^ 0x3F71D992FBB2CCEBULL))) -
		(mask ^ ~key ^ 0x3F71D992FBB2CCEBULL) -
		0x3F71D992FBB2CCEBULL;

	// Final decryption - structure remains similar
	uintptr_t DecryptedCR3 =
		0x137FEEF6AB38CFB4ULL * (expr * logic) +
		((~(expr * logic) ^ ~v12) << 63) +
		0x6C80110954C7304DULL *
		(
			((int)_RAX & (expr * logic)) -
			(~(int)_RAX & ~(expr * logic)) -
			(int)_RAX
			) -
		0x7FFFFFFFFFFFFFFFULL * v12 -
		0x4F167C5CD4C7304EULL;

	return DecryptedCR3;
}

// Helper function to find PML4 base (HVCI version only)
uintptr_t find_pml4_base(uintptr_t vgk_base, bool hvci) {
	ShadowRegionsDataStructure Data;

	uint64_t kernel_cr3 = hypercall::get_process_cr3(4, sys::ps_initial_system_process, sys::current_cr3);

	if (hvci) {
		// Use kernel CR3 to get ShadowRegions
		
			
		std::println("[DEBUG] HVCI mode - using kernel CR3: 0x{:X}", kernel_cr3);
		std::println("[DEBUG] Reading ShadowRegionsDataStructure from vgk_base +  0x838F8: 0x{:X}", vgk_base + 0x838F8);
		
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			&Data, vgk_base + 0x838F8, kernel_cr3, sizeof(ShadowRegionsDataStructure));
		
		if (bytes_read != sizeof(ShadowRegionsDataStructure)) {
			std::println("[ERROR] Failed to read ShadowRegionsDataStructure with kernel CR3");
			return 0;
		}

		uintptr_t DecryptedClonedCR3 = decrypted_cloned_cr3(vgk_base, hvci);
		if (!DecryptedClonedCR3) {
			std::println("[ERROR] Failed to decrypt cloned CR3");
			return 0;
		}

		std::println("[DEBUG] DecryptedClonedCR3: 0x{:X}", DecryptedClonedCR3);
		// Note: In a real implementation, you would switch the CR3 here
		// For now, we'll continue with the kernel CR3
	} else {
		// Normal operation for HVCI off
		std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(
			&Data, vgk_base + 0x838F8, kernel_cr3, sizeof(ShadowRegionsDataStructure));
		
		if (bytes_read != sizeof(ShadowRegionsDataStructure)) {
			std::println("[ERROR] Failed to read ShadowRegionsDataStructure");
			return 0;
		}

		uintptr_t DecClonedCr3 = decrypted_cloned_cr3(vgk_base, hvci);
		if (!DecClonedCr3) {
			std::println("[ERROR] Failed to decrypt cloned CR3");
			return 0;
		}
	}

	std::println("[DEBUG] ShadowRegionsDataStructure contents:");
	std::println("[DEBUG]   OriginalPML4_t: 0x{:X}", Data.OriginalPML4_t);
	std::println("[DEBUG]   ClonedPML4_t: 0x{:X}", Data.ClonedPML4_t);
	std::println("[DEBUG]   GameCr3: 0x{:X}", Data.GameCr3);
	std::println("[DEBUG]   ClonedCr3: 0x{:X}", Data.ClonedCr3);
	std::println("[DEBUG]   FreeIndex: 0x{:X}", Data.FreeIndex);

	uintptr_t source = Data.FreeIndex << 39;
	std::println("[DEBUG] PML4 source (FreeIndex << 39): 0x{:X}", source);
	
	return source;
}

void process_get_world(CLI::App* get_world)
{
	// Define offsets
	constexpr uintptr_t uworld = 0xC0;
	constexpr uintptr_t persistent_level = 0x0038;
	
	// Target process name
	std::string process_name = "VALORANT-Win64-Shipping.exe";
	
	// Get the target process PID
	uint32_t target_pid = sys::get_pid_from_process_name(process_name);
	if (target_pid == 0) {
		std::println("[ERROR] Process '{}' not found!", process_name);
		std::println("[TIP] Make sure VALORANT is running before using this command.");
		return;
	}
	
	auto base = hypercall::get_process_base(target_pid, sys::ps_initial_system_process, sys::current_cr3);
	if (base == 0) {
		std::println("[ERROR] Failed to get process base address for PID: {}", target_pid);
		return;
	}
	
	std::println("[SUCCESS] Found {} with PID: {}", process_name, target_pid);
	std::println("[INFO] Process base address: 0x{:X}", base);
	
	// Get vgk.sys module base
	uintptr_t vgk_base = get_module_base("vgk.sys");
	if (vgk_base == 0) {
		std::println("[ERROR] Failed to get vgk.sys module base address");
		return;
	}
	
	std::println("[INFO] vgk.sys base address: 0x{:X}", vgk_base);
	

	
	
	// Get the target process CR3 using the cached CR3
	std::uint64_t process_cr3 = hypercall::get_cached_cr3();
	if (process_cr3 == 0) {
		std::println("[ERROR] No cached CR3 available for target process");
		std::println("[TIP] Run 'cr3_cache {}' first to cache the process CR3", process_name);
		return;
	}
	
	std::println("[INFO] Using cached CR3: 0x{:X}", process_cr3);
	
	// Scan for UWorld in PML4 entries
	uintptr_t world_pointer = 0;
	int valid_candidates = 0;
	int total_attempts = 0;
	
	std::println("[DEBUG] Scanning {} entries (0x500 / 8)", 0x500 / 8);
	
	std::uint64_t bytes_read;

		// Read the owning world
		uintptr_t Gworld = 0;
		bytes_read = hypercall::read_guest_virtual_memory(
			&Gworld, base + 0xb578310, process_cr3, sizeof(Gworld));
			
		if (bytes_read != sizeof(Gworld)) 
		{
			
	std::println("[DEBUG] Failed to read owning world for candidate 0x{:X}: {} bytes", Gworld, bytes_read);
		
		}
		
    std::println("[DEBUG] Gworld 0x{:X}: {} bytes", Gworld, bytes_read);

	bytes_read = hypercall::read_guest_virtual_memory(
		&Gworld, Gworld, process_cr3, sizeof(Gworld));

	if (bytes_read != sizeof(Gworld))
	{

		std::println("[DEBUG] Failed to read owning world for candidate 0x{:X}: {} bytes", Gworld, bytes_read);

	}

	

	uintptr_t Glevel = 0;

	bytes_read = hypercall::read_guest_virtual_memory(
		&Glevel, Gworld + 0x0038, process_cr3, sizeof(Glevel));

	if (bytes_read != sizeof(Glevel))
	{

		std::println("[DEBUG] Failed to read Glevel for candidate 0x{:X}: {} bytes", Glevel, bytes_read);

	}
	std::println("[DEBUG] Glevel 0x{:X}: {} bytes", Glevel, bytes_read);

	// Read the actors array as TArray
	TArray Actors;
	bytes_read = hypercall::read_guest_virtual_memory(
		&Actors, Glevel + Offsets::AActorArray, process_cr3, sizeof(Actors));

	if (bytes_read != sizeof(Actors))
	{
		std::println("[DEBUG] Failed to read Actors array: {} bytes", bytes_read);
		return;
	}
	
	std::println("[DEBUG] Actors array - Array: 0x{:X}, Count: {}, MaxCount: {}", 
		Actors.GetAddress(), Actors.Size(), Actors.MaxCount);

	if (!Actors.IsValid())
	{
		std::println("[ERROR] Actors array is not valid");
		return;
	}

	std::println("[INFO] Found {} actors in the world", Actors.Size());

	// Iterate through all actors
	for (int i = 0; i < Actors.Size(); i++)
	{
		auto CurrentActor = Actors[i];
		
		if (CurrentActor == 0)
		{
			continue; // Skip null actors
		}

		// Read RootComponent
		uint64_t RootComponent = 0;
		hypercall::read_guest_virtual_memory(&RootComponent, CurrentActor + Offsets::RootComponent, process_cr3, sizeof(RootComponent));
		
		if (RootComponent == 0)
		{
			continue; // Skip actors without root component
		}

		// Read world position
		Vector3 WorldPos;
		hypercall::read_guest_virtual_memory(&WorldPos, RootComponent + Offsets::RelativeLocation, process_cr3, sizeof(WorldPos));

		// Read health if available
		float Health = 0.0f;
		hypercall::read_guest_virtual_memory(&Health, CurrentActor + Offsets::Health, process_cr3, sizeof(Health));

		// Read alive status
		bool bAlive = false;
		hypercall::read_guest_virtual_memory(&bAlive, CurrentActor + Offsets::bAlive, process_cr3, sizeof(bAlive));

		// Print actor information
		std::println("[ACTOR {}] Address: 0x{:X}, Position: ({:.2f}, {:.2f}, {:.2f}), Health: {:.2f}, Alive: {}", 
			i, CurrentActor, WorldPos.x, WorldPos.y, WorldPos.z, Health, bAlive ? "Yes" : "No");
	}

	std::println("[SUCCESS] Finished processing {} actors", Actors.Size());
}




void commands::process(const std::string command)
{
	if (command.empty() == true)
	{
}
	

	CLI::App app;
	app.require_subcommand();

	sys::kernel::parse_modules();

	const std::unordered_map<std::string, std::uint64_t> aliases = form_aliases();

	CLI::Transformer aliases_transformer = CLI::Transformer(aliases, CLI::ignore_case);

	aliases_transformer.description(" can_use_aliases");

	CLI::App* rgpm = init_rgpm(app, aliases_transformer);
	CLI::App* wgpm = init_wgpm(app, aliases_transformer);
	CLI::App* cgpm = init_cgpm(app, aliases_transformer);
	CLI::App* gvat = init_gvat(app, aliases_transformer);
	CLI::App* rgvm = init_rgvm(app, aliases_transformer);
	CLI::App* wgvm = init_wgvm(app, aliases_transformer);
	CLI::App* cgvm = init_cgvm(app, aliases_transformer);
	CLI::App* akh = init_akh(app, aliases_transformer);
	CLI::App* rkh = init_rkh(app, aliases_transformer);
	CLI::App* gva = init_gva(app, aliases_transformer);
	CLI::App* gpb = init_gpb(app, aliases_transformer);
	CLI::App* hgpp = init_hgpp(app, aliases_transformer);
	CLI::App* fl = init_fl(app);
	CLI::App* hfpc = init_hfpc(app);
	CLI::App* lkm = init_lkm(app);
	CLI::App* kme = init_kme(app);
	CLI::App* dkm = init_dkm(app);
	CLI::App* chkmap = init_chkmap(app, aliases_transformer);
	CLI::App* vuworld = init_vuworld(app);
	CLI::App* loaddll = init_loaddll(app);
	CLI::App* stealthdll = init_stealthdll(app);
	CLI::App* rs = init_rs(app);
	CLI::App* notepad_cr3 = init_notepad_cr3(app);
	CLI::App* hide_hv_memory = init_hide_hv_memory(app);
	CLI::App* test_ntoskrnl = init_test_ntoskrnl(app);
	CLI::App* test_exports = init_test_exports(app);
	CLI::App* pfn_query_demo = init_pfn_query_demo(app);
	CLI::App* cr3_cache = init_cr3_cache(app);
	CLI::App* cr3_monitor = init_cr3_monitor(app);
	CLI::App* get_world = init_get_world(app);

	try
	{
		app.parse(command);

		d_initial_process_command(rgpm);
		d_process_command(wgpm);
		d_process_command(cgpm);
		d_process_command(gvat);
		d_process_command(rgvm);
		d_process_command(wgvm);
		d_process_command(cgvm);
		d_process_command(akh);
		d_process_command(rkh);
		d_process_command(gva);
		d_process_command(gpb);
		d_process_command(hgpp);
		d_process_command(fl);
		d_process_command(hfpc);
		d_process_command(lkm);
		d_process_command(kme);
		d_process_command(dkm);
		d_process_command(chkmap);
		d_process_command(vuworld);
		d_process_command(stealthdll);
		d_process_command(rs);
		d_process_command(notepad_cr3);
		d_process_command(hide_hv_memory);
		d_process_command(test_ntoskrnl);
		d_process_command(test_exports);
		d_process_command(pfn_query_demo);
		d_process_command(cr3_cache);
		d_process_command(cr3_monitor);
		d_process_command(get_world);
	}
	catch (const CLI::ParseError& error)
	{
		app.exit(error);
	}
}

