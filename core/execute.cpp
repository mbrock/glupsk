#include "core/execute.hpp"

#include "core/decode.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace glupsk {
namespace {

enum class StoreDest : u8 {
    discard = 0,
    memory = 1,
    local = 2,
    stack = 3,
};

enum class StringNodeType : u8 {
    branch = 0x00,
    terminator = 0x01,
    character = 0x02,
    c_string = 0x03,
    unicode_character = 0x04,
    unicode_string = 0x05,
    indirect = 0x08,
    double_indirect = 0x09,
    indirect_args = 0x0a,
    double_indirect_args = 0x0b,
};

enum class StringObjectType : u8 {
    c_string = 0xe0,
    compressed = 0xe1,
    unicode = 0xe2,
};

enum class GestaltSelector : u32 {
    glulx_version = 0,
    terp_version = 1,
    resize_mem = 2,
    undo = 3,
    io_system = 4,
    unicode = 5,
    mem_copy = 6,
    malloc_ = 7,
    malloc_heap = 8,
    acceleration = 9,
    accel_func = 10,
    float_ = 11,
    ext_undo = 12,
    double_ = 13,
};

struct StoreRef {
    StoreDest type = StoreDest::discard;
    u32 address = 0;
};

struct Arguments {
    std::array<u32, 32> values = {};
    u32 count = 0;

    span<const u32> as_span() const {
        return {values.data(), static_cast<std::size_t>(count)};
    }
};

u32 sign_extend(u32 value, int bits) {
    const auto mask = u32{1} << (bits - 1);
    return (value ^ mask) - mask;
}

std::int32_t as_signed(u32 value) {
    return static_cast<std::int32_t>(value);
}

u32 align_to(u32 value, u32 alignment) {
    const auto mask = alignment - 1;
    return (value + mask) & ~mask;
}

u32 frame_len(const Machine& machine) {
    return machine.stack.read32(machine.regs.frame_ptr);
}

u32 locals_pos(const Machine& machine) {
    return machine.stack.read32(machine.regs.frame_ptr + 4);
}

u32 locals_base(const Machine& machine) {
    return machine.regs.frame_ptr + locals_pos(machine);
}

u32 value_stack_base(const Machine& machine) {
    return machine.regs.frame_ptr + frame_len(machine);
}

u32 ram_address(const Machine& machine, u32 offset) {
    return machine.memory.ramstart + offset;
}

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

u32 load_operand(Machine& machine, Operand operand, u8 width = 4) {
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

void store_value(Machine& machine, StoreRef ref, u32 value, u8 width = 4) {
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

void store_value(Machine& machine, Operand operand, u32 value, u8 width = 4) {
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

void output_char(Machine& machine, u32 ch) {
    if (machine.regs.iosys_mode == 0) {
        return;
    }
    if (machine.regs.iosys_mode != 2) {
        throw std::runtime_error("unsupported I/O system");
    }
    if (machine.glk) {
        machine.glk->put_char(ch);
    }
}

void output_string_object(Machine& machine, u32 address);

void output_c_string(Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read8(address++);
        if (ch == 0) {
            return;
        }
        output_char(machine, ch);
    }
}

void output_uni_string(Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read32(address);
        address += 4;
        if (ch == 0) {
            return;
        }
        output_char(machine, ch);
    }
}

void output_huffman_leaf(Machine& machine, u32 node) {
    const auto type = static_cast<StringNodeType>(machine.memory.read8(node));
    switch (type) {
        case StringNodeType::character:
            output_char(machine, machine.memory.read8(node + 1));
            return;
        case StringNodeType::c_string:
            output_c_string(machine, node + 1);
            return;
        case StringNodeType::unicode_character:
            output_char(machine, machine.memory.read32(node + 1));
            return;
        case StringNodeType::unicode_string:
            output_uni_string(machine, node + 1);
            return;
        case StringNodeType::indirect: {
            const auto address = machine.memory.read32(node + 1);
            output_string_object(machine, address);
            return;
        }
        case StringNodeType::double_indirect: {
            const auto address = machine.memory.read32(machine.memory.read32(node + 1));
            output_string_object(machine, address);
            return;
        }
        case StringNodeType::indirect_args:
        case StringNodeType::double_indirect_args:
            throw std::runtime_error("string function references are not implemented");
        case StringNodeType::branch:
        case StringNodeType::terminator:
        default:
            throw std::runtime_error("unknown string leaf node");
    }
}

void output_compressed_string(Machine& machine, u32 address) {
    if (machine.regs.string_table == 0) {
        throw std::runtime_error("compressed string without string table");
    }
    const auto root = machine.memory.read32(machine.regs.string_table + 8);
    auto byte_address = address;
    auto bit = u8{0};

    while (true) {
        auto node = root;
        while (static_cast<StringNodeType>(machine.memory.read8(node)) ==
               StringNodeType::branch) {
            const auto byte = machine.memory.read8(byte_address);
            const auto take_right = (byte >> bit) & 1;
            ++bit;
            if (bit == 8) {
                bit = 0;
                ++byte_address;
            }
            node = machine.memory.read32(node + (take_right ? 5 : 1));
        }

        if (static_cast<StringNodeType>(machine.memory.read8(node)) ==
            StringNodeType::terminator) {
            return;
        }
        output_huffman_leaf(machine, node);
    }
}

void output_string_object(Machine& machine, u32 address) {
    const auto type = static_cast<StringObjectType>(machine.memory.read8(address));
    switch (type) {
        case StringObjectType::c_string:
            output_c_string(machine, address + 1);
            return;
        case StringObjectType::compressed:
            output_compressed_string(machine, address + 1);
            return;
        case StringObjectType::unicode:
            output_uni_string(machine, address + 4);
            return;
        default:
            throw std::runtime_error("streamstr target is not a string");
    }
}

u32 gestalt(u32 selector, u32 arg) {
    switch (static_cast<GestaltSelector>(selector)) {
        case GestaltSelector::glulx_version:
            return 0x00030103;
        case GestaltSelector::terp_version:
            return 0x00000001;
        case GestaltSelector::resize_mem:
        case GestaltSelector::undo:
        case GestaltSelector::malloc_:
        case GestaltSelector::acceleration:
        case GestaltSelector::float_:
        case GestaltSelector::ext_undo:
        case GestaltSelector::double_:
            return 0;
        case GestaltSelector::io_system:
            return (arg == 0 || arg == 1 || arg == 2) ? 1 : 0;
        case GestaltSelector::unicode:
        case GestaltSelector::mem_copy:
            return 1;
        case GestaltSelector::malloc_heap:
        case GestaltSelector::accel_func:
            return 0;
        default:
            return 0;
    }
}

u32 binary_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options) {
    const auto key_indirect = (options & 0x01) != 0;
    const auto return_index = (options & 0x04) != 0;

    auto read_key_byte = [&](u32 source, u32 index) -> u8 {
        if (key_indirect) {
            return machine.memory.read8(source + index);
        }
        if (key_size == 1) {
            return static_cast<u8>(source);
        }
        if (key_size == 2) {
            return static_cast<u8>((source >> ((1 - index) * 8)) & 0xff);
        }
        return static_cast<u8>((source >> ((3 - index) * 8)) & 0xff);
    };

    auto compare_at = [&](u32 index) {
        const auto entry = start + index * struct_size + key_offset;
        for (u32 ix = 0; ix < key_size; ++ix) {
            const auto want = read_key_byte(key, ix);
            const auto got = machine.memory.read8(entry + ix);
            if (want < got) {
                return -1;
            }
            if (want > got) {
                return 1;
            }
        }
        return 0;
    };

    auto low = u32{0};
    auto high = num_structs;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto cmp = compare_at(mid);
        if (cmp == 0) {
            return return_index ? mid : start + mid * struct_size;
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return return_index ? 0xffffffffu : 0;
}

void execute_instruction(Machine& machine, const Instruction& insn) {
    machine.regs.pc = insn.next_pc;
    const auto op = insn.opcode;
    const auto& a = insn.operands;

    auto l = [&](int index, u8 width = 4) {
        return load_operand(machine, a[index], width);
    };
    auto s = [&](int index, u32 value, u8 width = 4) {
        store_value(machine, a[index], value, width);
    };
    auto b = [&](bool condition, u32 offset) {
        if (condition) {
            branch_to(machine, insn.next_pc, offset);
        }
    };

    switch (op) {
        case Opcode::nop:
            return;
        case Opcode::add:
            s(2, l(0) + l(1));
            return;
        case Opcode::sub:
            s(2, l(0) - l(1));
            return;
        case Opcode::mul:
            s(2, l(0) * l(1));
            return;
        case Opcode::div:
            s(2, static_cast<u32>(as_signed(l(0)) / as_signed(l(1))));
            return;
        case Opcode::mod:
            s(2, static_cast<u32>(as_signed(l(0)) % as_signed(l(1))));
            return;
        case Opcode::neg:
            s(1, -l(0));
            return;
        case Opcode::bitand_:
            s(2, l(0) & l(1));
            return;
        case Opcode::bitor_:
            s(2, l(0) | l(1));
            return;
        case Opcode::bitxor_:
            s(2, l(0) ^ l(1));
            return;
        case Opcode::bitnot_:
            s(1, ~l(0));
            return;
        case Opcode::shiftl: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) << amount);
            return;
        }
        case Opcode::sshiftr: {
            const auto value = l(0);
            const auto amount = l(1);
            if (amount >= 32) {
                s(2, (value & 0x80000000u) ? 0xffffffffu : 0);
            } else {
                s(2, static_cast<u32>(as_signed(value) >> amount));
            }
            return;
        }
        case Opcode::ushiftr: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) >> amount);
            return;
        }
        case Opcode::jump:
            branch_to(machine, insn.next_pc, l(0));
            return;
        case Opcode::jz:
            b(l(0) == 0, l(1));
            return;
        case Opcode::jnz:
            b(l(0) != 0, l(1));
            return;
        case Opcode::jeq:
            b(l(0) == l(1), l(2));
            return;
        case Opcode::jne:
            b(l(0) != l(1), l(2));
            return;
        case Opcode::jlt:
            b(as_signed(l(0)) < as_signed(l(1)), l(2));
            return;
        case Opcode::jge:
            b(as_signed(l(0)) >= as_signed(l(1)), l(2));
            return;
        case Opcode::jgt:
            b(as_signed(l(0)) > as_signed(l(1)), l(2));
            return;
        case Opcode::jle:
            b(as_signed(l(0)) <= as_signed(l(1)), l(2));
            return;
        case Opcode::jltu:
            b(l(0) < l(1), l(2));
            return;
        case Opcode::jgeu:
            b(l(0) >= l(1), l(2));
            return;
        case Opcode::jgtu:
            b(l(0) > l(1), l(2));
            return;
        case Opcode::jleu:
            b(l(0) <= l(1), l(2));
            return;
        case Opcode::call: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            const auto args = pop_arguments(machine, argc);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args.as_span());
            return;
        }
        case Opcode::return_:
            finish_return(machine, l(0));
            return;
        case Opcode::catch_: {
            const auto offset = l(1);
            const auto dest = store_ref(machine, a[0]);
            push_call_stub(machine, dest, insn.next_pc);
            const auto token = machine.stack.sp;
            store_value(machine, dest, token);
            branch_to(machine, insn.next_pc, offset);
            return;
        }
        case Opcode::throw_: {
            const auto value = l(0);
            const auto token = l(1);
            machine.stack.sp = token;
            pop_call_stub(machine, value);
            return;
        }
        case Opcode::tailcall: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto args = pop_arguments(machine, argc);
            machine.stack.sp = machine.regs.frame_ptr;
            enter_function(machine, fn, args.as_span());
            return;
        }
        case Opcode::copy:
            s(1, l(0));
            return;
        case Opcode::copys:
            s(1, l(0, 2), 2);
            return;
        case Opcode::copyb:
            s(1, l(0, 1), 1);
            return;
        case Opcode::sexs:
            s(1, sign_extend(l(0) & 0xffff, 16));
            return;
        case Opcode::sexb:
            s(1, sign_extend(l(0) & 0xff, 8));
            return;
        case Opcode::aload:
            s(2, machine.memory.read32(l(0) + 4 * as_signed(l(1))));
            return;
        case Opcode::aloads:
            s(2, machine.memory.read16(l(0) + 2 * as_signed(l(1))));
            return;
        case Opcode::aloadb:
            s(2, machine.memory.read8(l(0) + as_signed(l(1))));
            return;
        case Opcode::aloadbit: {
            const auto bit_index = as_signed(l(1));
            auto address = l(0) + bit_index / 8;
            auto bit = bit_index % 8;
            if (bit < 0) {
                --address;
                bit += 8;
            }
            s(2, (machine.memory.read8(address) >> bit) & 1);
            return;
        }
        case Opcode::astore:
            machine.memory.write32(l(0) + 4 * as_signed(l(1)), l(2));
            return;
        case Opcode::astores:
            machine.memory.write16(l(0) + 2 * as_signed(l(1)),
                                   static_cast<u16>(l(2)));
            return;
        case Opcode::astoreb:
            machine.memory.write8(l(0) + as_signed(l(1)), static_cast<u8>(l(2)));
            return;
        case Opcode::astorebit: {
            const auto value = l(2);
            const auto bit_index = as_signed(l(1));
            auto address = l(0) + bit_index / 8;
            auto bit = bit_index % 8;
            if (bit < 0) {
                --address;
                bit += 8;
            }
            auto byte = machine.memory.read8(address);
            if (value) {
                byte |= static_cast<u8>(1u << bit);
            } else {
                byte &= static_cast<u8>(~(1u << bit));
            }
            machine.memory.write8(address, byte);
            return;
        }
        case Opcode::stkcount:
            s(0, (machine.stack.sp - value_stack_base(machine)) / 4);
            return;
        case Opcode::stkpeek: {
            const auto index = l(0);
            s(1, machine.stack.read32(machine.stack.sp - 4 - 4 * index));
            return;
        }
        case Opcode::stkswap: {
            const auto first = machine.stack.pop32();
            const auto second = machine.stack.pop32();
            machine.stack.push32(first);
            machine.stack.push32(second);
            return;
        }
        case Opcode::stkroll: {
            const auto count = l(0);
            const auto places_signed = as_signed(l(1));
            if (count == 0 || places_signed == 0) {
                return;
            }
            auto values = std::vector<u32>(count);
            for (u32 index = 0; index < count; ++index) {
                values[count - 1 - index] = machine.stack.pop32();
            }
            auto places = places_signed % static_cast<std::int32_t>(count);
            if (places < 0) {
                places += static_cast<std::int32_t>(count);
            }
            std::rotate(values.begin(), values.end() - places, values.end());
            for (const auto value : values) {
                machine.stack.push32(value);
            }
            return;
        }
        case Opcode::stkcopy: {
            const auto count = l(0);
            const auto start = machine.stack.sp - count * 4;
            for (u32 index = 0; index < count; ++index) {
                machine.stack.push32(machine.stack.read32(start + index * 4));
            }
            return;
        }
        case Opcode::streamchar:
            output_char(machine, l(0) & 0xff);
            return;
        case Opcode::streamnum:
            for (const auto ch : std::format("{}", as_signed(l(0)))) {
                output_char(machine, static_cast<u8>(ch));
            }
            return;
        case Opcode::streamstr:
            output_string_object(machine, l(0));
            return;
        case Opcode::streamunichar:
            output_char(machine, l(0));
            return;
        case Opcode::gestalt:
            s(2, gestalt(l(0), l(1)));
            return;
        case Opcode::debugtrap:
            throw std::runtime_error("debugtrap");
        case Opcode::getmemsize:
            s(0, static_cast<u32>(machine.memory.bytes.size()));
            return;
        case Opcode::setmemsize:
            s(1, 1);
            return;
        case Opcode::jumpabs:
            machine.regs.pc = l(0);
            return;
        case Opcode::random:
            s(1, 0);
            return;
        case Opcode::setrandom:
            return;
        case Opcode::quit:
            machine.halted = true;
            machine.running = false;
            return;
        case Opcode::verify:
            s(0, 0);
            return;
        case Opcode::saveundo:
        case Opcode::restoreundo:
        case Opcode::hasundo:
            s(0, 1);
            return;
        case Opcode::protect:
        case Opcode::discardundo:
            return;
        case Opcode::glk: {
            const auto selector = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            auto args = pop_arguments(machine, argc);
            auto result = u32{0};
            if (selector == 0x00a0 && args.count == 1) {
                result = static_cast<u8>(std::tolower(static_cast<unsigned char>(args.values[0])));
            } else if (selector == 0x00a1 && args.count == 1) {
                result = static_cast<u8>(std::toupper(static_cast<unsigned char>(args.values[0])));
            } else if (machine.glk) {
                result = machine.glk->call(selector, args.as_span());
            }
            store_value(machine, dest, result);
            return;
        }
        case Opcode::getstringtbl:
            s(0, machine.regs.string_table);
            return;
        case Opcode::setstringtbl:
            machine.regs.string_table = l(0);
            return;
        case Opcode::getiosys:
            store_value(machine, a[0], machine.regs.iosys_mode);
            store_value(machine, a[1], machine.regs.iosys_rock);
            return;
        case Opcode::setiosys:
            machine.regs.iosys_mode = l(0);
            machine.regs.iosys_rock = l(1);
            if (machine.regs.iosys_mode != 0 && machine.regs.iosys_mode != 2) {
                machine.regs.iosys_mode = 0;
            }
            return;
        case Opcode::binarysearch:
            s(7, binary_search(machine, l(0), l(1), l(2), l(3), l(4), l(5), l(6)));
            return;
        case Opcode::callf: {
            const auto dest = store_ref(machine, a[1]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), span<const u32>{});
            return;
        }
        case Opcode::callfi: {
            const auto args = std::array<u32, 1>{l(1)};
            const auto dest = store_ref(machine, a[2]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case Opcode::callfii: {
            const auto args = std::array<u32, 2>{l(1), l(2)};
            const auto dest = store_ref(machine, a[3]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case Opcode::callfiii: {
            const auto args = std::array<u32, 3>{l(1), l(2), l(3)};
            const auto dest = store_ref(machine, a[4]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case Opcode::mzero: {
            const auto count = l(0);
            const auto address = l(1);
            for (u32 offset = 0; offset < count; ++offset) {
                machine.memory.write8(address + offset, 0);
            }
            return;
        }
        case Opcode::mcopy: {
            const auto count = l(0);
            const auto source = l(1);
            const auto dest = l(2);
            auto temp = std::vector<u8>(count);
            for (u32 offset = 0; offset < count; ++offset) {
                temp[offset] = machine.memory.read8(source + offset);
            }
            for (u32 offset = 0; offset < count; ++offset) {
                machine.memory.write8(dest + offset, temp[offset]);
            }
            return;
        }
        case Opcode::malloc:
            s(1, 0);
            return;
        case Opcode::mfree:
        case Opcode::accelfunc:
        case Opcode::accelparam:
            return;
        default:
            throw std::runtime_error(std::format(
                "unimplemented opcode 0x{:x}", static_cast<u32>(op)));
    }
}

}  // namespace

void start(Machine& machine) {
    if (machine.running) {
        return;
    }
    const auto start_func = machine.regs.pc;
    machine.stack.sp = 0;
    machine.regs.frame_ptr = 0;
    push_call_stub(machine, {.type = StoreDest::discard, .address = 0}, 0);
    enter_function(machine, start_func, span<const u32>{});
    machine.running = true;
    machine.halted = false;
}

void step(Machine& machine) {
    if (!machine.running) {
        start(machine);
    }
    if (machine.halted) {
        return;
    }
    const auto insn = fetch_instruction(machine);
    execute_instruction(machine, insn);
}

RunResult run_until_halted(Machine& machine, u64 max_steps) {
    RunResult result;
    if (!machine.running) {
        start(machine);
    }
    while (!machine.halted && result.steps < max_steps) {
        step(machine);
        ++result.steps;
    }
    result.halted = machine.halted;
    return result;
}

}  // namespace glupsk
