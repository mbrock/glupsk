#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace glupsk {

using u8 = std::uint8_t;

template <typename T>
using span = std::span<T>;

using Bytes = std::vector<u8>;

}  // namespace glupsk
