#pragma once

#include "core/types.hpp"

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace glupsk {

struct Machine;

struct GlkWindowHandle {
    u32 id = 0;
};

struct GlkStreamHandle {
    u32 id = 0;
};

struct GlkFileRefHandle {
    u32 id = 0;
};

enum class GlkSelector : u32 {
    exit = 0x0001,
    gestalt = 0x0004,
    window_iterate = 0x0020,
    window_get_rock = 0x0021,
    window_get_root = 0x0022,
    window_open = 0x0023,
    window_get_size = 0x0025,
    window_clear = 0x002a,
    window_move_cursor = 0x002b,
    window_get_stream = 0x002c,
    set_window = 0x002f,
    stream_iterate = 0x0040,
    stream_get_rock = 0x0041,
    stream_open_memory = 0x0043,
    stream_close = 0x0044,
    stream_set_current = 0x0047,
    stream_get_current = 0x0048,
    fileref_iterate = 0x0064,
    fileref_get_rock = 0x0065,
    put_char = 0x0080,
    put_char_stream = 0x0081,
    put_string = 0x0082,
    put_string_stream = 0x0083,
    put_buffer = 0x0084,
    put_buffer_stream = 0x0085,
    set_style = 0x0086,
    char_to_lower = 0x00a0,
    char_to_upper = 0x00a1,
    stylehint_set = 0x00b0,
    select = 0x00c0,
    request_line_event = 0x00d0,
    buffer_to_lower_case_uni = 0x0120,
    buffer_to_upper_case_uni = 0x0121,
    buffer_to_title_case_uni = 0x0122,
    buffer_canon_decompose_uni = 0x0123,
    buffer_canon_normalize_uni = 0x0124,
    put_char_uni = 0x0128,
    put_string_uni = 0x0129,
    put_buffer_uni = 0x012a,
    put_char_stream_uni = 0x012b,
    put_string_stream_uni = 0x012c,
    put_buffer_stream_uni = 0x012d,
    stream_open_memory_uni = 0x0139,
    request_line_event_uni = 0x0141,
};

enum class GlkGestaltSelector : u32 {
    version = 0,
    char_output = 3,
    unicode = 15,
    unicode_norm = 16,
};

struct GlkGestaltQuery {
    GlkGestaltSelector selector = GlkGestaltSelector::version;
    u32 value = 0;
};

inline u32 glk_default_gestalt(GlkGestaltQuery query) {
    switch (query.selector) {
        case GlkGestaltSelector::version:
            return 0x00000706;
        case GlkGestaltSelector::char_output:
            return 2;
        case GlkGestaltSelector::unicode:
            return 1;
        case GlkGestaltSelector::unicode_norm:
            return 0;
        default:
            return 0;
    }
}

enum class GlkTextEncoding : u8 {
    latin1,
    unicode,
};

enum class GlkStyle : u32 {
    normal = 0,
    emphasized = 1,
    preformatted = 2,
    header = 3,
    subheader = 4,
    alert = 5,
    note = 6,
    block_quote = 7,
    input = 8,
    user1 = 9,
    user2 = 10,
};

struct GlkTextChar {
    u32 value = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

struct GlkTextBuffer {
    u32 address = 0;
    u32 length = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

struct GlkTextString {
    u32 address = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

// A borrowed text source in VM memory, or an immediate character. This is a
// bridge-side representation; semantic hosts should receive GlkTextData after
// the bridge has decoded the VM memory involved.
using GlkText = std::variant<GlkTextChar, GlkTextBuffer, GlkTextString>;

// Materialized text passed across the semantic host boundary. std::string is
// used for Latin-1/byte-oriented Glk text; std::vector<u32> is used for Unicode
// codepoints.
using GlkTextData = std::variant<std::string, std::vector<u32>>;

struct GlkEvent {
    u32 type = 0;
    GlkWindowHandle window = {};
    u32 val1 = 0;
    u32 val2 = 0;
};

struct GlkLineInputRequest {
    GlkWindowHandle window = {};
    u32 max_length = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
    GlkTextData initial_text = std::string{};
};

struct GlkCharInputRequest {
    GlkWindowHandle window = {};
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

struct GlkTimerRequest {
    u32 milliseconds = 0;
};

using GlkEventInterest =
    std::variant<GlkLineInputRequest, GlkCharInputRequest, GlkTimerRequest>;

struct GlkEventRequest {
    span<const GlkEventInterest> interests;
};

using GlkInputText = GlkTextData;

struct GlkLineInputEvent {
    GlkWindowHandle window = {};
    GlkInputText text;
};

struct GlkCharInputEvent {
    GlkWindowHandle window = {};
    u32 value = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

struct GlkTimerEvent {};

struct GlkArrangeEvent {};

using GlkHostEvent =
    std::variant<GlkLineInputEvent,
                 GlkCharInputEvent,
                 GlkTimerEvent,
                 GlkArrangeEvent>;

struct GlkReturned {
    u32 value = 0;
};

struct GlkBlocked {};

struct GlkFatal {
    std::string message;
};

using GlkCallResult = std::variant<GlkReturned, GlkBlocked, GlkFatal>;
using GlkEventResult = std::variant<GlkHostEvent, GlkBlocked, GlkFatal>;

struct GlkRuntime {
    virtual ~GlkRuntime() = default;

    virtual GlkCallResult call(Machine& machine,
                               u32 selector,
                               span<const u32> args) = 0;
    virtual void put_char(Machine& machine, u32 ch) = 0;

    // Drain any output buffered on the current stream to its backend. Called at
    // VM-instruction observe points (the run loop, before each @glk call) so
    // buffered per-character output never outlives a point where it could be
    // observed. The default is a no-op for runtimes that do not buffer.
    virtual void flush(Machine& machine) { (void) machine; }
};

inline GlkCallResult glk_returned(u32 value = 0) {
    return GlkReturned{.value = value};
}

inline GlkCallResult glk_blocked() {
    return GlkBlocked{};
}

inline GlkCallResult glk_fatal(std::string message) {
    return GlkFatal{.message = std::move(message)};
}

struct GlkWindowSpec {
    GlkWindowHandle split = {};
    u32 method = 0;
    u32 size = 0;
    u32 type = 0;
    u32 rock = 0;
};

enum class GlkWindowType : u32 {
    blank = 2,
    text_buffer = 3,
    text_grid = 4,
    graphics = 5,
};

struct GlkWindowSize {
    u32 width = 80;
    u32 height = 24;
};

template <typename Host>
struct GlkOpenedWindow {
    typename Host::Window window;
    typename Host::Stream stream;
};

}  // namespace glupsk
