#pragma once

#include "core/types.hpp"

#include <cstddef>

namespace glupsk {

inline u32 read_u32_be(span<const u8> bytes, std::size_t offset) {
    return (u32{bytes[offset]} << 24) |
           (u32{bytes[offset + 1]} << 16) |
           (u32{bytes[offset + 2]} << 8) |
           u32{bytes[offset + 3]};
}

inline void write_u32_be(span<u8> bytes, std::size_t offset, u32 value) {
    bytes[offset] = static_cast<u8>((value >> 24) & 0xff);
    bytes[offset + 1] = static_cast<u8>((value >> 16) & 0xff);
    bytes[offset + 2] = static_cast<u8>((value >> 8) & 0xff);
    bytes[offset + 3] = static_cast<u8>(value & 0xff);
}

}  // namespace glupsk
