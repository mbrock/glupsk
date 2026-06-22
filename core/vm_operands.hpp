#pragma once

#include "core/decode.hpp"
#include "core/machine.hpp"
#include "core/types.hpp"

#include <array>

namespace glupsk {

enum class StoreDest : u8 {
    discard = 0,
    memory = 1,
    local = 2,
    stack = 3,
};

struct StoreRef {
    StoreDest type = StoreDest::discard;
    u32 address = 0;
};

// Call operands are popped before the callee starts. Keeping them in a fixed
// local buffer makes blocked @glk calls resumable without heap traffic.
struct Arguments {
    std::array<u32, 32> values = {};
    u32 count = 0;

    span<const u32> as_span() const {
        return {values.data(), static_cast<std::size_t>(count)};
    }
};

u32 sign_extend(u32 value, int bits);
u32 ram_address(const Machine& machine, u32 offset);

// Loading a stack operand consumes it; callers should classify store operands
// first so result destinations are not popped by mistake.
u32 load_operand(Machine& machine, Operand operand, u8 width = 4);
StoreRef store_ref(const Machine& machine, Operand operand);
void store_value(Machine& machine, StoreRef ref, u32 value, u8 width = 4);
void store_value(Machine& machine, Operand operand, u32 value, u8 width = 4);

Arguments pop_arguments(Machine& machine, u32 argc);
void restore_arguments(Machine& machine, const Arguments& args);

bool is_store_operand(Opcode opcode, u8 index);
u8 load_width_for(Opcode opcode, u8 index);

}  // namespace glupsk
