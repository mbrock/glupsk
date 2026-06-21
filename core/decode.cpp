#include "core/decode.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

u8 operand_count_for(u32 opcode) {
    switch (opcode) {
        case 0x00:
        case 0x52:
        case 0x120:
        case 0x122:
        case 0x129:
            return 0;

        case 0x20:
        case 0x31:
        case 0x54:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x101:
        case 0x111:
        case 0x121:
        case 0x125:
        case 0x126:
        case 0x128:
        case 0x140:
        case 0x141:
        case 0x179:
            return 1;

        case 0x15:
        case 0x1b:
        case 0x22:
        case 0x23:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x44:
        case 0x45:
        case 0x50:
        case 0x51:
        case 0x53:
        case 0x102:
        case 0x103:
        case 0x104:
        case 0x110:
        case 0x123:
        case 0x124:
        case 0x127:
        case 0x148:
        case 0x149:
        case 0x160:
        case 0x178:
        case 0x180:
        case 0x181:
            return 2;

        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1c:
        case 0x1d:
        case 0x1e:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2c:
        case 0x2d:
        case 0x30:
        case 0x48:
        case 0x49:
        case 0x4a:
        case 0x4b:
        case 0x4c:
        case 0x4d:
        case 0x4e:
        case 0x4f:
        case 0x100:
        case 0x130:
        case 0x161:
        case 0x170:
        case 0x171:
            return 3;

        case 0x162:
            return 4;
        case 0x163:
            return 5;
        case 0x152:
            return 7;
        case 0x150:
        case 0x151:
            return 8;
        default:
            throw std::runtime_error("unknown opcode");
    }
}

u32 read_operand_data(const Memory& memory, u32& pc, u8 mode) {
    switch (mode) {
        case 0x0:
        case 0x8:
            return 0;
        case 0x1:
        case 0x5:
        case 0x9:
        case 0xd: {
            const auto value = memory.read8(pc);
            pc += 1;
            return value;
        }
        case 0x2:
        case 0x6:
        case 0xa:
        case 0xe: {
            const auto value = memory.read16(pc);
            pc += 2;
            return value;
        }
        case 0x3:
        case 0x7:
        case 0xb:
        case 0xf: {
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
    Instruction instruction;
    instruction.address = pc;
    instruction.opcode = header.opcode;
    instruction.operand_count = operand_count_for(header.opcode);

    auto cursor = header.next_pc;
    u8 modes[8] = {};
    for (u8 index = 0; index < instruction.operand_count; index += 2) {
        const auto byte = memory.read8(cursor++);
        modes[index] = byte & 0x0f;
        if (index + 1 < instruction.operand_count) {
            modes[index + 1] = (byte >> 4) & 0x0f;
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

}  // namespace glupsk
