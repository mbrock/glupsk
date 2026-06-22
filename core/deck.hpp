#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <ranges>

namespace glupsk {

template <typename R, typename T>
concept slice_of =
  std::ranges::contiguous_range<R> && std::ranges::borrowed_range<R> &&
  requires(R& rack) {
    { std::ranges::data(rack) } -> std::convertible_to<T*>;
  };

// The minimal vault a deck rides on: a sized, indexed store of T. std::span
// models it, and so does a proxy that (de)serializes through some backing —
// e.g. a slice of VM memory, where each slot reads/writes big-endian bytes via
// read32/write32 and re-resolves the address each access (so it survives the
// memory vector reallocating). slice_of refines this with contiguity, which is
// what unlocks contiguous_prefix().
template <typename R, typename T>
concept deck_storage = requires(R& rack, const R& const_rack, std::size_t i, T v) {
    { std::size(const_rack) } -> std::convertible_to<std::size_t>;
    rack[i] = v;
    { static_cast<T>(const_rack[i]) } -> std::same_as<T>;
};

template <typename T, typename R = std::span<T>>
    requires deck_storage<R, T> && std::is_trivially_copyable_v<T>
class Deck {
  // hmm, i wonder if we could change this pos_t
  // to some like std::ranges::range_difference_t<R>
  //
  // and change lo_ and hi_ to std::ranges::iterator_t<R>
  //
  // and change the slice_of requirement to not require contiguous_range<R>
  //   but mere e.g. random_access_range<R>
  //
  // i forget why tho haha like "it would be cool" "maybe"
  using pos_t = std::ptrdiff_t;

  public:
    class Flip {
      public:
        explicit Flip(Deck& deck) : deck_(&deck) {}
        bool push_back(T value) { return deck_->push_front(value); }
        bool push_front(T value) { return deck_->push_back(value); }
        std::optional<T> pop_back() { return deck_->pop_front(); }
        std::optional<T> pop_front() { return deck_->pop_back(); }
        std::size_t size() const { return deck_->size(); }
        bool empty() const { return deck_->empty(); }
        Deck& flip() const { return *deck_; }

      private:
        Deck* deck_;
    };

    Deck() = default;
    explicit Deck(R storage) : storage_(storage) {}

    std::size_t capacity() const { return std::size(storage_); }
    std::size_t size() const { return static_cast<std::size_t>(hi_ - lo_); }
    std::size_t unused() const { return capacity() - size(); }
    bool empty() const { return hi_ == lo_; }
    bool full() const { return size() == capacity(); }

    // Back end: produce, and LIFO pop.
    bool push_back(T value) {
        if (full()) {
            return false;
        }
        storage_[index(hi_)] = value;
        ++hi_;
        return true;
    }

    std::optional<T> pop_back() {
        if (empty()) {
            return std::nullopt;
        }
        --hi_;
        return static_cast<T>(storage_[index(hi_)]);
    }

    // Front end: FIFO consume, and prepend.
    std::optional<T> pop_front() {
        if (empty()) {
            return std::nullopt;
        }
        const T value = storage_[index(lo_)];
        ++lo_;
        return value;
    }

    bool push_front(T value) {
        if (full()) {
            return false;
        }
        --lo_;
        storage_[index(lo_)] = value;
        return true;
    }

    // Returns how many values could be pushed.
    std::size_t push_back(span<const T> values) {
        auto pushed = std::size_t{0};
        for (const auto& value : values) {
            if (!push_back(value)) {
                break;
            }
            ++pushed;
        }
        return pushed;
    }

    // Only contiguous vaults can hand out a span over their live region; a
    // proxy vault (VM memory) deliberately lacks this.
    std::span<T> contiguous_prefix() const
        requires slice_of<R, T>
    {
        if (empty()) return std::span<T>();
        const auto begin = index(lo_);
        const auto run = std::min(size(), capacity() - begin);
        return std::span(storage_).subspan(begin, run);
    }

    void consume(std::size_t count) {
        lo_ += static_cast<pos_t>(std::min(count, size()));
    }

    void clear() { lo_ = hi_ = 0; }

    Flip flip() { return Flip{*this}; }

  private:
    std::size_t index(pos_t position) const {
        const auto modulus = static_cast<pos_t>(capacity());
        auto wrapped = position % modulus;
        if (wrapped < 0) {
            wrapped += modulus;
        }
        return static_cast<std::size_t>(wrapped);
    }

    R storage_;
    pos_t lo_ = 0;
    pos_t hi_ = 0;
};

template <typename T, typename Storage = std::span<T>>
class Ring : private Deck<T, Storage> {
    using Base = Deck<T, Storage>;

  public:
    using Base::Base;
    using Base::capacity;
    using Base::clear;
    using Base::consume;
    using Base::empty;
    using Base::full;
    using Base::contiguous_prefix;
    using Base::size;
    using Base::unused;

    bool push(T value) { return Base::push_back(value); }
    std::size_t push(span<const T> values) { return Base::push_back(values); }
};

// infer deck value type from backing array value type
template <typename T, std::size_t Extent>
Deck(std::array<T, Extent>& s) -> Deck<T, std::span<T, Extent>>;

template <typename T, std::size_t Extent>
Ring(std::array<T, Extent>& s) -> Ring<T, std::span<T, Extent>>;


}  // namespace glupsk
