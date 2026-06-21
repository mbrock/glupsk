#pragma once

#include "core/types.hpp"

#include <array>
#include <concepts>
#include <deque>
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

enum class GlkTextEncoding : u8 {
    latin1,
    unicode,
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

// A borrowed text source in VM memory, or an immediate character.
//
// GlkTextBuffer uses an explicit length. GlkTextString is zero-terminated in
// VM memory. Neither owns host pointers; the host receives Machine& plus Glulx
// addresses so it can copy, retain, or stream from VM memory deliberately.
using GlkText = std::variant<GlkTextChar, GlkTextBuffer, GlkTextString>;

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
    GlkTextBuffer initial_text = {};
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

using GlkInputText = std::variant<std::string, std::vector<u32>>;

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

using GlkHostEvent =
    std::variant<GlkLineInputEvent, GlkCharInputEvent, GlkTimerEvent>;

struct GlkReturned {
    u32 value = 0;
};

struct GlkBlocked {};

struct GlkFatal {
    std::string message;
};

using GlkCallResult = std::variant<GlkReturned, GlkBlocked, GlkFatal>;
using GlkEventResult = std::variant<GlkHostEvent, GlkBlocked, GlkFatal>;

inline GlkCallResult glk_returned(u32 value = 0) {
    return GlkReturned{.value = value};
}

inline GlkCallResult glk_blocked() {
    return GlkBlocked{};
}

inline GlkCallResult glk_fatal(std::string message) {
    return GlkFatal{.message = std::move(message)};
}

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

template <typename Registry>
concept GlkHandleRegistry = requires(Registry registry,
                                     GlkWindowHandle window,
                                     GlkStreamHandle stream,
                                     GlkFileRefHandle fileref,
                                     typename Registry::Window& native_window,
                                     typename Registry::Stream& native_stream,
                                     typename Registry::FileRef& native_fileref) {
    // Native object types belong behind a registry. A bridge or host can use
    // this to resolve stable Glulx handles into platform/native objects.
    typename Registry::Window;
    typename Registry::Stream;
    typename Registry::FileRef;

    { registry.require_window(window) } -> std::same_as<typename Registry::Window&>;
    { registry.require_stream(stream) } -> std::same_as<typename Registry::Stream&>;
    { registry.require_fileref(fileref) } -> std::same_as<typename Registry::FileRef&>;

    { registry.intern_window(native_window) } -> std::same_as<GlkWindowHandle>;
    { registry.intern_stream(native_stream) } -> std::same_as<GlkStreamHandle>;
    { registry.intern_fileref(native_fileref) } -> std::same_as<GlkFileRefHandle>;
};

template <typename Host, typename Registry>
concept SemanticGlkHost =
    GlkHandleRegistry<Registry> &&
    requires(Host host,
             Registry& registry,
             Machine& machine,
             GlkWindowHandle window,
             GlkStreamHandle stream,
             u32 rock,
             u32 value,
             GlkText text,
             GlkEventRequest event_request) {
        // The semantic boundary uses typed, serializable Glulx handles. The
        // registry resolves those handles to host/native objects when needed.

        // Unknown gestalt selectors return 0.
        { host.gestalt(value, value) } -> std::same_as<u32>;

        // Create and query display surfaces. The rock is caller-owned metadata
        // that Glk stores and returns but never interprets.
        { host.window_open(registry, window, value, value, value, rock) }
            -> std::same_as<GlkWindowHandle>;
        { host.window_get_root(registry) } -> std::same_as<GlkWindowHandle>;
        { host.window_get_rock(registry, window) } -> std::same_as<u32>;
        { host.window_get_stream(registry, window) }
            -> std::same_as<GlkStreamHandle>;

        // Current-stream state is part of the Glk dispatch/session adapter, not a
        // backend policy decision. The semantic host writes to explicit streams.
        { host.stream_get_rock(registry, stream) } -> std::same_as<u32>;

        // Write an immediate character, a fixed-length VM buffer, or a
        // zero-terminated VM string to an explicit stream.
        { host.write(machine, registry, stream, text) }
            -> std::same_as<GlkCallResult>;

        // The bridge accumulates pending Glk requests and asks the host for the
        // next matching event. The bridge owns mapping host events back into
        // Glulx event_t structs and VM input buffers.
        { host.select(machine, registry, event_request) }
            -> std::same_as<GlkEventResult>;
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

struct TranscriptFileRef {
    u32 id = 0;
    u32 rock = 0;
};

struct TranscriptGlk {
    using Window = TranscriptWindow;
    using Stream = TranscriptStream;
    using FileRef = TranscriptFileRef;
    using Rock = u32;

    // Linear log of text written to window streams or to no current stream.
    std::string transcript;

    // Scripted line input consumed by glk_select after request_line_event.
    std::deque<std::string> input_lines;

    // Fixed-size object tables keep this test host pointerless and simple.
    std::array<TranscriptWindow, 16> windows = {};
    std::array<TranscriptStream, 64> streams = {};
    std::array<TranscriptFileRef, 16> filerefs = {};
    u32 root_window = 0;
    u32 next_window_id = 1;
    u32 next_stream_id = 1;

    // Temporary dispatch/session state. A future bridge should own this and
    // call a semantic host write(machine, stream, text) operation instead.
    u32 current_stream = 0;

    bool line_pending = false;
    bool line_unicode = false;
    u32 line_window = 0;
    u32 line_buffer = 0;
    u32 line_maxlen = 0;
    u32 line_initial_len = 0;

    void add_input_line(std::string line);
    TranscriptWindow& require_window(GlkWindowHandle window);
    TranscriptStream& require_stream(GlkStreamHandle stream);
    TranscriptFileRef& require_fileref(GlkFileRefHandle fileref);
    GlkWindowHandle intern_window(TranscriptWindow& window);
    GlkStreamHandle intern_stream(TranscriptStream& stream);
    GlkFileRefHandle intern_fileref(TranscriptFileRef& fileref);
    bool select_would_block() const;
    u32 call(Machine& machine, u32 selector, span<const u32> args);
    void put_char(Machine& machine, u32 ch);
};

static_assert(GlkHandleRegistry<TranscriptGlk>);

}  // namespace glupsk
