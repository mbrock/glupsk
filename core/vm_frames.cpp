#include "core/vm_frames.hpp"

#include "core/vm_accel.hpp"

#include <format>
#include <stdexcept>

namespace glupsk {
namespace {

u32 align_to(u32 value, u32 alignment) {
    const auto mask = alignment - 1;
    return (value + mask) & ~mask;
}

}  // namespace

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

void enter_function(Machine& machine, u32 address, span<const u32> args) {
    if (const auto function = accelerated_function_for(machine, address);
        function != 0) {
        const auto result = call_accelerated_function(machine, function, args);
        pop_call_stub(machine, result);
        return;
    }

    const auto type = machine.memory.read8(address++);
    if (type != 0xc0 && type != 0xc1) {
        throw std::runtime_error(std::format("call to non-function at 0x{:x}",
                                            address - 1));
    }

    const auto frame_ptr = machine.stack.sp;
    const auto format_start = address;
    auto format_cursor = address;
    auto format_pair_count = u32{0};
    auto local_len = u32{0};
    while (true) {
        const auto local_type = machine.memory.read8(format_cursor++);
        const auto local_count = machine.memory.read8(format_cursor++);
        ++format_pair_count;
        if (local_type == 0) {
            if (format_pair_count & 1) {
                ++format_pair_count;
            }
            break;
        }
        if (local_type != 1 && local_type != 2 && local_type != 4) {
            throw std::runtime_error("illegal local type");
        }
        local_len = align_to(local_len, local_type);
        local_len += static_cast<u32>(local_type * local_count);
    }
    local_len = align_to(local_len, 4);

    const auto raw_format_bytes = format_cursor - format_start;
    const auto format_bytes = format_pair_count * 2;
    const auto locals_pos_value = 8 + format_bytes;
    const auto frame_len_value = locals_pos_value + local_len;

    if (static_cast<std::size_t>(frame_ptr) + frame_len_value >
        machine.stack.bytes.size()) {
        throw std::runtime_error("stack overflow in function call");
    }

    machine.stack.write32(frame_ptr, frame_len_value);
    machine.stack.write32(frame_ptr + 4, locals_pos_value);
    for (u32 index = 0; index < raw_format_bytes; ++index) {
        machine.stack.write8(frame_ptr + 8 + index,
                             machine.memory.read8(format_start + index));
    }
    for (u32 index = raw_format_bytes; index < format_bytes; ++index) {
        machine.stack.write8(frame_ptr + 8 + index, 0);
    }
    for (u32 offset = 0; offset < local_len; ++offset) {
        machine.stack.write8(frame_ptr + locals_pos_value + offset, 0);
    }

    machine.regs.frame_ptr = frame_ptr;
    machine.stack.sp = frame_ptr + frame_len_value;
    machine.regs.pc = format_cursor;

    if (type == 0xc0) {
        for (auto it = args.rbegin(); it != args.rend(); ++it) {
            machine.stack.push32(*it);
        }
        machine.stack.push32(static_cast<u32>(args.size()));
        return;
    }

    auto frame_format_cursor = frame_ptr + 8;
    auto local_cursor = frame_ptr + locals_pos_value;
    auto arg_index = std::size_t{0};
    while (arg_index < args.size()) {
        const auto local_type = machine.stack.read8(frame_format_cursor++);
        const auto local_count = machine.stack.read8(frame_format_cursor++);
        if (local_type == 0) {
            break;
        }
        local_cursor = align_to(local_cursor, local_type);
        for (u8 index = 0; index < local_count && arg_index < args.size();
             ++index, ++arg_index) {
            switch (local_type) {
                case 1:
                    machine.stack.write8(local_cursor, static_cast<u8>(args[arg_index]));
                    local_cursor += 1;
                    break;
                case 2:
                    machine.stack.write16(local_cursor, static_cast<u16>(args[arg_index]));
                    local_cursor += 2;
                    break;
                case 4:
                    machine.stack.write32(local_cursor, args[arg_index]);
                    local_cursor += 4;
                    break;
            }
        }
    }
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
