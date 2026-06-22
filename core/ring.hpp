#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace glupsk {

// A Ring separates two concerns the way nxtui's farm does: the *bookkeeping*
// (two monotone frontiers — the Pacioli account) lives in Ring; the *vault*
// (where slot i physically lives) lives in a Storage policy. Every Storage can
// store/load a value at a physical index and report its capacity. Contiguous
// storages additionally expose data(), which unlocks the span-returning
// readable() drain path; a non-contiguous vault (e.g. a VM-memory region that
// must go through read32/write32) simply does not model ContiguousRingStorage,
// so the type system forbids handing out a span over it — the degenerate case
// made legible as a missing capability rather than a runtime trap.
template <typename S, typename T>
concept RingStorage = requires(S& storage, const S& const_storage,
                               std::size_t index, T value) {
    { const_storage.capacity() } -> std::convertible_to<std::size_t>;
    storage.store(index, value);
    { const_storage.load(index) } -> std::convertible_to<T>;
};

template <typename S, typename T>
concept ContiguousRingStorage =
    RingStorage<S, T> && requires(S& storage, const S& const_storage) {
        { const_storage.data() } -> std::convertible_to<const T*>;
        { storage.data() } -> std::convertible_to<T*>;
    };

// Vault that owns its values in a heap vector sized at construction.
template <typename T>
class OwnedStorage {
  public:
    OwnedStorage() = default;
    explicit OwnedStorage(std::size_t capacity) : values_(capacity) {}

    std::size_t capacity() const { return values_.size(); }
    void store(std::size_t index, T value) { values_[index] = value; }
    T load(std::size_t index) const { return values_[index]; }
    const T* data() const { return values_.data(); }
    T* data() { return values_.data(); }

  private:
    std::vector<T> values_;
};

// Vault over caller-owned contiguous storage the ring does not own.
template <typename T>
class BorrowedStorage {
  public:
    BorrowedStorage() = default;
    BorrowedStorage(T* values, std::size_t capacity)
        : values_(values), capacity_(capacity) {}

    std::size_t capacity() const { return capacity_; }
    void store(std::size_t index, T value) { values_[index] = value; }
    T load(std::size_t index) const { return values_[index]; }
    const T* data() const { return values_; }
    T* data() { return values_; }

  private:
    T* values_ = nullptr;
    std::size_t capacity_ = 0;
};

// Fixed-capacity circular buffer for trivially-copyable values.
//
// A refinement of `module RingBuffer` in pacioli/src/Pacioli.agda: the state
// is two monotonically increasing frontiers, head_ (write, total pushed) and
// tail_ (read, total consumed) — the T-account `(head // tail)` — with
// everything derived from their difference:
//
//   * size()   = head_ - tail_              -- Occupancy / debit-positive reading
//   * unused() = capacity() - size()        -- Free / credit-positive reading
//   * push()   posts (1 // 0); consume(n)   posts (0 // n)
//   * the safety invariant is the halfspace guard  tail_ <= head_ <= tail_ + N,
//     and  size() + unused() == capacity()  is the balance sheet.
//
// Frontiers mask by capacity (`% capacity()`) and never reset, sidestepping the
// classic full-vs-empty ambiguity. A zero-capacity ring is a valid "unbuffered"
// ring that accepts nothing. Reading is segment-at-a-time: readable() returns
// the first contiguous run, so a full drain loops readable()/consume() until
// empty (contiguous storages only).
template <typename T, typename Storage = OwnedStorage<T>>
    requires RingStorage<Storage, T>
class Ring {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Ring is intended for trivial value types");

  public:
    Ring() = default;
    explicit Ring(Storage storage) : storage_(std::move(storage)) {}
    explicit Ring(std::size_t capacity)
        requires std::constructible_from<Storage, std::size_t>
        : storage_(capacity) {}

    std::size_t capacity() const { return storage_.capacity(); }
    std::size_t size() const { return head_ - tail_; }
    std::size_t unused() const { return capacity() - size(); }
    bool empty() const { return head_ == tail_; }
    bool full() const { return size() == capacity(); }

    // Push one value. Returns false (and drops it) when the ring is full.
    bool push(T value) {
        if (full()) {
            return false;
        }
        storage_.store(head_ % capacity(), value);
        ++head_;
        return true;
    }

    // Push as many of `values` as fit, in order. Returns the count accepted.
    std::size_t push(span<const T> values) {
        auto pushed = std::size_t{0};
        for (const auto& value : values) {
            if (!push(value)) {
                break;
            }
            ++pushed;
        }
        return pushed;
    }

    // First contiguous readable run. May be shorter than size() when the
    // contents wrap; consume() it and call again to read the remainder.
    span<const T> readable() const
        requires ContiguousRingStorage<Storage, T>
    {
        if (empty()) {
            return {};
        }
        const auto begin = tail_ % capacity();
        const auto run = std::min(size(), capacity() - begin);
        return {storage_.data() + begin, run};
    }

    // Drop up to `count` values from the front.
    void consume(std::size_t count) {
        tail_ += std::min(count, size());
    }

    void clear() { tail_ = head_; }

  private:
    Storage storage_;
    std::size_t head_ = 0;  // write frontier: total values pushed
    std::size_t tail_ = 0;  // read frontier: total values consumed
};

}  // namespace glupsk
