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

    const auto& unicode = std::get<std::vector<u32>>(text);
    const auto count =
        std::min<u32>(pending.max_length, static_cast<u32>(unicode.size()));
    for (u32 index = 0; index < count; ++index) {
        if (pending.encoding == GlkTextEncoding::unicode) {
            machine.memory.write32(pending.buffer_address + index * 4,
                                   unicode[index]);
        } else {
            machine.memory.write8(pending.buffer_address + index,
                                  static_cast<u8>(unicode[index]));
        }
    }
    return count;
}

}  // namespace glupsk
