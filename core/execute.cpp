#include "core/execute.hpp"

#include "core/decode.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <stdexcept>

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

enum class GlkSelector : u32 {
    char_to_lower = 0x00a0,
    char_to_upper = 0x00a1,
    select = 0x00c0,
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

u32 glk_char_to_lower(u32 ch) {
    if ((ch >= 0x41 && ch <= 0x5a) || (ch >= 0xc0 && ch <= 0xd6) ||
        (ch >= 0xd8 && ch <= 0xde)) {
        return ch + 0x20;
    }
    return ch;
}

u32 glk_char_to_upper(u32 ch) {
    if ((ch >= 0x61 && ch <= 0x7a) || (ch >= 0xe0 && ch <= 0xf6) ||
        (ch >= 0xf8 && ch <= 0xfe)) {
        return ch - 0x20;
    }
    return ch;
}

u32 next_random(Machine& machine) {
    machine.rng_state = machine.rng_state * 1664525u + 1013904223u;
    return machine.rng_state;
}

u32 random_in_range(Machine& machine, u32 bound) {
    const auto value = next_random(machine);
    if (bound == 0) {
        return value;
    }
    if ((bound & 0x80000000u) == 0) {
        return value % bound;
    }
    const auto count = 0u - bound;
    return 0u - (value % count);
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

void swap_stack_words(Stack& stack, u32 left, u32 right) {
    const auto value = stack.read32(left);
    stack.write32(left, stack.read32(right));
    stack.write32(right, value);
}

void reverse_stack_words(Stack& stack, u32 start, u32 count) {
    if (count == 0) {
        return;
    }

    auto left = start;
    auto right = start + (count - 1) * 4;
    while (left < right) {
        swap_stack_words(stack, left, right);
        left += 4;
        right -= 4;
    }
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

u32 accelerated_function_for(const Machine& machine, u32 address);
u32 call_accelerated_function(Machine& machine,
                              u32 function,
                              span<const u32> args);

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

void output_char(Machine& machine, u32 ch) {
    if (machine.regs.iosys_mode == 0) {
        return;
    }
    if (machine.regs.iosys_mode != 2) {
        throw std::runtime_error("unsupported I/O system");
    }
    if (machine.glk) {
        machine.glk->put_char(machine, ch);
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

bool accel_function_supported(u32 function);

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

u32 gestalt(u32 selector, u32 arg) {
    switch (static_cast<GestaltSelector>(selector)) {
        case GestaltSelector::glulx_version:
            return 0x00030103;
        case GestaltSelector::terp_version:
            return 0x00000001;
        case GestaltSelector::undo:
        case GestaltSelector::malloc_:
        case GestaltSelector::ext_undo:
            return 0;
        case GestaltSelector::resize_mem:
            return 1;
        case GestaltSelector::io_system:
            return (arg == 0 || arg == 1 || arg == 2) ? 1 : 0;
        case GestaltSelector::unicode:
        case GestaltSelector::mem_copy:
        case GestaltSelector::acceleration:
        case GestaltSelector::float_:
        case GestaltSelector::double_:
            return 1;
        case GestaltSelector::malloc_heap:
            return 0;
        case GestaltSelector::accel_func:
            return accel_function_supported(arg) ? 1 : 0;
        default:
            return 0;
    }
}

bool is_zero_key(const Machine& machine, u32 address, u32 key_size) {
    for (u32 index = 0; index < key_size; ++index) {
        if (machine.memory.read8(address + index) != 0) {
            return false;
        }
    }
    return true;
}

u8 key_byte(const Machine& machine,
            u32 key,
            u32 key_size,
            bool key_indirect,
            u32 index) {
    if (key_indirect) {
        return machine.memory.read8(key + index);
    }
    if (key_size != 1 && key_size != 2 && key_size != 4) {
        throw std::runtime_error("direct search key size must be 1, 2, or 4");
    }
    return static_cast<u8>((key >> ((key_size - index - 1) * 8)) & 0xff);
}

int compare_key_at(Machine& machine,
                   u32 key,
                   u32 key_size,
                   u32 address,
                   bool key_indirect) {
    for (u32 index = 0; index < key_size; ++index) {
        const auto want = key_byte(machine, key, key_size, key_indirect, index);
        const auto got = machine.memory.read8(address + index);
        if (want < got) {
            return -1;
        }
        if (want > got) {
            return 1;
        }
    }
    return 0;
}

u32 failed_search(bool return_index) {
    return return_index ? 0xffffffffu : 0;
}

u32 successful_search(u32 start, u32 struct_size, u32 index, bool return_index) {
    return return_index ? index : start + index * struct_size;
}

u32 linear_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options) {
    if ((options & ~0x07u) != 0) {
        throw std::runtime_error("linearsearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto zero_key_terminates = (options & 0x02) != 0;
    const auto return_index = (options & 0x04) != 0;

    for (u32 index = 0; num_structs == 0xffffffffu || index < num_structs;
         ++index) {
        const auto entry_key = start + index * struct_size + key_offset;
        const auto cmp = compare_key_at(machine, key, key_size, entry_key,
                                        key_indirect);
        if (cmp == 0) {
            return successful_search(start, struct_size, index, return_index);
        }
        if (zero_key_terminates && is_zero_key(machine, entry_key, key_size)) {
            return failed_search(return_index);
        }
    }
    return failed_search(return_index);
}

u32 binary_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options) {
    if ((options & ~0x05u) != 0) {
        throw std::runtime_error("binarysearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto return_index = (options & 0x04) != 0;

    auto low = u32{0};
    auto high = num_structs;
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto cmp = compare_key_at(machine, key, key_size,
                                        start + mid * struct_size + key_offset,
                                        key_indirect);
        if (cmp == 0) {
            return successful_search(start, struct_size, mid, return_index);
        }
        if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return failed_search(return_index);
}

u32 linked_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 key_offset,
                  u32 next_offset,
                  u32 options) {
    if ((options & ~0x03u) != 0) {
        throw std::runtime_error("linkedsearch received unsupported options");
    }
    const auto key_indirect = (options & 0x01) != 0;
    const auto zero_key_terminates = (options & 0x02) != 0;

    auto entry = start;
    while (entry != 0) {
        const auto entry_key = entry + key_offset;
        const auto cmp = compare_key_at(machine, key, key_size, entry_key,
                                        key_indirect);
        if (cmp == 0) {
            return entry;
        }
        if (zero_key_terminates && is_zero_key(machine, entry_key, key_size)) {
            return 0;
        }
        entry = machine.memory.read32(entry + next_offset);
    }
    return 0;
}

u32 accel_arg(span<const u32> args, std::size_t index) {
    return index < args.size() ? args[index] : 0;
}

bool accel_function_supported(u32 function) {
    return function >= 1 && function <= 13;
}

u32 accelerated_function_for(const Machine& machine, u32 address) {
    for (const auto entry : machine.accel_entries) {
        if (entry.address == address) {
            return entry.function;
        }
    }
    return 0;
}

void set_accelerated_function(Machine& machine, u32 function, u32 address) {
    for (auto& entry : machine.accel_entries) {
        if (entry.address == address) {
            entry.function = accel_function_supported(function) ? function : 0;
            return;
        }
    }
    if (!accel_function_supported(function)) {
        return;
    }
    for (auto& entry : machine.accel_entries) {
        if (entry.address == 0) {
            entry = {.address = address, .function = function};
            return;
        }
    }
    throw std::runtime_error("acceleration table is full");
}

void set_acceleration_param(Machine& machine, u32 index, u32 value) {
    if (index < machine.accel_params.size()) {
        machine.accel_params[index] = value;
    }
}

bool accel_obj_in_class(const Machine& machine, u32 obj) {
    return machine.memory.read32(obj + 13 + machine.accel_params[7]) ==
           machine.accel_params[2];
}

u32 accel_z_region(const Machine& machine, u32 address) {
    if (address < 36 ||
        static_cast<std::size_t>(address) >= machine.memory.bytes.size()) {
        return 0;
    }
    const auto type = machine.memory.read8(address);
    if (type >= 0xe0) {
        return 3;
    }
    if (type >= 0xc0) {
        return 2;
    }
    if (type >= 0x70 && type <= 0x7f && address >= machine.memory.ramstart) {
        return 1;
    }
    return 0;
}

u32 accel_cp_tab_at(Machine& machine, u32 obj, u32 id, u32 prop_table_offset) {
    if (accel_z_region(machine, obj) != 1) {
        return 0;
    }
    auto table = machine.memory.read32(obj + prop_table_offset);
    if (table == 0) {
        return 0;
    }
    const auto count = machine.memory.read32(table);
    table += 4;
    return binary_search(machine, id, 2, table, 10, count, 0, 0);
}

u32 accel_cp_tab(Machine& machine, u32 obj, u32 id) {
    return accel_cp_tab_at(machine, obj, id, 16);
}

u32 accel_cp_tab_new(Machine& machine, u32 obj, u32 id) {
    const auto offset = 4 * (3 + machine.accel_params[7] / 4);
    return accel_cp_tab_at(machine, obj, id, offset);
}

u32 accel_get_prop(Machine& machine, u32 obj, u32 id, bool newer);
u32 accel_oc_cl(Machine& machine, u32 obj, u32 cla, bool newer);

u32 accel_get_prop(Machine& machine, u32 obj, u32 id, bool newer) {
    auto cla = u32{0};
    if ((id & 0xffff0000u) != 0) {
        cla = machine.memory.read32(machine.accel_params[0] +
                                    (id & 0xffffu) * 4);
        if (accel_oc_cl(machine, obj, cla, newer) == 0) {
            return 0;
        }
        id >>= 16;
        obj = cla;
    }

    const auto prop =
        newer ? accel_cp_tab_new(machine, obj, id) : accel_cp_tab(machine, obj, id);
    if (prop == 0) {
        return 0;
    }
    if (accel_obj_in_class(machine, obj) && cla == 0) {
        if (id < machine.accel_params[1] || id >= machine.accel_params[1] + 8) {
            return 0;
        }
    }
    if (machine.memory.read32(machine.accel_params[6]) != obj &&
        (machine.memory.read8(prop + 9) & 1) != 0) {
        return 0;
    }
    return prop;
}

u32 accel_ra_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto prop = accel_get_prop(machine, obj, id, newer);
    return prop == 0 ? 0 : machine.memory.read32(prop + 4);
}

u32 accel_rl_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto prop = accel_get_prop(machine, obj, id, newer);
    return prop == 0 ? 0 : 4 * machine.memory.read16(prop + 2);
}

u32 accel_oc_cl(Machine& machine, u32 obj, u32 cla, bool newer) {
    const auto region = accel_z_region(machine, obj);
    if (region == 3) {
        return cla == machine.accel_params[5] ? 1 : 0;
    }
    if (region == 2) {
        return cla == machine.accel_params[4] ? 1 : 0;
    }
    if (region != 1) {
        return 0;
    }

    if (cla == machine.accel_params[2]) {
        return (accel_obj_in_class(machine, obj) || obj == machine.accel_params[2] ||
                obj == machine.accel_params[5] || obj == machine.accel_params[4] ||
                obj == machine.accel_params[3])
                   ? 1
                   : 0;
    }
    if (cla == machine.accel_params[3]) {
        return (accel_obj_in_class(machine, obj) || obj == machine.accel_params[2] ||
                obj == machine.accel_params[5] || obj == machine.accel_params[4] ||
                obj == machine.accel_params[3])
                   ? 0
                   : 1;
    }
    if (cla == machine.accel_params[5] || cla == machine.accel_params[4]) {
        return 0;
    }
    if (!accel_obj_in_class(machine, cla)) {
        return 0;
    }

    const auto prop = accel_get_prop(machine, obj, 2, newer);
    if (prop == 0) {
        return 0;
    }
    const auto list = machine.memory.read32(prop + 4);
    if (list == 0) {
        return 0;
    }
    const auto len = machine.memory.read16(prop + 2);
    for (u32 index = 0; index < len; ++index) {
        if (machine.memory.read32(list + index * 4) == cla) {
            return 1;
        }
    }
    return 0;
}

u32 accel_rv_pr(Machine& machine, span<const u32> args, bool newer) {
    const auto id = accel_arg(args, 1);
    const auto address = accel_ra_pr(machine, accel_arg(args, 0), id, newer);
    if (address == 0) {
        if (id > 0 && id < machine.accel_params[1]) {
            return machine.memory.read32(machine.accel_params[8] + id * 4);
        }
        return 0;
    }
    return machine.memory.read32(address);
}

u32 accel_op_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto region = accel_z_region(machine, obj);
    if (region == 3) {
        return (id == machine.accel_params[1] + 6 ||
                id == machine.accel_params[1] + 7)
                   ? 1
                   : 0;
    }
    if (region == 2) {
        return id == machine.accel_params[1] + 5 ? 1 : 0;
    }
    if (region != 1) {
        return 0;
    }
    if (id >= machine.accel_params[1] && id < machine.accel_params[1] + 8 &&
        accel_obj_in_class(machine, obj)) {
        return 1;
    }
    return accel_ra_pr(machine, obj, id, newer) != 0 ? 1 : 0;
}

u32 call_accelerated_function(Machine& machine,
                              u32 function,
                              span<const u32> args) {
    switch (function) {
        case 1:
            return accel_z_region(machine, accel_arg(args, 0));
        case 2:
            return accel_cp_tab(machine, accel_arg(args, 0), accel_arg(args, 1));
        case 3:
            return accel_ra_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 4:
            return accel_rl_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 5:
            return accel_oc_cl(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 6:
            return accel_rv_pr(machine, args, false);
        case 7:
            return accel_op_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 8:
            return accel_cp_tab_new(machine, accel_arg(args, 0), accel_arg(args, 1));
        case 9:
            return accel_ra_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 10:
            return accel_rl_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 11:
            return accel_oc_cl(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 12:
            return accel_rv_pr(machine, args, true);
        case 13:
            return accel_op_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        default:
            return 0;
    }
}

void execute_instruction(Machine& machine, const Instruction& insn) {
    machine.regs.pc = insn.next_pc;
    const auto op = insn.opcode;
    const auto& a = insn.operands;
    auto values = std::array<u32, 8>{};
    for (u8 index = 0; index < insn.operand_count; ++index) {
        if (!is_store_operand(op, index)) {
            values[index] = load_operand(machine, a[index], load_width_for(op, index));
        }
    }

    auto l = [&](int index) {
        return values[static_cast<std::size_t>(index)];
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
            s(1, l(0), 2);
            return;
        case Opcode::copyb:
            s(1, l(0), 1);
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
            auto places = places_signed % static_cast<std::int32_t>(count);
            if (places < 0) {
                places += static_cast<std::int32_t>(count);
            }
            const auto start = machine.stack.sp - count * 4;
            reverse_stack_words(machine.stack, start, count);
            reverse_stack_words(machine.stack, start, static_cast<u32>(places));
            reverse_stack_words(machine.stack, start + static_cast<u32>(places) * 4,
                                count - static_cast<u32>(places));
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
        {
            auto buffer = std::array<char, 16>{};
            const auto value = as_signed(l(0));
            const auto [end, ec] =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (ec != std::errc{}) {
                throw std::runtime_error("streamnum conversion failed");
            }
            for (auto cursor = buffer.data(); cursor != end; ++cursor) {
                const auto ch = *cursor;
                output_char(machine, static_cast<u8>(ch));
            }
            return;
        }
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
        case Opcode::setmemsize: {
            const auto new_size = l(0);
            if ((new_size & 0xffu) != 0 || new_size < machine.memory.endmem) {
                s(1, 1);
                return;
            }
            machine.memory.bytes.resize(new_size);
            s(1, 0);
            return;
        }
        case Opcode::jumpabs:
            machine.regs.pc = l(0);
            return;
        case Opcode::random:
            s(1, random_in_range(machine, l(0)));
            return;
        case Opcode::setrandom:
            machine.rng_state = l(0) == 0 ? 0x6d2b79f5 : l(0);
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
            if (static_cast<GlkSelector>(selector) == GlkSelector::select &&
                machine.glk && machine.glk->select_would_block()) {
                machine.regs.pc = insn.address;
                machine.blocked = true;
                return;
            }
            auto args = pop_arguments(machine, argc);
            auto result = u32{0};
            if (static_cast<GlkSelector>(selector) == GlkSelector::char_to_lower &&
                args.count == 1) {
                result = glk_char_to_lower(args.values[0]);
            } else if (static_cast<GlkSelector>(selector) == GlkSelector::char_to_upper &&
                       args.count == 1) {
                result = glk_char_to_upper(args.values[0]);
            } else if (machine.glk) {
                result = machine.glk->call(machine, selector, args.as_span());
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
        case Opcode::linearsearch:
            s(7, linear_search(machine, l(0), l(1), l(2), l(3), l(4), l(5),
                                l(6)));
            return;
        case Opcode::binarysearch:
            s(7, binary_search(machine, l(0), l(1), l(2), l(3), l(4), l(5), l(6)));
            return;
        case Opcode::linkedsearch:
            s(6, linked_search(machine, l(0), l(1), l(2), l(3), l(4), l(5)));
            return;
        case Opcode::callf: {
            const auto fn = l(0);
            const auto dest = store_ref(machine, a[1]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, span<const u32>{});
            return;
        }
        case Opcode::callfi: {
            const auto fn = l(0);
            const auto args = std::array<u32, 1>{l(1)};
            const auto dest = store_ref(machine, a[2]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case Opcode::callfii: {
            const auto fn = l(0);
            const auto args = std::array<u32, 2>{l(1), l(2)};
            const auto dest = store_ref(machine, a[3]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case Opcode::callfiii: {
            const auto fn = l(0);
            const auto args = std::array<u32, 3>{l(1), l(2), l(3)};
            const auto dest = store_ref(machine, a[4]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
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
            if (dest < source) {
                for (u32 offset = 0; offset < count; ++offset) {
                    machine.memory.write8(dest + offset,
                                          machine.memory.read8(source + offset));
                }
            } else {
                for (u32 offset = count; offset > 0; --offset) {
                    machine.memory.write8(
                        dest + offset - 1,
                        machine.memory.read8(source + offset - 1));
                }
            }
            return;
        }
        case Opcode::malloc:
            throw std::runtime_error("malloc is not implemented");
        case Opcode::mfree:
            throw std::runtime_error("mfree is not implemented");
        case Opcode::accelfunc:
            set_accelerated_function(machine, l(0), l(1));
            return;
        case Opcode::accelparam:
            set_acceleration_param(machine, l(0), l(1));
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
    machine.blocked = false;
    machine.halted = false;
}

void resume(Machine& machine) {
    if (!machine.halted) {
        machine.blocked = false;
    }
}

void step(Machine& machine) {
    if (!machine.running) {
        start(machine);
    }
    if (machine.halted || machine.blocked) {
        return;
    }
    const auto insn = fetch_instruction(machine);
    execute_instruction(machine, insn);
}

RunResult run_until_blocked(Machine& machine, u64 max_steps) {
    RunResult result;
    if (!machine.running) {
        start(machine);
    }
    while (!machine.halted && !machine.blocked && result.steps < max_steps) {
        step(machine);
        ++result.steps;
    }
    result.blocked = machine.blocked;
    result.halted = machine.halted;
    return result;
}

RunResult run_until_halted(Machine& machine, u64 max_steps) {
    return run_until_blocked(machine, max_steps);
}

}  // namespace glupsk
