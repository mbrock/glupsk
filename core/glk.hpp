#pragma once

#include "core/types.hpp"

#include <array>
#include <concepts>
#include <deque>
#include <string>
#include <variant>

namespace glupsk {

struct Machine;

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

struct GlkEvent {
    u32 type = 0;
    u32 window = 0;
    u32 val1 = 0;
    u32 val2 = 0;
};

enum class GlkCallStatus : u8 {
    returned,
    blocked,
    fatal,
};

struct GlkCallResult {
    GlkCallStatus status = GlkCallStatus::returned;
    u32 value = 0;
};

struct GlkWindowStream {};

struct GlkMemoryStream {
    u32 address = 0;
    u32 len = 0;
    u32 mode = 0;
};

struct GlkUnicodeMemoryStream {
    u32 address = 0;
    u32 len = 0;
    u32 mode = 0;
};

using GlkStreamBacking =
    std::variant<std::monostate, GlkWindowStream, GlkMemoryStream,
                 GlkUnicodeMemoryStream>;

template <typename Host>
concept SemanticGlkHost = requires(Host host,
                                   Machine& machine,
                                   typename Host::Window window,
                                   typename Host::Stream stream,
                                   typename Host::Rock rock,
                                   u32 value,
                                   u32 address,
                                   u32 length) {
    // A semantic host can choose native C++ types for its objects. The Glulx
    // bridge is responsible for mapping those types to and from story u32s.
    typename Host::Window;
    typename Host::Stream;
    typename Host::FileRef;
    typename Host::Rock;

    // Unknown gestalt selectors return 0.
    { host.gestalt(value, value) } -> std::same_as<u32>;

    // Create and query display surfaces. The rock is caller-owned metadata
    // that Glk stores and returns but never interprets.
    { host.window_open(window, value, value, value, rock) }
        -> std::same_as<typename Host::Window>;
    { host.window_get_root() } -> std::same_as<typename Host::Window>;
    { host.window_get_rock(window) } -> std::same_as<typename Host::Rock>;
    { host.window_get_stream(window) } -> std::same_as<typename Host::Stream>;

    // Current streams receive unqualified text output.
    { host.stream_set_current(stream) } -> std::same_as<void>;
    { host.stream_get_current() } -> std::same_as<typename Host::Stream>;
    { host.stream_get_rock(stream) } -> std::same_as<typename Host::Rock>;

    // Text and memory buffers are still VM-addressed at this boundary.
    { host.put_char(machine, value) } -> std::same_as<void>;
    { host.put_buffer(machine, address, length, false) } -> std::same_as<void>;
    { host.put_string(machine, address, false) } -> std::same_as<void>;

    // Input requests arm a later event; select either writes one or blocks.
    { host.request_line_event(machine, window, address, length, value, false) }
        -> std::same_as<void>;
    { host.select(machine, address) } -> std::same_as<GlkCallResult>;
};

struct TranscriptStream {
    GlkStreamBacking backing = {};
    u32 id = 0;
    u32 pos = 0;
    u32 read_count = 0;
    u32 write_count = 0;
    u32 rock = 0;

    bool allocated() const {
        return !std::holds_alternative<std::monostate>(backing);
    }
};

struct TranscriptWindow {
    u32 id = 0;
    u32 rock = 0;
    u32 stream_id = 0;
    u32 type = 0;
};

struct TranscriptGlk {
    struct Window {
        u32 id = 0;
    };
    struct Stream {
        u32 id = 0;
    };
    struct FileRef {
        u32 id = 0;
    };
    using Rock = u32;

    // Linear log of text written to window streams or to no current stream.
    std::string transcript;

    // Scripted line input consumed by glk_select after request_line_event.
    std::deque<std::string> input_lines;

    // Fixed-size object tables keep this test host pointerless and simple.
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

}  // namespace glupsk
