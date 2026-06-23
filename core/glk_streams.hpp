#pragma once

#include "core/error.hpp"
#include "core/glk_registry.hpp"
#include "core/machine.hpp"
#include "core/glk_text.hpp"

namespace glupsk {

inline void glk_validate_memory_stream_mode(u32 mode) {
    if (mode != 0 && mode != 1 && mode != 2 && mode != 3) {
        fail("unsupported memory stream mode");
    }
}

template <typename Host>
void glk_write_to_memory_stream(Machine& machine,
                                GlkStreamRecord<Host>& record,
                                const GlkMemoryStream& stream,
                                const GlkTextData& text) {
    if (stream.mode == 2) {
        fail("cannot write to read-only memory stream");
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
    if (const auto* codepoints = std::get_if<std::vector<u32>>(&text)) {
        for (const auto ch : *codepoints) {
            write_codepoint(ch);
        }
        return;
    }
    if (const auto* codepoints = std::get_if<span<const u32>>(&text)) {
        for (const auto ch : *codepoints) {
            write_codepoint(ch);
        }
        return;
    }
    for (const auto chunk : std::get<GlkCodepointChunks>(text).chunks) {
        for (const auto ch : chunk) {
            write_codepoint(ch);
        }
    }
}

template <typename WriteCodepoint>
void glk_write_unicode_text(const GlkTextData& text,
                            WriteCodepoint write_codepoint) {
    if (const auto* bytes = std::get_if<std::string>(&text)) {
        for (const auto ch : *bytes) {
            write_codepoint(static_cast<u8>(ch));
        }
        return;
    }
    if (const auto* codepoints = std::get_if<std::vector<u32>>(&text)) {
        for (const auto ch : *codepoints) {
            write_codepoint(ch);
        }
        return;
    }
    if (const auto* codepoints = std::get_if<span<const u32>>(&text)) {
        for (const auto ch : *codepoints) {
            write_codepoint(ch);
        }
        return;
    }
    for (const auto chunk : std::get<GlkCodepointChunks>(text).chunks) {
        for (const auto ch : chunk) {
            write_codepoint(ch);
        }
    }
}

template <typename Host>
void glk_write_to_unicode_memory_stream(Machine& machine,
                                        GlkStreamRecord<Host>& record,
                                        const GlkUnicodeMemoryStream& stream,
                                        const GlkTextData& text) {
    if (stream.mode == 2) {
        fail("cannot write to read-only unicode memory stream");
    }
    const auto write_codepoint = [&](u32 ch) {
        if (record.pos < stream.len && stream.address != 0) {
            machine.memory.write32(stream.address + record.pos * 4, ch);
        }
        ++record.pos;
        ++record.write_count;
    };

    glk_write_unicode_text(text, write_codepoint);
}

template <typename Host>
GlkCallResult glk_write_to_stream_record(Host& host,
                                         Machine& machine,
                                         GlkStreamRecord<Host>& record,
                                         const GlkTextData& text,
                                         GlkStyle style) {
    if (auto* host_stream = std::get_if<GlkHostStream<Host>>(&record.backing)) {
        auto result = host.write(host_stream->stream, text, style);
        if (std::holds_alternative<GlkReturned>(result)) {
            record.write_count += glk_text_length(text);
        }
        return result;
    }
    if (auto* memory = std::get_if<GlkMemoryStream>(&record.backing)) {
        glk_write_to_memory_stream(machine, record, *memory, text);
        return glk_returned();
    }
    if (auto* memory = std::get_if<GlkUnicodeMemoryStream>(&record.backing)) {
        glk_write_to_unicode_memory_stream(machine, record, *memory, text);
        return glk_returned();
    }
    fail("invalid Glk stream record");
}

}  // namespace glupsk
