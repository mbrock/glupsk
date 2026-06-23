#include "core/glk_text.hpp"

#include <algorithm>

namespace glupsk {

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

u32 glk_text_length(const GlkTextData& text) {
    if (const auto* bytes = std::get_if<std::string>(&text)) {
        return static_cast<u32>(bytes->size());
    }
    if (const auto* codepoints = std::get_if<std::vector<u32>>(&text)) {
        return static_cast<u32>(codepoints->size());
    }
    if (const auto* codepoints = std::get_if<span<const u32>>(&text)) {
        return static_cast<u32>(codepoints->size());
    }
    u32 count = 0;
    for (const auto chunk : std::get<GlkCodepointChunks>(text).chunks) {
        count += static_cast<u32>(chunk.size());
    }
    return count;
}

bool glk_text_is_latin1(const GlkTextData& text) {
    return std::holds_alternative<std::string>(text);
}

GlkTextData glk_own_text(const GlkTextData& text) {
    if (const auto* bytes = std::get_if<std::string>(&text)) {
        return *bytes;
    }
    if (const auto* codepoints = std::get_if<std::vector<u32>>(&text)) {
        return *codepoints;
    }
    if (const auto* codepoints = std::get_if<span<const u32>>(&text)) {
        return std::vector<u32>{codepoints->begin(), codepoints->end()};
    }

    auto owned = std::vector<u32>{};
    owned.reserve(glk_text_length(text));
    for (const auto chunk : std::get<GlkCodepointChunks>(text).chunks) {
        owned.insert(owned.end(), chunk.begin(), chunk.end());
    }
    return owned;
}

GlkTextData glk_materialize_text(Machine& machine, const GlkText& text) {
    // Hosts receive semantic text, not raw VM pointers. Latin-1 stays compact
    // as bytes; Unicode paths carry codepoints.
    return std::visit(
        [&](const auto& value) -> GlkTextData {
            return glk_materialize_text(machine, value);
        },
        text);
}

GlkTextData glk_materialize_text(Machine&, const GlkTextChar& text) {
    if (text.encoding == GlkTextEncoding::unicode) {
        return std::vector<u32>{text.value};
    }
    return std::string{static_cast<char>(text.value & 0xffu)};
}

GlkTextData glk_materialize_text(Machine& machine, const GlkTextBuffer& text) {
    if (text.encoding == GlkTextEncoding::unicode) {
        auto codepoints = std::vector<u32>{};
        codepoints.reserve(text.length);
        for (u32 index = 0; index < text.length; ++index) {
            codepoints.push_back(machine.memory.read32(text.address + index * 4));
        }
        return codepoints;
    }

    auto bytes = std::string{};
    bytes.reserve(text.length);
    for (u32 index = 0; index < text.length; ++index) {
        bytes.push_back(static_cast<char>(machine.memory.read8(text.address + index)));
    }
    return bytes;
}

GlkTextData glk_materialize_text(Machine& machine, const GlkTextString& text) {
    if (text.encoding == GlkTextEncoding::unicode) {
        auto codepoints = std::vector<u32>{};
        auto address = text.address;
        while (true) {
            const auto ch = machine.memory.read32(address);
            address += 4;
            if (ch == 0) {
                return codepoints;
            }
            codepoints.push_back(ch);
        }
    }

    auto bytes = std::string{};
    auto address = text.address;
    while (true) {
        const auto ch = machine.memory.read8(address++);
        if (ch == 0) {
            return bytes;
        }
        bytes.push_back(static_cast<char>(ch));
    }
}

u32 glk_transform_unicode_buffer(Machine& machine,
                                 u32 address,
                                 u32 len,
                                 u32 numchars,
                                 u32 (*transform)(u32)) {
    const auto writable = std::min(len, numchars);
    for (u32 index = 0; index < writable; ++index) {
        const auto cell = address + index * 4;
        machine.memory.write32(cell, transform(machine.memory.read32(cell)));
    }
    return numchars;
}

u32 glk_title_case_unicode_buffer(Machine& machine,
                                  u32 address,
                                  u32 len,
                                  u32 numchars,
                                  bool lower_rest) {
    const auto writable = std::min(len, numchars);
    if (writable == 0) {
        return numchars;
    }

    machine.memory.write32(address,
                           glk_char_to_upper(machine.memory.read32(address)));
    if (lower_rest) {
        for (u32 index = 1; index < writable; ++index) {
            const auto cell = address + index * 4;
            machine.memory.write32(cell,
                                   glk_char_to_lower(machine.memory.read32(cell)));
        }
    }
    return numchars;
}

}  // namespace glupsk
