#pragma once

#include "core/ranges.hpp"
#include "core/types.hpp"

#include <bit>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <utility>

namespace glupsk::word {

// Fixed-width unsigned integers that can be assembled from bytes without
// depending on host alignment, object representation, or native byte order.
template <typename T>
concept unsigned_word =
    std::unsigned_integral<T> && !std::same_as<T, bool>;

template <std::endian Order>
consteval std::endian resolved_order() {
    if constexpr (Order == std::endian::native) {
        return std::endian::native;
    } else {
        return Order;
    }
}

template <unsigned_word T>
constexpr T byteswap_if_needed(T value, std::endian order) {
    if constexpr (sizeof(T) == 1) {
        return value;
    }
    if (order == std::endian::native) {
        return value;
    }
    return std::byteswap(value);
}

template <unsigned_word T>
constexpr T load_native(span<const u8> bytes, std::size_t offset) {
    auto value = T{0};
    if constexpr (std::endian::native == std::endian::little) {
        for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
            value |= static_cast<T>(T{bytes[offset + index]} << (8 * index));
        }
    } else {
        for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
            value = static_cast<T>((value << 8) | T{bytes[offset + index]});
        }
    }
    return value;
}

template <std::endian Order, unsigned_word T>
constexpr T load(span<const u8> bytes, std::size_t offset) {
    return byteswap_if_needed<T>(load_native<T>(bytes, offset),
                                 resolved_order<Order>());
}

template <unsigned_word T>
constexpr void store_native(span<u8> bytes, std::size_t offset, T value) {
    if constexpr (std::endian::native == std::endian::little) {
        for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
            bytes[offset + index] = static_cast<u8>((value >> (8 * index)) & T{0xff});
        }
    } else {
        for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
            const auto shift = 8 * (sizeof(T) - index - 1);
            bytes[offset + index] = static_cast<u8>((value >> shift) & T{0xff});
        }
    }
}

template <std::endian Order, unsigned_word T>
constexpr void store(span<u8> bytes, std::size_t offset, T value) {
    store_native<T>(bytes, offset,
                    byteswap_if_needed<T>(value, resolved_order<Order>()));
}

template <unsigned_word T>
constexpr T load_be(span<const u8> bytes, std::size_t offset) {
    return load<std::endian::big, T>(bytes, offset);
}

template <unsigned_word T>
constexpr T load_le(span<const u8> bytes, std::size_t offset) {
    return load<std::endian::little, T>(bytes, offset);
}

template <unsigned_word T>
constexpr void store_be(span<u8> bytes, std::size_t offset, T value) {
    store<std::endian::big, T>(bytes, offset, value);
}

template <unsigned_word T>
constexpr void store_le(span<u8> bytes, std::size_t offset, T value) {
    store<std::endian::little, T>(bytes, offset, value);
}

template <typename R>
concept byte_range =
    range::random_access<R> && std::same_as<range::Value<R>, u8>;

template <typename R>
concept mutable_byte_range =
    byte_range<R> && requires(R r, range::Offset<R> i, u8 value) {
        { std::ranges::begin(r)[i] } -> std::convertible_to<u8>;
        std::ranges::begin(r)[i] = value;
    };

template <unsigned_word T, std::endian Order, mutable_byte_range Bytes>
class EndianRef {
  public:
    EndianRef() = default;
    EndianRef(Bytes& bytes, range::Offset<Bytes> offset)
        : bytes_(&bytes), offset_(offset) {}

    operator T() const {
        auto value = T{0};
        if constexpr (std::endian::native == std::endian::little) {
            for (auto index = range::Offset<Bytes>{0};
                 index < static_cast<range::Offset<Bytes>>(sizeof(T)); ++index) {
                value |= static_cast<T>(
                    T{std::ranges::begin(*bytes_)[offset_ + index]} <<
                    (8 * static_cast<std::size_t>(index)));
            }
        } else {
            for (auto index = range::Offset<Bytes>{0};
                 index < static_cast<range::Offset<Bytes>>(sizeof(T)); ++index) {
                value = static_cast<T>(
                    (value << 8) | T{std::ranges::begin(*bytes_)[offset_ + index]});
            }
        }
        return byteswap_if_needed<T>(value, resolved_order<Order>());
    }

    EndianRef& operator=(T value) {
        value = byteswap_if_needed<T>(value, resolved_order<Order>());
        for (auto index = range::Offset<Bytes>{0};
             index < static_cast<range::Offset<Bytes>>(sizeof(T)); ++index) {
            if constexpr (std::endian::native == std::endian::little) {
                std::ranges::begin(*bytes_)[offset_ + index] =
                    static_cast<u8>((value >> (8 * index)) & T{0xff});
            } else {
                const auto shift =
                    8 * (sizeof(T) - static_cast<std::size_t>(index) - 1);
                std::ranges::begin(*bytes_)[offset_ + index] =
                    static_cast<u8>((value >> shift) & T{0xff});
            }
        }
        return *this;
    }

    EndianRef& operator=(const EndianRef& other) {
        return *this = static_cast<T>(other);
    }

  private:
    Bytes* bytes_ = nullptr;
    range::Offset<Bytes> offset_ = 0;
};

template <unsigned_word T,
          std::endian Order,
          mutable_byte_range Bytes = std::span<u8>>
class EndianWords;

template <typename Words>
struct EndianAccess {
    using difference_type = typename Words::difference_type;
    using value_type = typename Words::value_type;
    using reference = typename Words::reference;

    static reference read(Words& words, difference_type index) {
        return words[index];
    }
};

// A proxy-backed mutable range of T-sized endian cells over raw bytes.
// Dereferencing does not produce a T&; it reassembles or rewrites bytes.
template <unsigned_word T, std::endian Order, mutable_byte_range Bytes>
class EndianWords {
  public:
    using value_type = T;
    using reference = EndianRef<T, Order, Bytes>;
    using iterator = range::Cursor<EndianWords, EndianAccess<EndianWords>>;
    using size_type = std::size_t;
    using difference_type = range::Offset<Bytes>;

    EndianWords() = default;
    EndianWords(Bytes bytes, difference_type offset, std::size_t count)
        : bytes_(bytes), offset_(offset), count_(count) {}

    [[nodiscard]] std::size_t size() const { return count_; }
    [[nodiscard]] bool empty() const { return count_ == 0; }

    reference operator[](difference_type index) const {
        return {bytes_, offset_ + index * static_cast<difference_type>(sizeof(T))};
    }

    iterator begin() { return iterator{*this, 0}; }
    iterator end() {
        return iterator{*this, static_cast<difference_type>(count_)};
    }

  private:
    mutable Bytes bytes_{};
    difference_type offset_ = 0;
    std::size_t count_ = 0;
};

template <unsigned_word T, std::endian Order, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto endian_words(Bytes&& bytes,
                  range::Offset<std::views::all_t<Bytes>> offset,
                  std::size_t count) {
    return EndianWords<T, Order, std::views::all_t<Bytes>>{
        std::views::all(std::forward<Bytes>(bytes)), offset, count};
}

template <unsigned_word T, std::endian Order, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto endian_words(Bytes&& bytes) {
    auto view = std::views::all(std::forward<Bytes>(bytes));
    const auto count = static_cast<std::size_t>(
        std::ranges::distance(view) / static_cast<range::Offset<decltype(view)>>(sizeof(T)));
    return EndianWords<T, Order, decltype(view)>{view, 0, count};
}

template <unsigned_word T, mutable_byte_range Bytes = std::span<u8>>
using BigEndianRef = EndianRef<T, std::endian::big, Bytes>;

template <unsigned_word T, mutable_byte_range Bytes = std::span<u8>>
using LittleEndianRef = EndianRef<T, std::endian::little, Bytes>;

template <unsigned_word T, mutable_byte_range Bytes = std::span<u8>>
using BigEndianWords = EndianWords<T, std::endian::big, Bytes>;

template <unsigned_word T, mutable_byte_range Bytes = std::span<u8>>
using LittleEndianWords = EndianWords<T, std::endian::little, Bytes>;

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto big_endian_words(Bytes&& bytes,
                      range::Offset<std::views::all_t<Bytes>> offset,
                      std::size_t count) {
    return endian_words<T, std::endian::big>(
        std::forward<Bytes>(bytes), offset, count);
}

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto big_endian_words(Bytes&& bytes) {
    return endian_words<T, std::endian::big>(std::forward<Bytes>(bytes));
}

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto little_endian_words(Bytes&& bytes,
                         range::Offset<std::views::all_t<Bytes>> offset,
                         std::size_t count) {
    return endian_words<T, std::endian::little>(
        std::forward<Bytes>(bytes), offset, count);
}

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto little_endian_words(Bytes&& bytes) {
    return endian_words<T, std::endian::little>(std::forward<Bytes>(bytes));
}

}  // namespace glupsk::word
