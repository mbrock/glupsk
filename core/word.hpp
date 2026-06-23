#pragma once

#include "core/ranges.hpp"
#include "core/types.hpp"

#include <concepts>
#include <cstddef>
#include <ranges>
#include <utility>

namespace glupsk::word {

// Fixed-width unsigned integers that can be assembled from bytes without
// depending on host alignment, object representation, or native byte order.
template <typename T>
concept unsigned_word =
    std::unsigned_integral<T> && (sizeof(T) == 1 || sizeof(T) == 2 ||
                                 sizeof(T) == 4 || sizeof(T) == 8);

template <unsigned_word T>
constexpr T load_be(span<const u8> bytes, std::size_t offset) {
    auto value = T{0};
    for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
        value = static_cast<T>((value << 8) | T{bytes[offset + index]});
    }
    return value;
}

template <unsigned_word T>
constexpr void store_be(span<u8> bytes, std::size_t offset, T value) {
    for (auto index = std::size_t{0}; index < sizeof(T); ++index) {
        const auto shift = 8 * (sizeof(T) - index - 1);
        bytes[offset + index] = static_cast<u8>((value >> shift) & T{0xff});
    }
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

template <unsigned_word T, mutable_byte_range Bytes>
class BigEndianRef {
  public:
    BigEndianRef() = default;
    BigEndianRef(Bytes& bytes, range::Offset<Bytes> offset)
        : bytes_(&bytes), offset_(offset) {}

    operator T() const {
        auto value = T{0};
        for (auto index = range::Offset<Bytes>{0};
             index < static_cast<range::Offset<Bytes>>(sizeof(T)); ++index) {
            value = static_cast<T>(
                (value << 8) | T{std::ranges::begin(*bytes_)[offset_ + index]});
        }
        return value;
    }

    BigEndianRef& operator=(T value) {
        for (auto index = range::Offset<Bytes>{0};
             index < static_cast<range::Offset<Bytes>>(sizeof(T)); ++index) {
            const auto shift = 8 * (sizeof(T) - static_cast<std::size_t>(index) - 1);
            std::ranges::begin(*bytes_)[offset_ + index] =
                static_cast<u8>((value >> shift) & T{0xff});
        }
        return *this;
    }

    BigEndianRef& operator=(const BigEndianRef& other) {
        return *this = static_cast<T>(other);
    }

  private:
    Bytes* bytes_ = nullptr;
    range::Offset<Bytes> offset_ = 0;
};

template <unsigned_word T, mutable_byte_range Bytes = std::span<u8>>
class BigEndianWords;

template <typename Words>
struct BigEndianAccess {
    using difference_type = typename Words::difference_type;
    using value_type = typename Words::value_type;
    using reference = typename Words::reference;

    static reference read(Words& words, difference_type index) {
        return words[index];
    }
};

// A proxy-backed mutable range of T-sized big-endian cells over raw bytes.
// Dereferencing does not produce a T&; it reassembles or rewrites bytes.
template <unsigned_word T, mutable_byte_range Bytes>
class BigEndianWords {
  public:
    using value_type = T;
    using reference = BigEndianRef<T, Bytes>;
    using iterator = range::Cursor<BigEndianWords, BigEndianAccess<BigEndianWords>>;
    using size_type = std::size_t;
    using difference_type = range::Offset<Bytes>;

    BigEndianWords() = default;
    BigEndianWords(Bytes bytes, difference_type offset, std::size_t count)
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

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto big_endian_words(Bytes&& bytes,
                      range::Offset<std::views::all_t<Bytes>> offset,
                      std::size_t count) {
    return BigEndianWords<T, std::views::all_t<Bytes>>{
        std::views::all(std::forward<Bytes>(bytes)), offset, count};
}

template <unsigned_word T, std::ranges::viewable_range Bytes>
    requires mutable_byte_range<std::views::all_t<Bytes>>
auto big_endian_words(Bytes&& bytes) {
    auto view = std::views::all(std::forward<Bytes>(bytes));
    const auto count = static_cast<std::size_t>(
        std::ranges::distance(view) / static_cast<range::Offset<decltype(view)>>(sizeof(T)));
    return BigEndianWords<T, decltype(view)>{view, 0, count};
}

}  // namespace glupsk::word
