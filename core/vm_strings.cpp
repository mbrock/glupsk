#include "core/vm_strings.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

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

void output_c_string(Machine& machine, u32 address);
void output_uni_string(Machine& machine, u32 address);
void output_compressed_string(Machine& machine, u32 address);
void output_huffman_leaf(Machine& machine, u32 node);

}  // namespace

void output_char(Machine& machine, u32 ch) {
    if (machine.regs.iosys_mode == 0) {
        return;
    }
    if (machine.regs.iosys_mode != 2) {
        throw std::runtime_error("unsupported I/O system");
    }
    // Stream opcodes currently write synchronously; blocked output will need a
    // resumable opcode state instead of throwing through this path.
    if (machine.glk) {
        machine.glk->put_char(machine, ch);
    }
}

namespace {

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

}  // namespace

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

}  // namespace glupsk
