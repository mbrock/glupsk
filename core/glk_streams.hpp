#pragma once

#include "core/glk_registry.hpp"
#include "core/machine.hpp"

#include <stdexcept>

namespace glupsk {

inline void glk_validate_memory_stream_mode(u32 mode) {
    if (mode != 0 && mode != 1 && mode != 2 && mode != 3) {
        throw std::runtime_error(
            std::format("unsupported memory stream mode {}", mode));
    }
}

template <typename Host>
void glk_write_to_memory_stream(Machine& machine,
                                GlkStreamRecord<Host>& record,
                                const GlkMemoryStream& stream,
                                const GlkTextData& text) {
    if (stream.mode == 2) {
        throw std::runtime_error("cannot write to read-only memory stream");
    }
    const auto write_codepoint = [&](u32 ch) {
        if (record.pos < stream.len && stream.address != 0) {
            // Byte memory streams intentionally truncate Unicode codepoints.
            machine.memory.write8(stream.address + record.pos, static_cast<u8>(ch));
        }
        ++record.pos;
        ++record.write_count;
    };

    if (const auto* bytes = std::get_if<std::string>(&text)) {
        for (const auto ch : *bytes) {
            write_codepoint(static_cast<u8>(ch));
        }
        return;
    }
    for (const auto ch : std::get<std::vector<u32>>(text)) {
        write_codepoint(ch);
    }
}

template <typename Host>
void glk_write_to_unicode_memory_stream(Machine& machine,
                                        GlkStreamRecord<Host>& record,
                                        const GlkUnicodeMemoryStream& stream,
                                        const GlkTextData& text) {
    if (stream.mode == 2) {
        throw std::runtime_error("cannot write to read-only unicode memory stream");
    }
    const auto write_codepoint = [&](u32 ch) {
        if (record.pos < stream.len && stream.address != 0) {
            machine.memory.write32(stream.address + record.pos * 4, ch);
        }
        ++record.pos;
        ++record.write_count;
    };

    if (const auto* bytes = std::get_if<std::string>(&text)) {
        for (const auto ch : *bytes) {
            write_codepoint(static_cast<u8>(ch));
        }
        return;
    }
    for (const auto ch : std::get<std::vector<u32>>(text)) {
        write_codepoint(ch);
    }
}

}  // namespace glupsk
