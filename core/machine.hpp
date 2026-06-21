#pragma once

#include "core/story.hpp"
#include "core/types.hpp"

#include <array>
#include <deque>
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

struct Machine;

enum class TranscriptStreamKind : u8 {
    none,
    window,
    memory,
    unicode_memory,
};

struct TranscriptStream {
    TranscriptStreamKind kind = TranscriptStreamKind::none;
    u32 id = 0;
    u32 address = 0;
    u32 len = 0;
    u32 pos = 0;
    u32 read_count = 0;
    u32 write_count = 0;
    u32 mode = 0;
    u32 rock = 0;
};

struct TranscriptWindow {
    u32 id = 0;
    u32 rock = 0;
    u32 stream_id = 0;
    u32 type = 0;
};

struct TranscriptGlk {
    std::string transcript;
    std::deque<std::string> input_lines;
    std::array<TranscriptWindow, 16> windows = {};
    std::array<TranscriptStream, 64> streams = {};
    u32 root_window = 0;
    u32 next_window_id = 1;
    u32 next_stream_id = 1;
    u32 current_stream = 0;
    bool line_pending = false;
    bool line_unicode = false;
    u32 line_window = 0;
    u32 line_buffer = 0;
    u32 line_maxlen = 0;
    u32 line_initial_len = 0;

    void add_input_line(std::string line);
    bool select_would_block() const;
    u32 call(Machine& machine, u32 selector, span<const u32> args);
    void put_char(Machine& machine, u32 ch);
};

struct AccelerationEntry {
    u32 address = 0;
    u32 function = 0;
};

struct Machine {
    Memory memory;
    Stack stack;
    Registers regs;
    std::array<u32, 9> accel_params = {};
    std::array<AccelerationEntry, 64> accel_entries = {};
    bool running = false;
    bool blocked = false;
    bool halted = false;
    TranscriptGlk* glk = nullptr;

    static Machine from_story(const Story& story);
};

}  // namespace glupsk
