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

struct StoreRef {
    u8 type = 0;
    u32 address = 0;
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
        case 0x0:
            return 0;
        case 0x1:
            return sign_extend(operand.data, 8);
        case 0x2:
            return sign_extend(operand.data, 16);
        case 0x3:
            return operand.data;
        case 0x5:
        case 0x6:
        case 0x7:
            switch (width) {
                case 1:
                    return machine.memory.read8(operand.data);
                case 2:
                    return machine.memory.read16(operand.data);
                case 4:
                    return machine.memory.read32(operand.data);
            }
            break;
        case 0x8:
            return machine.stack.pop32();
        case 0x9:
        case 0xa:
        case 0xb:
            return read_local(machine, operand.data, width);
        case 0xd:
        case 0xe:
        case 0xf:
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
        case 0x0:
            return {.type = 0, .address = 0};
        case 0x5:
        case 0x6:
        case 0x7:
            return {.type = 1, .address = operand.data};
        case 0x8:
            return {.type = 3, .address = 0};
        case 0x9:
        case 0xa:
        case 0xb:
            return {.type = 2, .address = operand.data};
        case 0xd:
        case 0xe:
        case 0xf:
            return {.type = 1, .address = ram_address(machine, operand.data)};
        default:
            throw std::runtime_error("invalid store operand");
    }
}

void store_value(Machine& machine, StoreRef ref, u32 value, u8 width = 4) {
    switch (ref.type) {
        case 0:
            return;
        case 1:
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
        case 2:
            write_local(machine, ref.address, value, width);
            return;
        case 3:
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

std::vector<u32> pop_arguments(Machine& machine, u32 argc) {
    auto args = std::vector<u32>(argc);
    for (u32 index = 0; index < argc; ++index) {
        args[index] = machine.stack.pop32();
    }
    return args;
}

void push_call_stub(Machine& machine, StoreRef dest, u32 pc) {
    machine.stack.push32(dest.type);
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
    const auto dest = StoreRef{.type = static_cast<u8>(machine.stack.read32(machine.stack.sp)),
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
    auto format_bytes = std::vector<u8>{};
    auto local_len = u32{0};
    while (true) {
        const auto local_type = machine.memory.read8(address++);
        const auto local_count = machine.memory.read8(address++);
        format_bytes.push_back(local_type);
        format_bytes.push_back(local_count);
        if (local_type == 0) {
            if ((format_bytes.size() / 2) & 1) {
                format_bytes.push_back(0);
                format_bytes.push_back(0);
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

    const auto locals_pos_value = static_cast<u32>(8 + format_bytes.size());
    const auto frame_len_value = locals_pos_value + local_len;

    if (static_cast<std::size_t>(frame_ptr) + frame_len_value >
        machine.stack.bytes.size()) {
        throw std::runtime_error("stack overflow in function call");
    }

    machine.stack.write32(frame_ptr, frame_len_value);
    machine.stack.write32(frame_ptr + 4, locals_pos_value);
    for (auto index = std::size_t{0}; index < format_bytes.size(); ++index) {
        machine.stack.write8(frame_ptr + 8 + static_cast<u32>(index),
                             format_bytes[index]);
    }
    for (u32 offset = 0; offset < local_len; ++offset) {
        machine.stack.write8(frame_ptr + locals_pos_value + offset, 0);
    }

    machine.regs.frame_ptr = frame_ptr;
    machine.stack.sp = frame_ptr + frame_len_value;
    machine.regs.pc = address;

    if (type == 0xc0) {
        for (auto it = args.rbegin(); it != args.rend(); ++it) {
            machine.stack.push32(*it);
        }
        machine.stack.push32(static_cast<u32>(args.size()));
        return;
    }

    auto format_cursor = frame_ptr + 8;
    auto local_cursor = frame_ptr + locals_pos_value;
    auto arg_index = std::size_t{0};
    while (arg_index < args.size()) {
        const auto local_type = machine.stack.read8(format_cursor++);
        const auto local_count = machine.stack.read8(format_cursor++);
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
    const auto type = machine.memory.read8(node);
    switch (type) {
        case 0x02:
            output_char(machine, machine.memory.read8(node + 1));
            return;
        case 0x03:
            output_c_string(machine, node + 1);
            return;
        case 0x04:
            output_char(machine, machine.memory.read32(node + 1));
            return;
        case 0x05:
            output_uni_string(machine, node + 1);
            return;
        case 0x08: {
            const auto address = machine.memory.read32(node + 1);
            output_string_object(machine, address);
            return;
        }
        case 0x09: {
            const auto address = machine.memory.read32(machine.memory.read32(node + 1));
            output_string_object(machine, address);
            return;
        }
        case 0x0a:
        case 0x0b:
            throw std::runtime_error("string function references are not implemented");
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
        while (machine.memory.read8(node) == 0) {
            const auto byte = machine.memory.read8(byte_address);
            const auto take_right = (byte >> bit) & 1;
            ++bit;
            if (bit == 8) {
                bit = 0;
                ++byte_address;
            }
            node = machine.memory.read32(node + (take_right ? 5 : 1));
        }

        if (machine.memory.read8(node) == 1) {
            return;
        }
        output_huffman_leaf(machine, node);
    }
}

void output_string_object(Machine& machine, u32 address) {
    const auto type = machine.memory.read8(address);
    switch (type) {
        case 0xe0:
            output_c_string(machine, address + 1);
            return;
        case 0xe1:
            output_compressed_string(machine, address + 1);
            return;
        case 0xe2:
            output_uni_string(machine, address + 4);
            return;
        default:
            throw std::runtime_error("streamstr target is not a string");
    }
}

u32 gestalt(u32 selector, u32 arg) {
    switch (selector) {
        case 0:
            return 0x00030103;
        case 1:
            return 0x00000001;
        case 2:
        case 3:
        case 7:
        case 9:
        case 11:
        case 12:
        case 13:
            return 0;
        case 4:
            return (arg == 0 || arg == 1 || arg == 2) ? 1 : 0;
        case 5:
        case 6:
            return 1;
        case 8:
        case 10:
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
        case 0x00:
            return;
        case 0x10:
            s(2, l(0) + l(1));
            return;
        case 0x11:
            s(2, l(0) - l(1));
            return;
        case 0x12:
            s(2, l(0) * l(1));
            return;
        case 0x13:
            s(2, static_cast<u32>(as_signed(l(0)) / as_signed(l(1))));
            return;
        case 0x14:
            s(2, static_cast<u32>(as_signed(l(0)) % as_signed(l(1))));
            return;
        case 0x15:
            s(1, -l(0));
            return;
        case 0x18:
            s(2, l(0) & l(1));
            return;
        case 0x19:
            s(2, l(0) | l(1));
            return;
        case 0x1a:
            s(2, l(0) ^ l(1));
            return;
        case 0x1b:
            s(1, ~l(0));
            return;
        case 0x1c: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) << amount);
            return;
        }
        case 0x1d: {
            const auto value = l(0);
            const auto amount = l(1);
            if (amount >= 32) {
                s(2, (value & 0x80000000u) ? 0xffffffffu : 0);
            } else {
                s(2, static_cast<u32>(as_signed(value) >> amount));
            }
            return;
        }
        case 0x1e: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) >> amount);
            return;
        }
        case 0x20:
            branch_to(machine, insn.next_pc, l(0));
            return;
        case 0x22:
            b(l(0) == 0, l(1));
            return;
        case 0x23:
            b(l(0) != 0, l(1));
            return;
        case 0x24:
            b(l(0) == l(1), l(2));
            return;
        case 0x25:
            b(l(0) != l(1), l(2));
            return;
        case 0x26:
            b(as_signed(l(0)) < as_signed(l(1)), l(2));
            return;
        case 0x27:
            b(as_signed(l(0)) >= as_signed(l(1)), l(2));
            return;
        case 0x28:
            b(as_signed(l(0)) > as_signed(l(1)), l(2));
            return;
        case 0x29:
            b(as_signed(l(0)) <= as_signed(l(1)), l(2));
            return;
        case 0x2a:
            b(l(0) < l(1), l(2));
            return;
        case 0x2b:
            b(l(0) >= l(1), l(2));
            return;
        case 0x2c:
            b(l(0) > l(1), l(2));
            return;
        case 0x2d:
            b(l(0) <= l(1), l(2));
            return;
        case 0x30: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            const auto args = pop_arguments(machine, argc);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case 0x31:
            finish_return(machine, l(0));
            return;
        case 0x32: {
            const auto offset = l(1);
            const auto dest = store_ref(machine, a[0]);
            push_call_stub(machine, dest, insn.next_pc);
            const auto token = machine.stack.sp;
            store_value(machine, dest, token);
            branch_to(machine, insn.next_pc, offset);
            return;
        }
        case 0x33: {
            const auto value = l(0);
            const auto token = l(1);
            machine.stack.sp = token;
            pop_call_stub(machine, value);
            return;
        }
        case 0x34: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto args = pop_arguments(machine, argc);
            machine.stack.sp = machine.regs.frame_ptr;
            enter_function(machine, fn, args);
            return;
        }
        case 0x40:
            s(1, l(0));
            return;
        case 0x41:
            s(1, l(0, 2), 2);
            return;
        case 0x42:
            s(1, l(0, 1), 1);
            return;
        case 0x44:
            s(1, sign_extend(l(0) & 0xffff, 16));
            return;
        case 0x45:
            s(1, sign_extend(l(0) & 0xff, 8));
            return;
        case 0x48:
            s(2, machine.memory.read32(l(0) + 4 * as_signed(l(1))));
            return;
        case 0x49:
            s(2, machine.memory.read16(l(0) + 2 * as_signed(l(1))));
            return;
        case 0x4a:
            s(2, machine.memory.read8(l(0) + as_signed(l(1))));
            return;
        case 0x4b: {
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
        case 0x4c:
            machine.memory.write32(l(0) + 4 * as_signed(l(1)), l(2));
            return;
        case 0x4d:
            machine.memory.write16(l(0) + 2 * as_signed(l(1)),
                                   static_cast<u16>(l(2)));
            return;
        case 0x4e:
            machine.memory.write8(l(0) + as_signed(l(1)), static_cast<u8>(l(2)));
            return;
        case 0x4f: {
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
        case 0x50:
            s(0, (machine.stack.sp - value_stack_base(machine)) / 4);
            return;
        case 0x51: {
            const auto index = l(0);
            s(1, machine.stack.read32(machine.stack.sp - 4 - 4 * index));
            return;
        }
        case 0x52: {
            const auto first = machine.stack.pop32();
            const auto second = machine.stack.pop32();
            machine.stack.push32(first);
            machine.stack.push32(second);
            return;
        }
        case 0x53: {
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
        case 0x54: {
            const auto count = l(0);
            const auto start = machine.stack.sp - count * 4;
            for (u32 index = 0; index < count; ++index) {
                machine.stack.push32(machine.stack.read32(start + index * 4));
            }
            return;
        }
        case 0x70:
            output_char(machine, l(0) & 0xff);
            return;
        case 0x71:
            for (const auto ch : std::format("{}", as_signed(l(0)))) {
                output_char(machine, static_cast<u8>(ch));
            }
            return;
        case 0x72:
            output_string_object(machine, l(0));
            return;
        case 0x73:
            output_char(machine, l(0));
            return;
        case 0x100:
            s(2, gestalt(l(0), l(1)));
            return;
        case 0x101:
            throw std::runtime_error("debugtrap");
        case 0x102:
            s(0, static_cast<u32>(machine.memory.bytes.size()));
            return;
        case 0x103:
            s(1, 1);
            return;
        case 0x104:
            machine.regs.pc = l(0);
            return;
        case 0x110:
            s(1, 0);
            return;
        case 0x111:
            return;
        case 0x120:
            machine.halted = true;
            machine.running = false;
            return;
        case 0x121:
            s(0, 0);
            return;
        case 0x125:
        case 0x126:
        case 0x128:
            s(0, 1);
            return;
        case 0x127:
        case 0x129:
            return;
        case 0x130: {
            const auto selector = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            auto args = pop_arguments(machine, argc);
            auto result = u32{0};
            if (selector == 0x00a0 && args.size() == 1) {
                result = static_cast<u8>(std::tolower(static_cast<unsigned char>(args[0])));
            } else if (selector == 0x00a1 && args.size() == 1) {
                result = static_cast<u8>(std::toupper(static_cast<unsigned char>(args[0])));
            } else if (machine.glk) {
                result = machine.glk->call(selector, args);
            }
            store_value(machine, dest, result);
            return;
        }
        case 0x140:
            s(0, machine.regs.string_table);
            return;
        case 0x141:
            machine.regs.string_table = l(0);
            return;
        case 0x148:
            store_value(machine, a[0], machine.regs.iosys_mode);
            store_value(machine, a[1], machine.regs.iosys_rock);
            return;
        case 0x149:
            machine.regs.iosys_mode = l(0);
            machine.regs.iosys_rock = l(1);
            if (machine.regs.iosys_mode != 0 && machine.regs.iosys_mode != 2) {
                machine.regs.iosys_mode = 0;
            }
            return;
        case 0x151:
            s(7, binary_search(machine, l(0), l(1), l(2), l(3), l(4), l(5), l(6)));
            return;
        case 0x160: {
            const auto dest = store_ref(machine, a[1]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), span<const u32>{});
            return;
        }
        case 0x161: {
            const auto args = std::array<u32, 1>{l(1)};
            const auto dest = store_ref(machine, a[2]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case 0x162: {
            const auto args = std::array<u32, 2>{l(1), l(2)};
            const auto dest = store_ref(machine, a[3]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case 0x163: {
            const auto args = std::array<u32, 3>{l(1), l(2), l(3)};
            const auto dest = store_ref(machine, a[4]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, l(0), args);
            return;
        }
        case 0x170: {
            const auto count = l(0);
            const auto address = l(1);
            for (u32 offset = 0; offset < count; ++offset) {
                machine.memory.write8(address + offset, 0);
            }
            return;
        }
        case 0x171: {
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
        case 0x178:
            s(1, 0);
            return;
        case 0x179:
        case 0x180:
        case 0x181:
            return;
        default:
            throw std::runtime_error(std::format("unimplemented opcode 0x{:x}", op));
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
    push_call_stub(machine, {.type = 0, .address = 0}, 0);
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
