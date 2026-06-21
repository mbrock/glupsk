#include "core/machine.hpp"

#include "core/bytes.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

std::size_t checked_offset(const Memory& memory, u32 address, std::size_t width) {
    const auto offset = static_cast<std::size_t>(address);
    if (offset > memory.bytes.size() || memory.bytes.size() - offset < width) {
        throw std::runtime_error("memory access is outside ENDMEM");
    }
    return offset;
}

std::size_t checked_write_offset(const Memory& memory,
                                 u32 address,
                                 std::size_t width) {
    const auto offset = checked_offset(memory, address, width);
    if (address < memory.ramstart) {
        throw std::runtime_error("memory write targets ROM");
    }
    return offset;
}

}  // namespace

u8 Memory::read8(u32 address) const {
    return bytes[checked_offset(*this, address, 1)];
}

u16 Memory::read16(u32 address) const {
    return read_u16_be(bytes, checked_offset(*this, address, 2));
}

u32 Memory::read32(u32 address) const {
    return read_u32_be(bytes, checked_offset(*this, address, 4));
}

void Memory::write8(u32 address, u8 value) {
    bytes[checked_write_offset(*this, address, 1)] = value;
}

void Memory::write16(u32 address, u16 value) {
    write_u16_be(bytes, checked_write_offset(*this, address, 2), value);
}

void Memory::write32(u32 address, u32 value) {
    write_u32_be(bytes, checked_write_offset(*this, address, 4), value);
}

void Stack::push32(u32 value) {
    if (bytes.size() - sp < 4) {
        throw std::runtime_error("stack overflow");
    }
    write_u32_be(bytes, sp, value);
    sp += 4;
}

u32 Stack::pop32() {
    if (sp < 4) {
        throw std::runtime_error("stack underflow");
    }
    sp -= 4;
    return read_u32_be(bytes, sp);
}

Machine Machine::from_story(const Story& story) {
    const auto& header = story.header();
    auto story_bytes = story.bytes();

    Machine machine;
    machine.memory.bytes = Bytes(story_bytes.begin(), story_bytes.end());
    machine.memory.bytes.resize(header.endmem);
    machine.memory.ramstart = header.ramstart;
    machine.memory.extstart = header.extstart;
    machine.memory.endmem = header.endmem;

    machine.stack.bytes.resize(header.stack_size);

    machine.regs.pc = header.start_func;
    machine.regs.string_table = header.decoding_table;

    return machine;
}

}  // namespace glupsk
