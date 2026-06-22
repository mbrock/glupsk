#pragma once

#include "core/glk.hpp"
#include "core/story.hpp"
#include "core/types.hpp"

#include <array>

namespace glupsk {

struct Memory {
    Bytes bytes;
    u32 ramstart = 0;
    u32 extstart = 0;
    u32 endmem = 0;

    u8 read8(u32 address) const;
    u16 read16(u32 address) const;
    u32 read32(u32 address) const;

    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);
};

// A proxy vault (in the deck_storage sense) over a slice of VM memory: count
// slots of T starting at byte address `base`, each slot read/written via the
// Memory accessors. Two properties make it a proxy rather than a span: it
// resolves the byte address through `Memory` on every access (so it survives
// the memory vector reallocating), and it goes through read32/write32 (so the
// big-endian byte layout is honored). T is u8 (Latin-1) or u32 (Unicode).
template <typename T>
class MemorySlab {
  public:
    class Reference {
      public:
        Reference(Memory& memory, u32 address)
            : memory_(&memory), address_(address) {}

        operator T() const {
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(memory_->read8(address_));
            } else {
                return static_cast<T>(memory_->read32(address_));
            }
        }

        Reference& operator=(T value) {
            if constexpr (sizeof(T) == 1) {
                memory_->write8(address_, static_cast<u8>(value));
            } else {
                memory_->write32(address_, static_cast<u32>(value));
            }
            return *this;
        }

      private:
        Memory* memory_;
        u32 address_;
    };

    MemorySlab() = default;
    MemorySlab(Memory& memory, u32 base, std::size_t count)
        : memory_(&memory), base_(base), count_(count) {}

    std::size_t size() const { return count_; }

    Reference operator[](std::size_t index) const {
        return Reference{*memory_,
                         base_ + static_cast<u32>(index * sizeof(T))};
    }

  private:
    Memory* memory_ = nullptr;
    u32 base_ = 0;
    std::size_t count_ = 0;
};

struct Stack {
    Bytes bytes;
    u32 sp = 0;

    bool empty() const { return sp == 0; }
    u32 capacity() const { return static_cast<u32>(bytes.size()); }

    u8 read8(u32 address) const;
    u16 read16(u32 address) const;
    u32 read32(u32 address) const;

    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);

    void push32(u32 value);
    u32 pop32();
};

struct Registers {
    u32 pc = 0;
    u32 string_table = 0;
    u32 frame_ptr = 0;
    u32 iosys_mode = 0;
    u32 iosys_rock = 0;
};

struct AccelerationEntry {
    u32 address = 0;
    u32 function = 0;
};

struct Machine {
    Memory memory;
    Stack stack;
    Registers regs;
    u32 rng_state = 0x6d2b79f5;
    std::array<u32, 9> accel_params = {};
    std::array<AccelerationEntry, 64> accel_entries = {};
    bool running = false;
    bool blocked = false;
    bool halted = false;
    GlkRuntime* glk = nullptr;

    static Machine from_story(const Story& story);
};

}  // namespace glupsk
