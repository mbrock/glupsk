#include "core/decode.hpp"

#include "core/opcode_meta.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

u32 read_operand_data(const Memory& memory, u32& pc, OperandMode mode) {
    switch (mode) {
        using enum OperandMode;
        case zero:
        case stack:
            return 0;
        case const8:
        case mem8:
        case local8:
        case ram8: {
            const auto value = memory.read8(pc);
            pc += 1;
            return value;
        }
        case const16:
        case mem16:
        case local16:
        case ram16: {
            const auto value = memory.read16(pc);
            pc += 2;
            return value;
        }
        case const32:
        case mem32:
        case local32:
        case ram32: {
            const auto value = memory.read32(pc);
            pc += 4;
            return value;
        }
        default:
            throw std::runtime_error("unused operand addressing mode");
    }
}

}  // namespace

OpcodeHeader decode_opcode_header(const Memory& memory, u32 pc) {
    const auto first = memory.read8(pc);

    if ((first & 0x80) == 0) {
        return {
            .address = pc,
            .opcode = first,
            .next_pc = pc + 1,
            .encoded_size = 1,
        };
    }

    if ((first & 0xc0) == 0x80) {
        return {
            .address = pc,
            .opcode = static_cast<u32>(memory.read16(pc) & 0x3fff),
            .next_pc = pc + 2,
            .encoded_size = 2,
        };
    }

    return {
        .address = pc,
        .opcode = memory.read32(pc) & 0x0fffffff,
        .next_pc = pc + 4,
        .encoded_size = 4,
    };
}

OpcodeHeader fetch_opcode_header(const Machine& machine) {
    return decode_opcode_header(machine.memory, machine.regs.pc);
}

Instruction decode_instruction(const Memory& memory, u32 pc) {
    const auto header = decode_opcode_header(memory, pc);
    const auto opcode = static_cast<Opcode>(header.opcode);
    Instruction instruction;
    instruction.address = pc;
    instruction.opcode = opcode;
    instruction.operand_count = operand_count_for(opcode);

    auto cursor = header.next_pc;
    OperandMode modes[8] = {};
    for (u8 index = 0; index < instruction.operand_count; index += 2) {
        const auto byte = memory.read8(cursor++);
        modes[index] = static_cast<OperandMode>(byte & 0x0f);
        if (index + 1 < instruction.operand_count) {
            modes[index + 1] = static_cast<OperandMode>((byte >> 4) & 0x0f);
        }
    }

    for (u8 index = 0; index < instruction.operand_count; ++index) {
        instruction.operands[index].mode = modes[index];
        instruction.operands[index].data =
            read_operand_data(memory, cursor, modes[index]);
    }
    instruction.next_pc = cursor;
    return instruction;
}

Instruction fetch_instruction(const Machine& machine) {
    return decode_instruction(machine.memory, machine.regs.pc);
}

std::string_view operand_mode_name(OperandMode mode) {
    switch (mode) {
        using enum OperandMode;
        case zero: return "zero";
        case const8: return "const8";
        case const16: return "const16";
        case const32: return "const32";
        case mem8: return "mem8";
        case mem16: return "mem16";
        case mem32: return "mem32";
        case stack: return "stack";
        case local8: return "local8";
        case local16: return "local16";
        case local32: return "local32";
        case ram8: return "ram8";
        case ram16: return "ram16";
        case ram32: return "ram32";
        default: return "unused";
    }
}

}  // namespace glupsk
