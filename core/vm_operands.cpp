#include "core/vm_operands.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

u32 locals_pos(const Machine& machine) {
    return machine.stack.read32(machine.regs.frame_ptr + 4);
}

u32 locals_base(const Machine& machine) {
    return machine.regs.frame_ptr + locals_pos(machine);
}

// Local operands are byte offsets into the current frame's locals area, not
// indexes into a separate register file.
u32 read_local(const Machine& machine, u32 address, u8 width) {
    const auto stack_address = locals_base(machine) + address;
    switch (width) {
        case 1:
            return machine.stack.read8(stack_address);
        case 2:
            return machine.stack.read16(stack_address);
        case 4:
            return machine.stack.read32(stack_address);
        default:
            throw std::runtime_error("invalid local read width");
    }
}

void write_local(Machine& machine, u32 address, u32 value, u8 width) {
    const auto stack_address = locals_base(machine) + address;
    switch (width) {
        case 1:
            machine.stack.write8(stack_address, static_cast<u8>(value));
            return;
        case 2:
            machine.stack.write16(stack_address, static_cast<u16>(value));
            return;
        case 4:
            machine.stack.write32(stack_address, value);
            return;
        default:
            throw std::runtime_error("invalid local write width");
    }
}

}  // namespace

u32 sign_extend(u32 value, int bits) {
    const auto mask = u32{1} << (bits - 1);
    return (value ^ mask) - mask;
}

u32 ram_address(const Machine& machine, u32 offset) {
    return machine.memory.ramstart + offset;
}

u32 load_operand(Machine& machine, Operand operand, u8 width) {
    switch (operand.mode) {
        using enum OperandMode;
        case zero:
            return 0;
        case const8:
            return sign_extend(operand.data, 8);
        case const16:
            return sign_extend(operand.data, 16);
        case const32:
            return operand.data;
        case mem8:
        case mem16:
        case mem32:
            switch (width) {
                case 1:
                    return machine.memory.read8(operand.data);
                case 2:
                    return machine.memory.read16(operand.data);
                case 4:
                    return machine.memory.read32(operand.data);
            }
            break;
        case stack:
            return machine.stack.pop32();
        case local8:
        case local16:
        case local32:
            return read_local(machine, operand.data, width);
        case ram8:
        case ram16:
        case ram32:
            switch (width) {
                case 1:
                    return machine.memory.read8(ram_address(machine, operand.data));
                case 2:
                    return machine.memory.read16(ram_address(machine, operand.data));
                case 4:
                    return machine.memory.read32(ram_address(machine, operand.data));
            }
            break;
        default:
            break;
    }
    throw std::runtime_error("invalid operand load");
}

StoreRef store_ref(const Machine& machine, Operand operand) {
    switch (operand.mode) {
        using enum OperandMode;
        case zero:
            return {.type = StoreDest::discard, .address = 0};
        case mem8:
        case mem16:
        case mem32:
            return {.type = StoreDest::memory, .address = operand.data};
        case stack:
            return {.type = StoreDest::stack, .address = 0};
        case local8:
        case local16:
        case local32:
            return {.type = StoreDest::local, .address = operand.data};
        case ram8:
        case ram16:
        case ram32:
            return {.type = StoreDest::memory,
                    .address = ram_address(machine, operand.data)};
        default:
            throw std::runtime_error("invalid store operand");
    }
}

void store_value(Machine& machine, StoreRef ref, u32 value, u8 width) {
    switch (ref.type) {
        case StoreDest::discard:
            return;
        case StoreDest::memory:
            switch (width) {
                case 1:
                    machine.memory.write8(ref.address, static_cast<u8>(value));
                    return;
                case 2:
                    machine.memory.write16(ref.address, static_cast<u16>(value));
                    return;
                case 4:
                    machine.memory.write32(ref.address, value);
                    return;
            }
            break;
        case StoreDest::local:
            write_local(machine, ref.address, value, width);
            return;
        case StoreDest::stack:
            machine.stack.push32(value);
            return;
        default:
            break;
    }
    throw std::runtime_error("invalid store");
}

void store_value(Machine& machine, Operand operand, u32 value, u8 width) {
    store_value(machine, store_ref(machine, operand), value, width);
}

Arguments pop_arguments(Machine& machine, u32 argc) {
    if (argc > 32) {
        throw std::runtime_error("too many temporary arguments");
    }

    Arguments args;
    args.count = argc;
    for (u32 index = 0; index < argc; ++index) {
        args.values[index] = machine.stack.pop32();
    }
    return args;
}

void restore_arguments(Machine& machine, const Arguments& args) {
    for (auto index = args.count; index > 0; --index) {
        machine.stack.push32(args.values[index - 1]);
    }
}

// Store operands are opcode-specific: the same syntactic operand mode can be a
// value in one opcode and a destination in another.
bool is_store_operand(Opcode opcode, u8 index) {
    switch (opcode) {
        using enum Opcode;
        case add:
        case sub:
        case mul:
        case div:
        case mod:
        case bitand_:
        case bitor_:
        case bitxor_:
        case shiftl:
        case sshiftr:
        case ushiftr:
        case aload:
        case aloads:
        case aloadb:
        case aloadbit:
        case gestalt:
        case glk:
            return index == 2;

        case neg:
        case bitnot_:
        case copy:
        case copys:
        case copyb:
        case sexs:
        case sexb:
        case stkpeek:
        case getmemsize:
        case setmemsize:
        case random:
        case save:
        case restore:
        case malloc:
            return index == 1;

        case catch_:
        case stkcount:
        case verify:
        case saveundo:
        case restoreundo:
        case hasundo:
        case getstringtbl:
            return index == 0;

        case getiosys:
            return index == 0 || index == 1;

        case call:
            return index == 2;
        case callf:
            return index == 1;
        case callfi:
            return index == 2;
        case callfii:
            return index == 3;
        case callfiii:
            return index == 4;

        case linearsearch:
        case binarysearch:
            return index == 7;
        case linkedsearch:
            return index == 6;

        default:
            return false;
    }
}

u8 load_width_for(Opcode opcode, u8 index) {
    if (index == 0 && opcode == Opcode::copys) {
        return 2;
    }
    if (index == 0 && opcode == Opcode::copyb) {
        return 1;
    }
    return 4;
}

}  // namespace glupsk
