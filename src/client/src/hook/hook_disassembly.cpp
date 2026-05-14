#include "hook_disassembly.h"

#include <Zydis/Zydis.h>
#include <array>
#include <optional>
#include <print>
#include <vector>

ZydisDecoder make_zydis_decoder()
{
	ZydisDecoder decoder = { };

	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

	return decoder;
}

std::uint8_t decode_instruction(const ZydisDecoder* decoder, ZydisDecoderContext* decoder_context, ZydisDecodedInstruction* decoded_instruction, const std::uint8_t* instruction)
{
	ZyanStatus status = ZydisDecoderDecodeInstruction(decoder, decoder_context, instruction, ZYDIS_MAX_INSTRUCTION_LENGTH, decoded_instruction);

	return ZYAN_SUCCESS(status) == 1;
}

std::uint8_t decode_operands(const ZydisDecoder* decoder, const ZydisDecoderContext* decoder_context, const ZydisDecodedInstruction* decoded_instruction, std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT>& operands)
{
	ZyanStatus status = ZydisDecoderDecodeOperands(decoder, decoder_context, decoded_instruction, operands.data(), static_cast<ZyanU8>(operands.size()));

	return ZYAN_SUCCESS(status) == 1;
}

std::uint8_t get_instruction_length(const ZydisDecodedInstruction& decoded_instruction)
{
	return decoded_instruction.length;
}

std::vector<std::uint8_t> do_push(std::uint64_t value)
{
	std::vector<std::uint8_t> bytes = {
		0x68, 0x21, 0x43, 0x65, 0x87, // push   0xffffffff87654321
		0xC7, 0x44, 0x24, 0x04, 0x78, 0x56, 0x34, 0x12, // mov    DWORD PTR [rsp+0x4],0x12345678
	};

	parted_address_t push_value_parted = { .value = value };

	*reinterpret_cast<std::uint32_t*>(&bytes[1]) = push_value_parted.u.low_part;
	*reinterpret_cast<std::uint32_t*>(&bytes[9]) = push_value_parted.u.high_part;

	return bytes;
}

std::vector<std::uint8_t> do_rip_relative_push(std::uint32_t offset)
{
	std::vector<std::uint8_t> bytes = {
		0xE8, 0x00, 0x00, 0x00, 0x00, // call $+5
		0x48, 0x81, 0x04, 0x24, 0x78, 0x56, 0x34, 0x12 // add QWORD PTR [rsp],0x12345678
	};

	*reinterpret_cast<std::uint32_t*>(&bytes[9]) = offset - 5;

	return bytes;
}

std::vector<std::uint8_t> add_rsp(std::int32_t offset)
{
	std::vector<std::uint8_t> bytes = {
		0x48, 0x81, 0xC4, 0x78, 0x56, 0x34, 0x12 // add    rsp,0x12345678
	};

	*reinterpret_cast<std::int32_t*>(&bytes[3]) = offset;

	return bytes;
}

std::vector<std::uint8_t> do_jmp(std::uint64_t address_to_jmp)
{
	std::vector<std::uint8_t> bytes = do_push(address_to_jmp);

	bytes.insert(bytes.end(), 0xC3); // ret

	return bytes;
}

std::vector<std::uint8_t> do_relative_jmp(std::int32_t offset)
{
	std::vector<std::uint8_t> bytes = { 0xE9, 0x00, 0x00, 0x00, 0x00 };

	*reinterpret_cast<std::int32_t*>(&bytes[1]) = offset;

	return bytes;
}

std::vector<std::uint8_t> do_call(std::uint64_t address_to_call)
{
	std::vector<std::uint8_t> bytes = do_jmp(address_to_call);

	// insert return address
	std::vector<std::uint8_t> return_address_push = do_rip_relative_push(0); // placeholder to see the size of push

	return_address_push = do_rip_relative_push(static_cast<std::uint32_t>(bytes.size() + return_address_push.size()));

	bytes.insert(bytes.begin(), return_address_push.begin(), return_address_push.end());

	return bytes;
}

// all of below are rel32 when applicable
constexpr std::array<ZydisMnemonic, 22> jcc_instructions = {
	ZYDIS_MNEMONIC_JB,
	ZYDIS_MNEMONIC_JBE,
	ZYDIS_MNEMONIC_JCXZ,
	ZYDIS_MNEMONIC_JECXZ,
	ZYDIS_MNEMONIC_JKNZD,
	ZYDIS_MNEMONIC_JKZD,
	ZYDIS_MNEMONIC_JL,
	ZYDIS_MNEMONIC_JLE,
	ZYDIS_MNEMONIC_JNB,
	ZYDIS_MNEMONIC_JNBE,
	ZYDIS_MNEMONIC_JNL,
	ZYDIS_MNEMONIC_JNLE,
	ZYDIS_MNEMONIC_JNO,
	ZYDIS_MNEMONIC_JNP,
	ZYDIS_MNEMONIC_JNS,
	ZYDIS_MNEMONIC_JNZ,
	ZYDIS_MNEMONIC_JO,
	ZYDIS_MNEMONIC_JP,
	ZYDIS_MNEMONIC_JRCXZ,
	ZYDIS_MNEMONIC_JS,
	ZYDIS_MNEMONIC_JZ
};

std::uint8_t is_jcc_instruction(const ZydisDecodedInstruction& decoded_instruction)
{
	return std::ranges::find(jcc_instructions, decoded_instruction.mnemonic) != jcc_instructions.end();
}

std::vector<std::uint8_t> encode_instruction(const ZydisEncoderRequest* encoder_request)
{
	std::array<std::uint8_t, ZYDIS_MAX_INSTRUCTION_LENGTH> encoded_instruction = { };

	std::uint64_t encoded_instruction_size = sizeof(encoded_instruction);

	ZyanStatus status = ZydisEncoderEncodeInstruction(encoder_request, encoded_instruction.data(), &encoded_instruction_size);

	if (ZYAN_SUCCESS(status) == 0)
	{
		return { };
	}

	return { encoded_instruction.data(), encoded_instruction.data() + encoded_instruction_size };
}

constexpr std::array<ZydisRegister, 16> general_purpose_registers = {
	ZYDIS_REGISTER_RAX,
	ZYDIS_REGISTER_RCX,
	ZYDIS_REGISTER_RDX,
	ZYDIS_REGISTER_RBX,
	ZYDIS_REGISTER_RSP,
	ZYDIS_REGISTER_RBP,
	ZYDIS_REGISTER_RSI,
	ZYDIS_REGISTER_RDI,
	ZYDIS_REGISTER_R8,
	ZYDIS_REGISTER_R9,
	ZYDIS_REGISTER_R10,
	ZYDIS_REGISTER_R11,
	ZYDIS_REGISTER_R12,
	ZYDIS_REGISTER_R13,
	ZYDIS_REGISTER_R14,
	ZYDIS_REGISTER_R15
};

void remove_register_from_list(std::vector<ZydisRegister>& unused_registers, const ZydisRegister register_used)
{
	auto found = std::ranges::find(unused_registers, register_used);

	if (found != unused_registers.end())
	{
		unused_registers.erase(found);
	}
}

std::vector<ZydisRegister> find_unused_general_purpose_registers(const ZydisDecodedInstruction& decoded_instruction, const std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT>& operands)
{
	std::vector<ZydisRegister> unused_registers = { general_purpose_registers.begin(), general_purpose_registers.end() };

	for (std::uint64_t i = 0; i < decoded_instruction.operand_count; i++)
	{
		const ZydisDecodedOperand& current_operand = operands[i];

		if (current_operand.type == ZYDIS_OPERAND_TYPE_REGISTER)
		{
			remove_register_from_list(unused_registers, current_operand.reg.value);
		}
		else if (current_operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
		{
			remove_register_from_list(unused_registers, current_operand.mem.base);
			remove_register_from_list(unused_registers, current_operand.mem.index);
		}
	}

	return unused_registers;
}

std::vector<std::uint8_t> do_stack_register_operation(ZydisMnemonic mnemonic, ZydisRegister register_involved)
{
	ZydisEncoderRequest encoder_request = { };

	encoder_request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
	encoder_request.mnemonic = mnemonic;

	encoder_request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
	encoder_request.operands[0].reg.value = register_involved;

	encoder_request.operand_count = 1;

	return encode_instruction(&encoder_request);
}

std::vector<std::uint8_t> do_mov_to_register_64(ZydisRegister register_involved, std::uint64_t value)
{
	ZydisEncoderRequest encoder_request = { };

	encoder_request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
	encoder_request.mnemonic = ZYDIS_MNEMONIC_MOV;

	encoder_request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
	encoder_request.operands[0].reg.value = register_involved;

	encoder_request.operands[1].type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
	encoder_request.operands[1].imm.u = value;

	encoder_request.operand_count = 2;

	return encode_instruction(&encoder_request);
}

std::vector<std::uint8_t> do_register_push(ZydisRegister pushable_register)
{
	return do_stack_register_operation(ZYDIS_MNEMONIC_PUSH, pushable_register);
}

std::vector<std::uint8_t> do_register_pop(ZydisRegister pushable_register)
{
	return do_stack_register_operation(ZYDIS_MNEMONIC_POP, pushable_register);
}

// this should only be used on instructions that do not modify the rip, as we use more efficient ways to fix those
std::uint8_t fix_rip_relative_operand(std::vector<std::uint8_t>& instruction_bytes, const ZydisDecodedInstruction& decoded_instruction, const std::pair<std::uint64_t, std::uint64_t>& rip_relative_operand, const std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT>& operands)
{
	std::vector<ZydisRegister> unused_registers = find_unused_general_purpose_registers(decoded_instruction, operands);

	if (unused_registers.empty() == 1)
	{
		return 0;
	}

	const ZydisRegister usable_register = unused_registers[0];

	std::vector<std::uint8_t> pre_bytes = do_register_push(usable_register);
	std::vector<std::uint8_t> post_bytes = do_register_pop(usable_register);

	ZydisEncoderRequest encoder_request = { };

	ZyanStatus status = ZydisEncoderDecodedInstructionToEncoderRequest(&decoded_instruction, operands.data(), decoded_instruction.operand_count_visible, &encoder_request);

	if (ZYAN_SUCCESS(status) == 0)
	{
		return 0;
	}

	const std::uint64_t absolute_address = rip_relative_operand.second;

	if (absolute_address != 0)
	{
		auto& operand = encoder_request.operands[rip_relative_operand.first];
		auto& memory_operand = operand.mem;

		const std::vector<std::uint8_t> mov_bytes = do_mov_to_register_64(usable_register, absolute_address);

		pre_bytes.insert(pre_bytes.end(), mov_bytes.begin(), mov_bytes.end());

		operand.type = ZYDIS_OPERAND_TYPE_MEMORY;

		memory_operand.base = usable_register;
		memory_operand.displacement = 0;
	}

	instruction_bytes = encode_instruction(&encoder_request);

	instruction_bytes.insert(instruction_bytes.begin(), pre_bytes.begin(), pre_bytes.end());
	instruction_bytes.insert(instruction_bytes.end(), post_bytes.begin(), post_bytes.end());

	return 1;
}

std::uint8_t encode_rel8_instruction_as_rel32(std::vector<std::uint8_t>& instruction_bytes, const ZydisDecodedInstruction& decoded_instruction, const std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT>& operands)
{
	ZydisEncoderRequest encoder_request = { };

	ZyanStatus status = ZydisEncoderDecodedInstructionToEncoderRequest(&decoded_instruction, operands.data(), decoded_instruction.operand_count_visible, &encoder_request);

	if (ZYAN_SUCCESS(status) == 0)
	{
		return 0;
	}

	encoder_request.branch_type = ZYDIS_BRANCH_TYPE_NEAR;
	encoder_request.branch_width = ZYDIS_BRANCH_WIDTH_32;

	instruction_bytes = encode_instruction(&encoder_request);

	return 1;
}

std::vector<std::uint8_t> load_instruction_bytes(const ZydisDecoder& decoder, const ZydisDecoderContext& decoder_context, const ZydisDecodedInstruction& decoded_instruction, std::uint8_t* instruction, std::uint64_t instruction_runtime_address, std::uint64_t original_instruction_length, std::uint64_t instruction_aligned_bytes_offset, std::uint64_t routine_runtime_start, std::vector<std::pair<std::uint64_t, std::uint64_t>>& jcc_jumps, std::vector<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>>& rip_altering_instructions)
{
	std::vector<std::uint8_t> instruction_bytes = { instruction, instruction + original_instruction_length };

	const std::uint8_t is_relative = (decoded_instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE) != 0;

	if (is_relative == 1)
	{
		std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> operands = { };
		std::uint8_t status = decode_operands(&decoder, &decoder_context, &decoded_instruction, operands);

		if (status == 1)
		{
			// pair<operand_index, absolute_address>
			std::optional<std::pair<std::uint64_t, std::uint64_t>> rip_relative_operand = std::nullopt;

			for (std::uint64_t i = 0; i < decoded_instruction.operand_count; i++)
			{
				const ZydisDecodedOperand& current_operand = operands[i];

				if ((current_operand.type == ZYDIS_OPERAND_TYPE_MEMORY && current_operand.mem.base == ZYDIS_REGISTER_RIP) || (current_operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && current_operand.imm.is_relative == 1))
				{
					std::uint64_t absolute_address = 0;

					status = ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&decoded_instruction, &current_operand, instruction_runtime_address, &absolute_address));

					if (status == 1)
					{
						rip_relative_operand = { i, absolute_address };

						break;
					}
				}
			}

			if (rip_relative_operand.has_value() == 1)
			{
				const auto& rip_relative_operand_info = rip_relative_operand.value();

				const std::uint64_t absolute_location = rip_relative_operand_info.second;

				if (is_jcc_instruction(decoded_instruction) == 1)
				{
					if (original_instruction_length == 2)
					{
						std::uint8_t encode_status = encode_rel8_instruction_as_rel32(instruction_bytes, decoded_instruction, operands);

						if (encode_status == 0)
						{
							std::println("failed to re-encode jcc instruction");
						}
					}

					jcc_jumps.emplace_back(instruction_aligned_bytes_offset, absolute_location);
					rip_altering_instructions.emplace_back(instruction_aligned_bytes_offset, absolute_location, 0);
				}
				else if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_JMP || decoded_instruction.mnemonic == ZYDIS_MNEMONIC_CALL)
				{
					// todo: find real relative jmp after now obviously we expand stuff
					if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_JMP && (instruction[0] == 0xE9 || instruction[0] == 0xEB))
					{
						instruction_bytes = do_jmp(absolute_location);
					}
					else if (decoded_instruction.mnemonic == ZYDIS_MNEMONIC_CALL && instruction[0] == 0xE8)
					{
						instruction_bytes = do_call(absolute_location);
					}

					rip_altering_instructions.emplace_back(instruction_aligned_bytes_offset, absolute_location, 0);
				}
				else
				{
					fix_rip_relative_operand(instruction_bytes, decoded_instruction, rip_relative_operand_info, operands);
				}

				for (auto& [inst_aligned_bytes_offset, inst_calculated_absolute_address, inst_offset] : rip_altering_instructions)
				{
					if (instruction_aligned_bytes_offset == inst_aligned_bytes_offset || inst_calculated_absolute_address < (routine_runtime_start + inst_aligned_bytes_offset))
					{
						continue;
					}

					inst_offset += instruction_bytes.size() - original_instruction_length;
				}
			}
		}
	}

	return instruction_bytes;
}

void add_offset_to_instruction(const ZydisDecoder* decoder, std::uint8_t* current_instruction, std::int32_t offset)
{
	ZydisDecoderContext decoder_context = { };

	ZydisDecodedInstruction decoded_instruction = { };

	std::uint8_t status = decode_instruction(decoder, &decoder_context, &decoded_instruction, current_instruction);

	if (status == 0)
	{
		return;
	}

	std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> operands = { };

	status = decode_operands(decoder, &decoder_context, &decoded_instruction, operands);

	if (status == 0)
	{
		return;
	}

	ZydisEncoderRequest encoder_request = { };

	status = ZYAN_SUCCESS(ZydisEncoderDecodedInstructionToEncoderRequest(&decoded_instruction, operands.data(), decoded_instruction.operand_count_visible, &encoder_request));

	if (status == 0)
	{
		return;
	}

	auto& operand = encoder_request.operands[0];

	if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
	{
		operand.mem.displacement += offset;
	}
	else if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
	{
		operand.imm.s += offset;
	}

	std::vector<std::uint8_t> instruction_bytes = encode_instruction(&encoder_request);

	memcpy(current_instruction, instruction_bytes.data(), instruction_bytes.size());
}

void fix_rip_altering_instructions(const ZydisDecoder* decoder, std::vector<std::uint8_t>& aligned_bytes, const std::vector<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>>& rip_altering_instructions, const std::uint64_t routine_runtime_address)
{
	for (auto& [inst_aligned_bytes_offset, inst_calculated_absolute_address, inst_offset] : rip_altering_instructions)
	{
		if (routine_runtime_address + aligned_bytes.size() < inst_calculated_absolute_address)
		{
			continue;
		}

		add_offset_to_instruction(decoder, &aligned_bytes[inst_aligned_bytes_offset], static_cast<std::int32_t>(inst_offset));
	}
}

void make_jcc_patches(std::vector<std::uint8_t>& aligned_bytes, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& jcc_jumps, const std::uint64_t routine_runtime_address)
{
	if (jcc_jumps.empty() == 1)
	{
		return;
	}

	std::uint64_t bytes_added = 0;

	for (auto& [instruction_aligned_bytes_offset, jmp_address] : jcc_jumps)
	{
		std::vector<std::uint8_t> bytes = do_jmp(jmp_address);

		aligned_bytes.insert(aligned_bytes.begin(), bytes.begin(), bytes.end());

		bytes_added += bytes.size();

		constexpr std::int64_t jcc_rel32_instruction_length = 6;

		std::int32_t rip_change = static_cast<std::int32_t>(-jcc_rel32_instruction_length - bytes_added - instruction_aligned_bytes_offset);

		*reinterpret_cast<std::int32_t*>(&aligned_bytes[instruction_aligned_bytes_offset + bytes_added + 2]) = rip_change;
	}

	// calculate instruction size
	std::vector<std::uint8_t> stub_pass_bytes = do_relative_jmp(0);

	std::int32_t relative_jmp_offset = static_cast<std::int32_t>(bytes_added);

	stub_pass_bytes = do_relative_jmp(relative_jmp_offset);

	aligned_bytes.insert(aligned_bytes.begin(), stub_pass_bytes.begin(), stub_pass_bytes.end());
}

std::pair<std::vector<std::uint8_t>, std::uint64_t> hook_disasm::get_routine_aligned_bytes(std::uint8_t* routine, const std::uint64_t minimum_size, const std::uint64_t routine_runtime_address)
{
	std::vector<std::pair<std::uint64_t, std::uint64_t>> jcc_jumps = { };
	std::vector<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>> rip_altering_instructions = { };

	ZydisDecoder decoder = make_zydis_decoder();

	std::vector<std::uint8_t> aligned_bytes = { };

	std::uint8_t* current_instruction = routine;
	std::uint64_t original_routine_bytes_copied = 0;

	while (original_routine_bytes_copied < minimum_size)
	{
		ZydisDecoderContext decoder_context = { };

		ZydisDecodedInstruction decoded_instruction = { };

		std::uint8_t decode_status = decode_instruction(&decoder, &decoder_context, &decoded_instruction, current_instruction);

		if (decode_status == 0)
		{
			break;
		}

		std::uint8_t original_instruction_length = get_instruction_length(decoded_instruction);

		std::uint64_t instruction_runtime_address = routine_runtime_address + original_routine_bytes_copied;

		std::vector<std::uint8_t> instruction_bytes = load_instruction_bytes(decoder, decoder_context, decoded_instruction, current_instruction, instruction_runtime_address, original_instruction_length, aligned_bytes.size(), routine_runtime_address, jcc_jumps, rip_altering_instructions);

		aligned_bytes.insert(aligned_bytes.end(), instruction_bytes.begin(), instruction_bytes.end());

		current_instruction += original_instruction_length;
		original_routine_bytes_copied += original_instruction_length;
	}

	fix_rip_altering_instructions(&decoder, aligned_bytes, rip_altering_instructions, routine_runtime_address);
	make_jcc_patches(aligned_bytes, jcc_jumps, routine_runtime_address);

	return { aligned_bytes, original_routine_bytes_copied };
}
