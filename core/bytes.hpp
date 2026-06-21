#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace glupsk {

inline std::uint32_t read_u32_be(const std::vector<std::uint8_t>& bytes,
                                 std::size_t offset) {
    return (std::uint32_t{bytes[offset]} << 24) |
           (std::uint32_t{bytes[offset + 1]} << 16) |
           (std::uint32_t{bytes[offset + 2]} << 8) |
           std::uint32_t{bytes[offset + 3]};
}

inline void write_u32_be(std::vector<std::uint8_t>& bytes,
                         std::size_t offset,
                         std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>((value >> 24) & 0xff);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    bytes[offset + 3] = static_cast<std::uint8_t>(value & 0xff);
}

}  // namespace glupsk
