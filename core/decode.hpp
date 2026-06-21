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

OpcodeHeader decode_opcode_header(const Memory& memory, u32 pc);
OpcodeHeader fetch_opcode_header(const Machine& machine);

}  // namespace glupsk
