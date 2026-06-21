#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

struct OpcodeHeader {
    u32 address = 0;
    u32 opcode = 0;
    u32 next_pc = 0;
    u8 encoded_size = 0;
};

struct Operand {
    u8 mode = 0;
    u32 data = 0;
};

struct Instruction {
    u32 address = 0;
    u32 opcode = 0;
    Operand operands[8] = {};
    u8 operand_count = 0;
    u32 next_pc = 0;
};

OpcodeHeader decode_opcode_header(const Memory& memory, u32 pc);
OpcodeHeader fetch_opcode_header(const Machine& machine);
Instruction decode_instruction(const Memory& memory, u32 pc);
Instruction fetch_instruction(const Machine& machine);

}  // namespace glupsk
