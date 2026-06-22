#include "core/vm_frames.hpp"

namespace glupsk {

u32 frame_len(const Machine& machine) {
    return machine.stack.read32(machine.regs.frame_ptr);
}

u32 value_stack_base(const Machine& machine) {
    return machine.regs.frame_ptr + frame_len(machine);
}

void push_call_stub(Machine& machine, StoreRef dest, u32 pc) {
    machine.stack.push32(static_cast<u32>(dest.type));
    machine.stack.push32(dest.address);
    machine.stack.push32(pc);
    machine.stack.push32(machine.regs.frame_ptr);
}

void pop_call_stub(Machine& machine, u32 value) {
    if (machine.stack.sp < 16) {
        machine.halted = true;
        machine.running = false;
        return;
    }

    machine.stack.sp -= 16;
    const auto dest = StoreRef{.type = static_cast<StoreDest>(machine.stack.read32(machine.stack.sp)),
                               .address = machine.stack.read32(machine.stack.sp + 4)};
    const auto pc = machine.stack.read32(machine.stack.sp + 8);
    const auto frame_ptr = machine.stack.read32(machine.stack.sp + 12);
    machine.regs.pc = pc;
    machine.regs.frame_ptr = frame_ptr;

    if (pc == 0) {
        machine.halted = true;
        machine.running = false;
        return;
    }
    store_value(machine, dest, value);
}

void finish_return(Machine& machine, u32 value) {
    machine.stack.sp = machine.regs.frame_ptr;
    pop_call_stub(machine, value);
}

void branch_to(Machine& machine, u32 next_pc, u32 offset) {
    if (offset == 0 || offset == 1) {
        finish_return(machine, offset);
        return;
    }
    machine.regs.pc = next_pc + offset - 2;
}

}  // namespace glupsk
