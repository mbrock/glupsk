#include "core/machine.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace glupsk {
namespace {

enum class GlkSelector : u32 {
    exit = 0x0001,
    gestalt = 0x0004,
    window_open = 0x0023,
    window_get_stream = 0x002c,
    set_window = 0x002f,
    put_char = 0x0080,
    put_char_stream = 0x0081,
    put_string = 0x0082,
    put_string_stream = 0x0083,
    put_buffer = 0x0084,
    put_buffer_stream = 0x0085,
    set_style = 0x0086,
    char_to_lower = 0x00a0,
    char_to_upper = 0x00a1,
    select = 0x00c0,
    request_line_event = 0x00d0,
    put_char_uni = 0x0120,
    put_string_uni = 0x0121,
    put_buffer_uni = 0x0122,
    put_char_stream_uni = 0x0123,
    put_string_stream_uni = 0x0124,
    put_buffer_stream_uni = 0x0125,
};

enum class GlkGestaltSelector : u32 {
    version = 0,
    char_output = 3,
    unicode = 15,
    unicode_norm = 16,
};

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

std::size_t checked_stack_offset(const Stack& stack,
                                 u32 address,
                                 std::size_t width) {
    const auto offset = static_cast<std::size_t>(address);
    if (offset > stack.bytes.size() || stack.bytes.size() - offset < width) {
        throw std::runtime_error("stack access is outside stack memory");
    }
    return offset;
}

void put_latin1(TranscriptGlk& glk, std::string_view text) {
    for (const auto ch : text) {
        glk.put_char(static_cast<unsigned char>(ch));
    }
}

void put_c_string(TranscriptGlk& glk, const Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read8(address++);
        if (ch == 0) {
            return;
        }
        glk.put_char(ch);
    }
}

void put_uni_string(TranscriptGlk& glk, const Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read32(address);
        address += 4;
        if (ch == 0) {
            return;
        }
        glk.put_char(ch);
    }
}

void put_byte_buffer(TranscriptGlk& glk,
                     const Machine& machine,
                     u32 address,
                     u32 len) {
    for (u32 index = 0; index < len; ++index) {
        glk.put_char(machine.memory.read8(address + index));
    }
}

void put_uni_buffer(TranscriptGlk& glk,
                    const Machine& machine,
                    u32 address,
                    u32 len) {
    for (u32 index = 0; index < len; ++index) {
        glk.put_char(machine.memory.read32(address + index * 4));
    }
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

u8 Stack::read8(u32 address) const {
    return bytes[checked_stack_offset(*this, address, 1)];
}

u16 Stack::read16(u32 address) const {
    return read_u16_be(bytes, checked_stack_offset(*this, address, 2));
}

u32 Stack::read32(u32 address) const {
    return read_u32_be(bytes, checked_stack_offset(*this, address, 4));
}

void Stack::write8(u32 address, u8 value) {
    bytes[checked_stack_offset(*this, address, 1)] = value;
}

void Stack::write16(u32 address, u16 value) {
    write_u16_be(bytes, checked_stack_offset(*this, address, 2), value);
}

void Stack::write32(u32 address, u32 value) {
    write_u32_be(bytes, checked_stack_offset(*this, address, 4), value);
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

void TranscriptGlk::add_input_line(std::string line) {
    input_lines.push_back(std::move(line));
}

bool TranscriptGlk::select_would_block() const {
    return !line_pending || input_lines.empty();
}

u32 TranscriptGlk::call(Machine& machine, u32 selector, span<const u32> args) {
    switch (static_cast<GlkSelector>(selector)) {
        case GlkSelector::exit:
            machine.halted = true;
            machine.running = false;
            return 0;
        case GlkSelector::gestalt:
            if (args.empty()) {
                return 0;
            }
            switch (static_cast<GlkGestaltSelector>(args[0])) {
                case GlkGestaltSelector::version:
                    return 0x00000706;
                case GlkGestaltSelector::char_output:
                    return 2;
                case GlkGestaltSelector::unicode:
                    return 1;
                case GlkGestaltSelector::unicode_norm:
                    return 0;
                default:
                    return 0;
            }
        case GlkSelector::window_open:
            return 1;
        case GlkSelector::window_get_stream:
            return args.empty() ? 1 : args[0];
        case GlkSelector::set_window:
        case GlkSelector::set_style:
            return 0;
        case GlkSelector::put_char:
        case GlkSelector::put_char_stream:
        case GlkSelector::put_char_uni:
        case GlkSelector::put_char_stream_uni:
            if (!args.empty()) {
                put_char(args.back());
            }
            return 0;
        case GlkSelector::put_string:
        case GlkSelector::put_string_stream:
            if (!args.empty()) {
                put_c_string(*this, machine, args.back() + 1);
            }
            return 0;
        case GlkSelector::put_string_uni:
        case GlkSelector::put_string_stream_uni:
            if (!args.empty()) {
                put_uni_string(*this, machine, args.back() + 4);
            }
            return 0;
        case GlkSelector::put_buffer:
            if (args.size() >= 2) {
                put_byte_buffer(*this, machine, args[0], args[1]);
            }
            return 0;
        case GlkSelector::put_buffer_stream:
            if (args.size() >= 3) {
                put_byte_buffer(*this, machine, args[1], args[2]);
            }
            return 0;
        case GlkSelector::put_buffer_uni:
            if (args.size() >= 2) {
                put_uni_buffer(*this, machine, args[0], args[1]);
            }
            return 0;
        case GlkSelector::put_buffer_stream_uni:
            if (args.size() >= 3) {
                put_uni_buffer(*this, machine, args[1], args[2]);
            }
            return 0;
        case GlkSelector::request_line_event:
            if (args.size() >= 3) {
                line_pending = true;
                line_window = args[0];
                line_buffer = args[1];
                line_maxlen = args[2];
                line_initial_len = args.size() >= 4 ? args[3] : 0;
            }
            return 0;
        case GlkSelector::select: {
            if (args.empty() || !line_pending || input_lines.empty()) {
                return 0;
            }

            const auto event_address = args[0];
            auto line = std::move(input_lines.front());
            input_lines.pop_front();

            const auto count = std::min<u32>(
                line_maxlen, static_cast<u32>(line.size()));
            for (u32 index = 0; index < count; ++index) {
                machine.memory.write8(line_buffer + index,
                                      static_cast<u8>(line[index]));
            }
            machine.memory.write32(event_address, 3);
            machine.memory.write32(event_address + 4, line_window);
            machine.memory.write32(event_address + 8, count);
            machine.memory.write32(event_address + 12, 0);
            line_pending = false;
            put_latin1(*this, line);
            put_char('\n');
            return 0;
        }
        case GlkSelector::char_to_lower:
        case GlkSelector::char_to_upper:
            return args.empty() ? 0 : args[0];
        default:
            return 0;
    }
}

void TranscriptGlk::put_char(u32 ch) {
    if (ch == '\r') {
        ch = '\n';
    }
    if (ch <= 0x7f) {
        transcript.push_back(static_cast<char>(ch));
        return;
    }
    transcript.push_back('?');
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
