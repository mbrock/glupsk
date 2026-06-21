#pragma once

#include "core/story.hpp"
#include "core/types.hpp"

#include <string>

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

struct TranscriptGlk {
    std::string transcript;

    u32 call(u32 selector, span<const u32> args);
    void put_char(u32 ch);
};

struct Machine {
    Memory memory;
    Stack stack;
    Registers regs;
    bool running = false;
    bool halted = false;
    TranscriptGlk* glk = nullptr;

    static Machine from_story(const Story& story);
};

}  // namespace glupsk
