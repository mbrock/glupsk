#pragma once

#include "core/types.hpp"

#include <cassert>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>

namespace glupsk {

template <typename R, typename T>
concept mutable_slice_of =
    std::ranges::random_access_range<R> &&
    std::same_as<std::ranges::range_value_t<R>, std::remove_cvref_t<T>> &&
    std::same_as<std::ranges::range_reference_t<R>, T &>;

template <typename R, typename T>
concept deck_storage = mutable_slice_of<R, T>;

template <typename R>
auto roll(std::ranges::range_difference_t<R> i, R& r) {
    using D = std::ranges::range_difference_t<R>;
    const auto m = static_cast<D>(std::ranges::size(r));
    assert(m > 0);

    auto ix = i % m;
    if (ix < 0)
        ix += m;

    return ix;
}

template <std::ranges::random_access_range R> class Mark {
public:
  using iterator_concept = std::random_access_iterator_tag;
  using iterator_category = std::random_access_iterator_tag;

  using difference_type = std::ranges::range_difference_t<R>;
  using value_type = std::ranges::range_value_t<R>;
  using reference = std::ranges::range_reference_t<R>;

  Mark() = default;

  Mark(R &home, difference_type base)
      : _home(std::addressof(home)), _base(base) {}

  reference operator*() const {
    return std::ranges::begin(*_home)[roll(_base, *_home)];
  }

  reference operator[](difference_type n) const { return *(*this + n); }

  Mark &operator++() {
    ++_base;
    return *this;
  }
  Mark operator++(int) {
    auto old = *this;
    ++*this;
    return old;
  }

  Mark &operator--() {
    --_base;
    return *this;
  }
  Mark operator--(int) {
    auto old = *this;
    --*this;
    return old;
  }

  Mark &operator+=(difference_type n) {
    _base += n;
    return *this;
  }

  Mark &operator-=(difference_type n) {
    _base -= n;
    return *this;
  }

  friend Mark operator+(Mark it, difference_type n) {
    it += n;
    return it;
  }

  friend Mark operator+(difference_type n, Mark it) {
    it += n;
    return it;
  }

  friend Mark operator-(Mark it, difference_type n) {
    it -= n;
    return it;
  }

  friend difference_type operator-(const Mark &a, const Mark &b) {
    return a._base - b._base;
  }

  friend bool operator==(const Mark &a, const Mark &b) {
    return a._home == b._home && a._base == b._base;
  }

  friend auto operator<=>(const Mark &a, const Mark &b) {
    return a._base <=> b._base;
  }

private:
  R *_home = nullptr;
  difference_type _base = 0;
};

static_assert(std::random_access_iterator<Mark<std::vector<int>>>);

template <std::ranges::random_access_range R>
auto indexing_subrange(R &range, std::ranges::range_difference_t<R> base,
                       std::ranges::range_size_t<R> size) {
  using D = std::ranges::range_difference_t<R>;

  return std::ranges::subrange(Mark<R>{range, base},
                               Mark<R>{range, base + static_cast<D>(size)});
}

template <typename T, typename R = std::span<T>>
  requires deck_storage<R, T> && std::is_trivially_copyable_v<T>
class Deck {
  using pos_t = std::ranges::range_difference_t<R>;

public:
  class Flip {
  public:
    explicit Flip(Deck &deck) : deck_(&deck) {}
    bool push_back(T value) { return deck_->push_front(value); }
    bool push_front(T value) { return deck_->push_back(value); }
    std::optional<T> pop_back() { return deck_->pop_front(); }
    std::optional<T> pop_front() { return deck_->pop_back(); }
    std::size_t size() const { return deck_->size(); }
    bool empty() const { return deck_->empty(); }
    Deck &flip() const { return *deck_; }

  private:
    Deck *deck_;
  };

  Deck() = default;
  explicit Deck(R storage) : storage_(storage) {}

  T &at_logical(pos_t p) {
      assert(sane());
    return std::ranges::begin(storage_)[roll(p, storage_)];
  }

  std::size_t capacity() const { return std::size(storage_); }

  std::size_t size() const {
      assert(hi_ >= lo_);
      return static_cast<std::size_t>(hi_ - lo_);
  }

  std::size_t unused() const { return capacity() - size(); }
  bool empty() const { return hi_ == lo_; }
  bool full() const { return size() == capacity(); }

  bool push_back(T value) {
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

  bool push_front(T value) {
    if (full())
      return false;
    --lo_;
    at_logical(lo_) = value;
    return true;
  }

  // Returns how many values could be pushed.
  std::size_t push_back(span<const T> values) {
    auto pushed = std::size_t{0};
    for (const auto &value : values) {
      if (!push_back(value)) {
        break;
      }
      ++pushed;
    }
    return pushed;
  }

  auto view() { return indexing_subrange(storage_, lo_, size()); }

  std::pair<std::span<T>, std::span<T>> contiguous_parts() const
      requires std::ranges::contiguous_range<R>
  {
      auto s = std::span<T>(storage_);

      if (empty()) {
          return {};
      }

      assert(hi_ >= lo_);
      assert(size() <= capacity());

      const auto cap = capacity();
      const auto begin = static_cast<std::size_t>(roll(lo_, storage_));
      const auto n = size();

      const auto first_n = std::min(n, cap - begin);

      return {
          s.subspan(begin, first_n),
          s.subspan(0, n - first_n)
      };
  }

  std::span<T> contiguous_prefix() const
    requires std::ranges::contiguous_range<R>
  {
    return contiguous_parts().first;
  }

  void consume(std::size_t count) {
      lo_ += static_cast<pos_t>(std::min(count, size()));
  }

  void clear() { lo_ = 0; hi_ = 0; }

  Flip flip() { return Flip{*this}; }

  bool sane() const {
      return lo_ <= hi_ && size() <= capacity();
  }

private:
  R storage_;
  pos_t lo_{0};
  pos_t hi_{0};
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
