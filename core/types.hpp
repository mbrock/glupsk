#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace glupsk {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

template <typename T>
using span = std::span<T>;

using Bytes = std::vector<u8>;

}  // namespace glupsk
