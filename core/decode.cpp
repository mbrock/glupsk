#include "core/decode.hpp"

namespace glupsk {

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

}  // namespace glupsk
