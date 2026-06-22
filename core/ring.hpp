#pragma once

#include "core/types.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace glupsk {

// Fixed-capacity circular buffer for trivially-copyable values.
//
// This is a refinement of `module RingBuffer` in pacioli/src/Pacioli.agda:
// the state is two monotonically increasing counters, head_ (the write
// frontier, total pushed) and tail_ (the read frontier, total consumed).
// Both only ever advance; everything else is derived from their difference,
// which is the Pacioli T-account `(head // tail)`:
//
//   * size()   = head_ - tail_              -- Occupancy / debit-positive reading
//   * unused() = capacity() - size()        -- Free / credit-positive reading
//   * push()   posts (1 // 0); consume(n)   posts (0 // n)
//   * the safety invariant is the halfspace guard  tail_ <= head_ <= tail_ + N,
//     and  size() + unused() == capacity()  is the balance sheet.
//
// Indexing masks the frontiers by capacity (`% capacity()`), so a value's
// physical slot is its frontier modulo N; the monotone counters never reset,
// which sidesteps the classic full-vs-empty ambiguity. A zero-capacity ring
// is a valid "unbuffered" ring that accepts nothing. Reading is
// segment-at-a-time: readable() returns the first contiguous run, so a full
// drain loops readable()/consume() until empty.
template <typename T>
class Ring {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Ring is intended for trivial value types");

  public:
    Ring() = default;
    explicit Ring(std::size_t capacity) : storage_(capacity) {}

    std::size_t capacity() const { return storage_.size(); }
    std::size_t size() const { return head_ - tail_; }
    std::size_t unused() const { return capacity() - size(); }
    bool empty() const { return head_ == tail_; }
    bool full() const { return size() == capacity(); }

    // Push one value. Returns false (and drops it) when the ring is full.
    bool push(T value) {
        if (full()) {
            return false;
        }
        storage_[head_ % capacity()] = value;
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
    span<const T> readable() const {
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
    std::vector<T> storage_;
    std::size_t head_ = 0;  // write frontier: total values pushed
    std::size_t tail_ = 0;  // read frontier: total values consumed
};

}  // namespace glupsk
