#pragma once

#include "core/types.hpp"
#include "core/word.hpp"

#include <cstddef>

namespace glupsk {

inline u32 read_u32_be(span<const u8> bytes, std::size_t offset) {
    return word::load_be<u32>(bytes, offset);
}

inline u16 read_u16_be(span<const u8> bytes, std::size_t offset) {
    return word::load_be<u16>(bytes, offset);
}

inline void write_u32_be(span<u8> bytes, std::size_t offset, u32 value) {
    word::store_be<u32>(bytes, offset, value);
}

inline void write_u16_be(span<u8> bytes, std::size_t offset, u16 value) {
    word::store_be<u16>(bytes, offset, value);
}

}  // namespace glupsk
