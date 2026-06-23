#pragma once

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

template <typename R>
    requires random_access<R>
class RootedIterator {
  public:
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;

    using difference_type = Offset<R>;
    using value_type = Value<R>;
    using reference = Reference<R>;

    RootedIterator() = default;

    explicit RootedIterator(R& home, difference_type base = 0)
        : range_(std::addressof(home)), offset_(base) {}

    reference operator*() const { return std::ranges::begin(*range_)[offset_]; }

    reference operator[](difference_type n) const { return *(*this + n); }

    RootedIterator& operator++() {
        ++offset_;
        return *this;
    }

    RootedIterator operator++(int) {
        auto old = *this;
        ++*this;
        return old;
    }

    RootedIterator& operator--() {
        --offset_;
        return *this;
    }

    RootedIterator operator--(int) {
        auto old = *this;
        --*this;
        return old;
    }

    RootedIterator& operator+=(difference_type n) {
        offset_ += n;
        return *this;
    }

    RootedIterator& operator-=(difference_type n) {
        offset_ -= n;
        return *this;
    }

    friend RootedIterator operator+(RootedIterator it, difference_type n) {
        it += n;
        return it;
    }

    friend RootedIterator operator+(difference_type n, RootedIterator it) {
        it += n;
        return it;
    }

    friend RootedIterator operator-(RootedIterator it, difference_type n) {
        it -= n;
        return it;
    }

    friend difference_type operator-(const RootedIterator& a,
                                     const RootedIterator& b) {
        return a.offset_ - b.offset_;
    }

    friend bool operator==(const RootedIterator& a,
                           const RootedIterator& b) {
        return a.range_ == b.range_ && a.offset_ == b.offset_;
    }

    friend auto operator<=>(const RootedIterator& a,
                            const RootedIterator& b) {
        return a.offset_ <=> b.offset_;
    }

  private:
    R* range_ = nullptr;
    difference_type offset_ = 0;
};

template <random_access R>
auto rooted_subrange(R& r, Offset<R> offset, Size<R> size) {
    return std::ranges::subrange(
        RootedIterator<R>{r, offset},
        RootedIterator<R>{r, offset + static_cast<Offset<R>>(size)});
}

}  // namespace glupsk::range
