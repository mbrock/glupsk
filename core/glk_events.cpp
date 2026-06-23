#include "core/glk_events.hpp"

#include <algorithm>

namespace glupsk {

void glk_write_ref(Machine& machine, u32 address, u32 value) {
    if (address == 0) {
        return;
    }
    // Glk uses 0xffffffff as a stack result reference.
    if (address == 0xffffffffu) {
        machine.stack.push32(value);
        return;
    }
    machine.memory.write32(address, value);
}

void glk_write_event_field(Machine& machine, u32 address, u32 field, u32 value) {
    if (address == 0) {
        return;
    }
    if (address == 0xffffffffu) {
        machine.stack.push32(value);
        return;
    }
    machine.memory.write32(address + field * 4, value);
}

u32 glk_write_input_text(Machine& machine,
                         const GlkPendingLine& pending,
                         const GlkInputText& text) {
    if (const auto* latin1 = std::get_if<std::string>(&text)) {
        const auto count =
            std::min<u32>(pending.max_length, static_cast<u32>(latin1->size()));
        for (u32 index = 0; index < count; ++index) {
            const auto ch = static_cast<u8>((*latin1)[index]);
            if (pending.encoding == GlkTextEncoding::unicode) {
                machine.memory.write32(pending.buffer_address + index * 4, ch);
            } else {
                machine.memory.write8(pending.buffer_address + index, ch);
            }
        }
        return count;
    }

    const auto write_codepoint = [&](u32 index, u32 ch) {
        if (pending.encoding == GlkTextEncoding::unicode) {
            machine.memory.write32(pending.buffer_address + index * 4,
                                   ch);
        } else {
            machine.memory.write8(pending.buffer_address + index,
                                  static_cast<u8>(ch));
        }
    };

    auto count = u32{0};
    const auto write_span = [&](span<const u32> codepoints) {
        for (const auto ch : codepoints) {
            if (count == pending.max_length) {
                return;
            }
            write_codepoint(count, ch);
            ++count;
        }
    };

    if (const auto* unicode = std::get_if<std::vector<u32>>(&text)) {
        write_span(*unicode);
    } else if (const auto* unicode = std::get_if<span<const u32>>(&text)) {
        write_span(*unicode);
    } else {
        for (const auto chunk : std::get<GlkCodepointChunks>(text).chunks) {
            write_span(chunk);
            if (count == pending.max_length) {
                break;
            }
        }
    }
    return count;
}

}  // namespace glupsk
