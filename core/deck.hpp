#pragma once

#include "core/ranges.hpp"
#include "core/types.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <ranges>
#include <type_traits>

namespace glupsk {
template <class Z> constexpr auto remainder(Z a, Z b) {
  auto r = a % b;
  if (r < 0)
    r += b;
  return r;
}

template <typename R> auto normalize_offset(range::Offset<R> i, R &r) {
  return remainder(i, std::ranges::distance(r));
}

template <typename T, typename Storage = std::span<T>>
  requires range::mutable_slice_of<Storage, T> && std::is_trivially_copyable_v<T>
class Deck {
  using Offset = range::Offset<Storage>;

public:
  class Flip {
  public:
    explicit Flip(Deck &deck) : deck_(&deck) {}

    bool push_back(T value) { return deck_->push_front(value); }
    bool push_front(T value) { return deck_->push_back(value); }
    std::optional<T> pop_back() { return deck_->pop_front(); }
    std::optional<T> pop_front() { return deck_->pop_back(); }
    [[nodiscard]] std::size_t size() const { return deck_->size(); }
    [[nodiscard]] bool empty() const { return deck_->empty(); }
    Deck &flip() const { return *deck_; }

  private:
    Deck *deck_;
  };

  Deck() = default;

  explicit Deck(Storage storage) : storage_(storage) {}

  T &at_logical(Offset p) {
    assert(sane());
    return std::ranges::begin(storage_)[normalize_offset(p, storage_)];
  }

  [[nodiscard]] std::size_t capacity() const { return std::size(storage_); }

  [[nodiscard]] std::size_t size() const {
    assert(hi_ >= lo_);
    return static_cast<std::size_t>(hi_ - lo_);
  }

  [[nodiscard]] std::size_t unused() const { return capacity() - size(); }
  [[nodiscard]] bool empty() const { return hi_ == lo_; }
  [[nodiscard]] bool full() const { return size() == capacity(); }

  [[nodiscard]] bool push_back(T value) {
    if (full())
      return false;
    at_logical(hi_) = value;
    ++hi_;
    return true;
  }

  std::optional<T> pop_back() {
    if (empty())
      return std::nullopt;
    --hi_;
    return at_logical(hi_);
  }

  std::optional<T> pop_front() {
    if (empty())
      return std::nullopt;
    T value = at_logical(lo_);
    ++lo_;
    return value;
  }

  [[nodiscard]] bool push_front(T value) {
    if (full())
      return false;
    --lo_;
    at_logical(lo_) = value;
    return true;
  }

  // Returns how many values could be pushed.
  [[nodiscard]] std::size_t push_back(span<const T> values) {
    auto pushed = std::size_t{0};
    for (const auto &value : values) {
      if (!push_back(value)) {
        break;
      }
      ++pushed;
    }
    return pushed;
  }

  std::pair<std::span<T>, std::span<T>> contiguous_parts() const
    requires std::ranges::contiguous_range<Storage>
  {
    auto s = std::span<T>(storage_);

    if (empty()) {
      return {};
    }

    assert(sane());

    const auto cap = capacity();
    const auto begin =
        static_cast<std::size_t>(normalize_offset(lo_, storage_));
    const auto n = size();

    const auto first_n = std::min(n, cap - begin);

    return {s.subspan(begin, first_n), s.subspan(0, n - first_n)};
  }

  std::span<T> contiguous_prefix() const
    requires std::ranges::contiguous_range<Storage>
  {
    return contiguous_parts().first;
  }

  void consume(std::size_t count) {
    lo_ += static_cast<Offset>(std::min(count, size()));
  }

  void clear() {
    lo_ = 0;
    hi_ = 0;
  }

  Flip flip() { return Flip{*this}; }

  [[nodiscard]] bool sane() const { return lo_ <= hi_ && size() <= capacity(); }

private:
  Storage storage_;
  Offset lo_{0};
  Offset hi_{0};
};

template <typename T, typename Storage = std::span<T>>
class Ring : private Deck<T, Storage> {
  using Base = Deck<T, Storage>;

public:
  using Base::Base;
  using Base::capacity;
  using Base::clear;
  using Base::consume;
  using Base::contiguous_prefix;
  using Base::empty;
  using Base::full;
  using Base::size;
  using Base::unused;

  bool push(T value) { return Base::push_back(value); }
  std::size_t push(span<const T> values) { return Base::push_back(values); }
};

// infer deck value type from backing array value type
template <typename T, std::size_t Extent>
Deck(std::array<T, Extent> &s) -> Deck<T, std::span<T, Extent>>;

template <typename T, std::size_t Extent>
Ring(std::array<T, Extent> &s) -> Ring<T, std::span<T, Extent>>;
} // namespace glupsk
