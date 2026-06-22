#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <ranges>

namespace glupsk {

// A Deck separates two concerns the way nxtui's farm does: the *bookkeeping*
// (two frontiers — the Pacioli account) lives in Deck; the *vault* (where slot
// i physically lives) lives in a Storage policy. Every Storage can store/load a
// value at a physical index and report its capacity. Contiguous storages also
// expose data(), which unlocks the span-returning readable() drain path; a
// non-contiguous vault (e.g. a VM-memory region reached via read32/write32)
// simply does not model ContiguousDeckStorage, so the type system forbids
// handing out a span over it.
template <typename S, typename T>
concept DeckStorage = requires(S& storage, const S& const_storage,
                               std::size_t index, T value) {
    { const_storage.capacity() } -> std::convertible_to<std::size_t>;
    storage.store(index, value);
    { const_storage.load(index) } -> std::convertible_to<T>;
};

template <typename S, typename T>
concept ContiguousDeckStorage =
    DeckStorage<S, T> && requires(S& storage, const S& const_storage) {
        { const_storage.data() } -> std::convertible_to<const T*>;
        { storage.data() } -> std::convertible_to<T*>;
    };

// Vault over caller-owned contiguous storage the deck does not own.
template <typename T>
class BorrowedStorage {
  public:
    BorrowedStorage() = default;
    BorrowedStorage(T* values, std::size_t capacity)
        : values_(values), capacity_(capacity) {}

    BorrowedStorage(std::ranges::contiguous_range auto& slab)
        : BorrowedStorage(slab.data(), slab.size()) {}

    std::size_t capacity() const { return capacity_; }
    void store(std::size_t index, T value) { values_[index] = value; }
    T load(std::size_t index) const { return values_[index]; }
    const T* data() const { return values_; }
    T* data() { return values_; }

  private:
    T* values_ = nullptr;
    std::size_t capacity_ = 0;
};

// Double-ended circular buffer for trivially-copyable values: the Pacioli
// *group*, not just the monoid.
//
// The state is two frontiers, lo_ (front) and hi_ (back), forming the T-account
// `(hi // lo)` (cf. module RingBuffer in pacioli/src/Pacioli.agda). They are
// *signed* because a frontier may run backward — pop_back retreats hi_, a read
// rewind retreats lo_ — which is exactly the negative posting the monoid cannot
// express and the group can. Everything is derived from their difference:
//
//   * size()   = hi_ - lo_                  -- Occupancy
//   * unused() = capacity() - size()        -- Free; size()+unused()==capacity()
//   * the safety guard is  lo_ <= hi_  and  hi_ - lo_ <= capacity()
//
// The two ends are symmetric, so flip() — swapping them — is the Pacioli
// involution `_⁻¹ᵀ`: flip().pop_front() == pop_back(), flip().push_back() ==
// push_front(). A FIFO is push_back + pop_front; a stack is push_back +
// pop_back (= push to the deck, pop from its flip). Frontiers mask by capacity
// and never reset; a zero-capacity deck accepts nothing.
template <typename T, typename Storage = BorrowedStorage<T>>
    requires DeckStorage<Storage, T>
class Deck {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Deck is intended for trivial value types");

    using pos_t = std::ptrdiff_t;

  public:
    // Non-owning view of a deck with its two ends swapped: the Pacioli inverse.
    class Flip {
      public:
        explicit Flip(Deck& deck) : deck_(&deck) {}
        bool push_back(T value) { return deck_->push_front(value); }
        bool push_front(T value) { return deck_->push_back(value); }
        std::optional<T> pop_back() { return deck_->pop_front(); }
        std::optional<T> pop_front() { return deck_->pop_back(); }
        std::size_t size() const { return deck_->size(); }
        bool empty() const { return deck_->empty(); }
        Deck& base() const { return *deck_; }

      private:
        Deck* deck_;
    };

    Deck() = default;
    explicit Deck(Storage storage) : storage_(std::move(storage)) {}

    std::size_t capacity() const { return storage_.capacity(); }
    std::size_t size() const { return static_cast<std::size_t>(hi_ - lo_); }
    std::size_t unused() const { return capacity() - size(); }
    bool empty() const { return hi_ == lo_; }
    bool full() const { return size() == capacity(); }

    // Back end: produce, and LIFO pop.
    bool push_back(T value) {
        if (full()) {
            return false;
        }
        storage_.store(index(hi_), value);
        ++hi_;
        return true;
    }

    std::optional<T> pop_back() {
        if (empty()) {
            return std::nullopt;
        }
        --hi_;
        return storage_.load(index(hi_));
    }

    // Front end: FIFO consume, and prepend.
    std::optional<T> pop_front() {
        if (empty()) {
            return std::nullopt;
        }
        const auto value = storage_.load(index(lo_));
        ++lo_;
        return value;
    }

    bool push_front(T value) {
        if (full()) {
            return false;
        }
        --lo_;
        storage_.store(index(lo_), value);
        return true;
    }

    // Push as many of `values` as fit at the back, in order. Returns the count.
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

    // First contiguous run from the front. May be shorter than size() when the
    // contents wrap; consume() it and call again to read the remainder.
    span<const T> readable() const
        requires ContiguousDeckStorage<Storage, T>
    {
        if (empty()) {
            return {};
        }
        const auto begin = index(lo_);
        const auto run = std::min(size(), capacity() - begin);
        return {storage_.data() + begin, run};
    }

    // Drop up to `count` values from the front.
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

    Storage storage_;
    pos_t lo_ = 0;  // front frontier
    pos_t hi_ = 0;  // back frontier
};

// The unipolar (FIFO) face of a Deck: produce at the back, consume from the
// front, nothing else. Restricting the deck to a single polarity is the monoid
// shadow of the group — see pacioli/src/Pacioli.agda. Owns its deck; used by
// stream buffers, which genuinely are FIFOs.
template <typename T, typename Storage = BorrowedStorage<T>>
class Ring : private Deck<T, Storage> {
    using Base = Deck<T, Storage>;

  public:
    using Base::Base;
    using Base::capacity;
    using Base::clear;
    using Base::consume;
    using Base::empty;
    using Base::full;
    using Base::readable;
    using Base::size;
    using Base::unused;

    bool push(T value) { return Base::push_back(value); }
    std::size_t push(span<const T> values) { return Base::push_back(values); }
};

}  // namespace glupsk
