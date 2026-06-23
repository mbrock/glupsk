#pragma once

#include <compare>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>

namespace glupsk::range {

template <class T>
concept random_access = std::ranges::random_access_range<T>;

template <class T> using Value = std::ranges::range_value_t<T>;
template <class T> using Reference = std::ranges::range_reference_t<T>;
template <class T> using Size = std::ranges::range_size_t<T>;
template <class T> using Offset = std::ranges::range_difference_t<T>;

using std::ranges::size;

template <class R, class T>
concept mutable_slice_of =
    random_access<R> && std::same_as<Value<R>, std::remove_cvref_t<T>> &&
    std::same_as<Reference<R>, T&>;

template <random_access R>
struct CursorRangeAccess {
    using difference_type = Offset<R>;
    using value_type = Value<R>;
    using reference = Reference<R>;

    static reference read(R& root, difference_type index) {
        return std::ranges::begin(root)[index];
    }
};

template <typename Root, typename Access = CursorRangeAccess<Root>>
class Cursor {
  public:
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;

    using difference_type = typename Access::difference_type;
    using value_type = typename Access::value_type;
    using reference = typename Access::reference;

    Cursor() = default;

    explicit Cursor(Root& root, difference_type index = 0)
        : root_(std::addressof(root)), index_(index) {}

    reference operator*() const { return Access::read(*root_, index_); }

    reference operator[](difference_type n) const { return *(*this + n); }

    Cursor& operator++() {
        ++index_;
        return *this;
    }

    Cursor operator++(int) {
        auto old = *this;
        ++*this;
        return old;
    }

    Cursor& operator--() {
        --index_;
        return *this;
    }

    Cursor operator--(int) {
        auto old = *this;
        --*this;
        return old;
    }

    Cursor& operator+=(difference_type n) {
        index_ += n;
        return *this;
    }

    Cursor& operator-=(difference_type n) {
        index_ -= n;
        return *this;
    }

    friend Cursor operator+(Cursor it, difference_type n) {
        it += n;
        return it;
    }

    friend Cursor operator+(difference_type n, Cursor it) {
        it += n;
        return it;
    }

    friend Cursor operator-(Cursor it, difference_type n) {
        it -= n;
        return it;
    }

    friend difference_type operator-(const Cursor& a, const Cursor& b) {
        return a.index_ - b.index_;
    }

    friend bool operator==(const Cursor& a, const Cursor& b) {
        return a.root_ == b.root_ && a.index_ == b.index_;
    }

    friend auto operator<=>(const Cursor& a, const Cursor& b) {
        return a.index_ <=> b.index_;
    }

  private:
    Root* root_ = nullptr;
    difference_type index_ = 0;
};

template <random_access R>
using RootedIterator = Cursor<R>;

template <random_access R>
auto rooted_subrange(R& r, Offset<R> offset, Size<R> size) {
    return std::ranges::subrange(
        RootedIterator<R>{r, offset},
        RootedIterator<R>{r, offset + static_cast<Offset<R>>(size)});
}

}  // namespace glupsk::range
