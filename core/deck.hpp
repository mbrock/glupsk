#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>

namespace glupsk {

template <class Z>
constexpr auto remainder(Z a, Z b) {
    auto r = a % b;
    if (r < 0) r += b;
    return r;
}

namespace range {
template <class T>
concept random_access = std::ranges::random_access_range<T>;

template <class T> using Value = std::ranges::range_value_t<T>;
template <class T> using Reference = std::ranges::range_reference_t<T>;
template <class T> using Size = std::ranges::range_size_t<T>;
template <class T> using Diff = std::ranges::range_difference_t<T>;

using std::ranges::size;
using std::ranges::distance;

template <class R, class T>
concept mutable_slice_of =
    random_access<R> && std::same_as<Value<R>, std::remove_cvref_t<T>> &&
    std::same_as<Reference<R>, T &>;
} // namespace range

template <typename R> auto roll(range::Diff<R> i, R &r) {
    return remainder(i, range::distance(r));
}

template <std::ranges::random_access_range R> class Mark {
public:
  using iterator_concept = std::random_access_iterator_tag;
  using iterator_category = std::random_access_iterator_tag;

  using difference_type = range::Diff<R>;
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
auto indexing_subrange(R &range, range::Diff<R> base,
                       std::ranges::range_size_t<R> size) {
  using D = range::Diff<R>;

  return std::ranges::subrange(Mark<R>{range, base},
                               Mark<R>{range, base + static_cast<D>(size)});
}

/// this is a non-owning kind of deque adapter that works with
/// any random access range, i guess
///
/// it can be used as a ring buffer, see Ring below,
/// but also as a stack, for example
///
/// it is related to double entry bookkeeping (see ~/pacioli
/// for more details)
///
/// it's also inspired by zig post-writergate
///
/// i kind of want it to be the base layer for all kinds of
/// memory frobbling and swizzling and slicing and so on
/// in the system
///
/// in particular i am thinking about some way to use
/// a Deck<u8, R> as a Deck<u32> or something like that
/// because in other parts of the program ("Memory", "Stack", etc)
/// we do that (big endian) all over the place
///
/// we made an attempt earlier to make a `MemorySlab` but
/// it's commented out and should now be psosible to write
/// in a much nicer way, with the stuff we've laid the groundwork
/// for
template <typename T, typename R = std::span<T>>
  requires range::mutable_slice_of<R, T> && std::is_trivially_copyable_v<T>
class Deck {
  using diff_t = range::Diff<R>;

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

  T &at_logical(diff_t p) {
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

    return {s.subspan(begin, first_n), s.subspan(0, n - first_n)};
  }

  std::span<T> contiguous_prefix() const
    requires std::ranges::contiguous_range<R>
  {
    return contiguous_parts().first;
  }

  void consume(std::size_t count) {
    lo_ += static_cast<diff_t>(std::min(count, size()));
  }

  void clear() {
    lo_ = 0;
    hi_ = 0;
  }

  Flip flip() { return Flip{*this}; }

  bool sane() const { return lo_ <= hi_ && size() <= capacity(); }

private:
  R storage_;
  diff_t lo_{0};
  diff_t hi_{0};
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
