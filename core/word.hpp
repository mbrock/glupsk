#pragma once

#include "core/types.hpp"

#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>

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

template <unsigned_word T>
class BigEndianRef {
  public:
    BigEndianRef() = default;
    BigEndianRef(span<u8> bytes, std::size_t offset)
        : bytes_(bytes), offset_(offset) {}

    operator T() const {
        return load_be<T>(bytes_, offset_);
    }

    BigEndianRef& operator=(T value) {
        store_be<T>(bytes_, offset_, value);
        return *this;
    }

    BigEndianRef& operator=(const BigEndianRef& other) {
        return *this = static_cast<T>(other);
    }

  private:
    span<u8> bytes_{};
    std::size_t offset_ = 0;
};

template <unsigned_word T>
class BigEndianWords;

// A proxy-backed mutable range of T-sized big-endian cells over raw bytes.
// Dereferencing does not produce a T&; it reassembles or rewrites bytes.
template <unsigned_word T>
class BigEndianIterator {
  public:
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = BigEndianRef<T>;

    BigEndianIterator() = default;
    BigEndianIterator(const BigEndianWords<T>& words, difference_type index)
        : words_(&words), index_(index) {}

    reference operator*() const;
    reference operator[](difference_type n) const { return *(*this + n); }

    BigEndianIterator& operator++() {
        ++index_;
        return *this;
    }

    BigEndianIterator operator++(int) {
        auto old = *this;
        ++*this;
        return old;
    }

    BigEndianIterator& operator--() {
        --index_;
        return *this;
    }

    BigEndianIterator operator--(int) {
        auto old = *this;
        --*this;
        return old;
    }

    BigEndianIterator& operator+=(difference_type n) {
        index_ += n;
        return *this;
    }

    BigEndianIterator& operator-=(difference_type n) {
        index_ -= n;
        return *this;
    }

    friend BigEndianIterator operator+(BigEndianIterator it,
                                       difference_type n) {
        it += n;
        return it;
    }

    friend BigEndianIterator operator+(difference_type n,
                                       BigEndianIterator it) {
        it += n;
        return it;
    }

    friend BigEndianIterator operator-(BigEndianIterator it,
                                       difference_type n) {
        it -= n;
        return it;
    }

    friend difference_type operator-(const BigEndianIterator& a,
                                     const BigEndianIterator& b) {
        return a.index_ - b.index_;
    }

    friend bool operator==(const BigEndianIterator& a,
                           const BigEndianIterator& b) {
        return a.words_ == b.words_ && a.index_ == b.index_;
    }

    friend auto operator<=>(const BigEndianIterator& a,
                            const BigEndianIterator& b) {
        return a.index_ <=> b.index_;
    }

  private:
    const BigEndianWords<T>* words_ = nullptr;
    difference_type index_ = 0;
};

template <unsigned_word T>
class BigEndianWords {
  public:
    using value_type = T;
    using reference = BigEndianRef<T>;
    using iterator = BigEndianIterator<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    BigEndianWords() = default;
    BigEndianWords(span<u8> bytes, std::size_t offset, std::size_t count)
        : bytes_(bytes), offset_(offset), count_(count) {}

    [[nodiscard]] std::size_t size() const { return count_; }
    [[nodiscard]] bool empty() const { return count_ == 0; }

    reference operator[](std::size_t index) const {
        return {bytes_, offset_ + index * sizeof(T)};
    }

    iterator begin() const { return {*this, 0}; }
    iterator end() const {
        return {*this, static_cast<difference_type>(count_)};
    }

  private:
    span<u8> bytes_{};
    std::size_t offset_ = 0;
    std::size_t count_ = 0;
};

template <unsigned_word T>
BigEndianRef<T> BigEndianIterator<T>::operator*() const {
    return (*words_)[static_cast<std::size_t>(index_)];
}

template <unsigned_word T>
BigEndianWords<T> big_endian_words(span<u8> bytes,
                                   std::size_t offset,
                                   std::size_t count) {
    return {bytes, offset, count};
}

template <unsigned_word T>
BigEndianWords<T> big_endian_words(span<u8> bytes) {
    return {bytes, 0, bytes.size() / sizeof(T)};
}

}  // namespace glupsk::word
