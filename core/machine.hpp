#pragma once

#include "core/story.hpp"
#include "core/types.hpp"

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
};

struct Registers {
    u32 pc = 0;
    u32 string_table = 0;
};

struct Machine {
    Memory memory;
    Stack stack;
    Registers regs;

    static Machine from_story(const Story& story);
};

}  // namespace glupsk
