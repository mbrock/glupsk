#pragma once

#include "core/decode.hpp"
#include "core/types.hpp"

#include <string_view>

namespace glupsk {

// Operand roles are opcode metadata, not operand-addressing metadata. A stack
// store operand must not be loaded, because loading would pop the value stack.
u8 operand_count_for(Opcode opcode);
std::string_view opcode_name(Opcode opcode);
bool is_store_operand(Opcode opcode, u8 index);
u8 load_width_for(Opcode opcode, u8 index);

}  // namespace glupsk
