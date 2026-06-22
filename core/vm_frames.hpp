#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"
#include "core/vm_operands.hpp"

namespace glupsk {

u32 frame_len(const Machine& machine);
u32 value_stack_base(const Machine& machine);

// A Glulx call stub remembers where a return value goes and which frame/pc to
// resume. The interpreter's initial stub uses pc 0 as the halt sentinel.
void push_call_stub(Machine& machine, StoreRef dest, u32 pc);
void pop_call_stub(Machine& machine, u32 value);

void enter_function(Machine& machine, u32 address, span<const u32> args);
void finish_return(Machine& machine, u32 value);
void branch_to(Machine& machine, u32 next_pc, u32 offset);

}  // namespace glupsk
