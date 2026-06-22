#pragma once

#include "core/glk.hpp"
#include "core/machine.hpp"

#include <algorithm>
#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace glupsk {

template <typename Host>
concept GlkHost =
    requires(Host host,
             GlkGestaltQuery gestalt_query,
             GlkWindowSpec window_spec,
             typename Host::Window& window,
             typename Host::Stream& stream,
             GlkTextData text,
             GlkEventRequest event_request,
             u32 value) {
        typename Host::Window;
        typename Host::Stream;
        typename Host::FileRef;

        { host.gestalt(gestalt_query) } -> std::same_as<u32>;
        { host.open_window(window_spec) } -> std::same_as<GlkOpenedWindow<Host>>;
        { host.window_size(window) } -> std::same_as<GlkWindowSize>;
        { host.window_clear(window) } -> std::same_as<void>;
        { host.window_move_cursor(window, value, value) } -> std::same_as<void>;
        { host.write(stream, text) } -> std::same_as<GlkCallResult>;
        { host.select(event_request) } -> std::same_as<GlkEventResult>;
        { host.echo_line_input() } -> std::same_as<bool>;
    };

template <GlkHost Host>
class GlkSession : public GlkRuntime {
  public:
    explicit GlkSession(Host host = {}) : host_(std::move(host)) {}

    Host& host() { return host_; }
    const Host& host() const { return host_; }

    GlkStreamHandle current_stream() const { return current_stream_; }

    GlkCallResult call(Machine& machine,
                       u32 selector,
                       span<const u32> args) override {
        switch (static_cast<GlkSelector>(selector)) {
            case GlkSelector::exit:
                machine.halted = true;
                machine.running = false;
                return glk_returned();
            case GlkSelector::gestalt:
                return glk_returned(
                    args.empty()
                        ? 0
                        : host_.gestalt(GlkGestaltQuery{
                              .selector =
                                  static_cast<GlkGestaltSelector>(args[0]),
                              .value = args.size() >= 2 ? args[1] : 0,
                          }));
            case GlkSelector::window_iterate:
                return glk_returned(iterate_windows(
                    machine, args.empty() ? 0 : args[0],
                    args.size() >= 2 ? args[1] : 0));
            case GlkSelector::window_get_rock:
                return glk_returned(args.empty() ? 0
                                                 : require_window(args[0]).rock);
            case GlkSelector::window_get_root:
                return glk_returned(root_window_.id);
            case GlkSelector::window_open:
                return glk_returned(open_window(
                                        GlkWindowSpec{
                                            .split = {
                                                .id = args.empty() ? 0 : args[0],
                                            },
                                            .method = args.size() >= 2 ? args[1] : 0,
                                            .size = args.size() >= 3 ? args[2] : 0,
                                            .type = args.size() >= 4 ? args[3] : 0,
                                            .rock = args.size() >= 5 ? args[4] : 0,
                                        })
                                        .id);
            case GlkSelector::window_get_size:
                if (args.size() >= 3) {
                    const auto size = host_.window_size(require_window(args[0]).window);
                    write_glk_ref(machine, args[1], size.width);
                    write_glk_ref(machine, args[2], size.height);
                }
                return glk_returned();
            case GlkSelector::window_clear:
                if (!args.empty()) {
                    host_.window_clear(require_window(args[0]).window);
                }
                return glk_returned();
            case GlkSelector::window_move_cursor:
                if (args.size() >= 3) {
                    host_.window_move_cursor(require_window(args[0]).window,
                                             args[1], args[2]);
                }
                return glk_returned();
            case GlkSelector::window_get_stream:
                return glk_returned(args.empty() ? 0
                                                 : require_window(args[0]).stream.id);
            case GlkSelector::set_window:
                set_window(args.empty() ? GlkWindowHandle{} :
                                          GlkWindowHandle{.id = args[0]});
                return glk_returned();
            case GlkSelector::stream_iterate:
                return glk_returned(iterate_streams(
                    machine, args.empty() ? 0 : args[0],
                    args.size() >= 2 ? args[1] : 0));
            case GlkSelector::stream_get_rock:
                return glk_returned(args.empty() ? 0 : require_stream(args[0]).rock);
            case GlkSelector::stream_open_memory:
                return glk_returned(open_memory_stream(
                    GlkMemoryStream{.address = args.size() >= 1 ? args[0] : 0,
                                    .len = args.size() >= 2 ? args[1] : 0,
                                    .mode = args.size() >= 3 ? args[2] : 0},
                    args.size() >= 4 ? args[3] : 0)
                                        .id);
            case GlkSelector::stream_open_memory_uni:
                return glk_returned(open_memory_stream(
                    GlkUnicodeMemoryStream{
                        .address = args.size() >= 1 ? args[0] : 0,
                        .len = args.size() >= 2 ? args[1] : 0,
                        .mode = args.size() >= 3 ? args[2] : 0},
                    args.size() >= 4 ? args[3] : 0)
                                        .id);
            case GlkSelector::stream_close:
                if (!args.empty()) {
                    close_stream(machine, GlkStreamHandle{.id = args[0]},
                                 args.size() >= 2 ? args[1] : 0);
                }
                return glk_returned();
            case GlkSelector::stream_set_current:
                set_current_stream(args.empty() ? GlkStreamHandle{} :
                                                  GlkStreamHandle{.id = args[0]});
                return glk_returned();
            case GlkSelector::stream_get_current:
                return glk_returned(current_stream_.id);
            case GlkSelector::fileref_iterate:
                if (args.size() >= 2) {
                    write_glk_ref(machine, args[1], 0);
                }
                return glk_returned();
            case GlkSelector::fileref_get_rock:
                if (!args.empty() && args[0] != 0) {
                    throw std::runtime_error(std::format(
                        "fileref_get_rock received invalid fileref {}", args[0]));
                }
                return glk_returned();
            case GlkSelector::set_style:
            case GlkSelector::stylehint_set:
                return glk_returned();
            case GlkSelector::put_char:
                return args.empty() ? glk_returned()
                                    : write(machine, GlkTextChar{.value = args[0]});
            case GlkSelector::put_char_uni:
                return args.empty()
                           ? glk_returned()
                           : write(machine, GlkTextChar{
                                                .value = args[0],
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_char_stream:
                return args.size() < 2
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextChar{.value = args[1]});
            case GlkSelector::put_char_stream_uni:
                return args.size() < 2
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextChar{
                                          .value = args[1],
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::put_string:
                return args.empty()
                           ? glk_returned()
                           : write(machine, GlkTextString{.address = args[0] + 1});
            case GlkSelector::put_string_stream:
                return args.size() < 2
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextString{.address = args[1] + 1});
            case GlkSelector::put_string_uni:
                return args.empty()
                           ? glk_returned()
                           : write(machine, GlkTextString{
                                                .address = args[0] + 4,
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_string_stream_uni:
                return args.size() < 2
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextString{
                                          .address = args[1] + 4,
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::put_buffer:
                return args.size() < 2
                           ? glk_returned()
                           : write(machine, GlkTextBuffer{.address = args[0],
                                                          .length = args[1]});
            case GlkSelector::put_buffer_stream:
                return args.size() < 3
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextBuffer{.address = args[1],
                                                    .length = args[2]});
            case GlkSelector::put_buffer_uni:
                return args.size() < 2
                           ? glk_returned()
                           : write(machine, GlkTextBuffer{
                                                .address = args[0],
                                                .length = args[1],
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_buffer_stream_uni:
                return args.size() < 3
                           ? glk_returned()
                           : write_to(machine, GlkStreamHandle{.id = args[0]},
                                      GlkTextBuffer{
                                          .address = args[1],
                                          .length = args[2],
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::buffer_to_lower_case_uni:
                return glk_returned(args.size() >= 3
                                        ? transform_unicode_buffer(
                                              machine, args[0], args[1], args[2],
                                              glk_char_to_lower)
                                        : 0);
            case GlkSelector::buffer_to_upper_case_uni:
                return glk_returned(args.size() >= 3
                                        ? transform_unicode_buffer(
                                              machine, args[0], args[1], args[2],
                                              glk_char_to_upper)
                                        : 0);
            case GlkSelector::buffer_to_title_case_uni:
                return glk_returned(args.size() >= 4
                                        ? title_case_unicode_buffer(
                                              machine, args[0], args[1], args[2],
                                              args[3] != 0)
                                        : 0);
            case GlkSelector::buffer_canon_decompose_uni:
            case GlkSelector::buffer_canon_normalize_uni:
                return glk_returned(args.size() >= 3 ? args[2] : 0);
            case GlkSelector::request_line_event:
                if (args.size() >= 3) {
                    request_line_event(machine, GlkWindowHandle{.id = args[0]},
                                       args[1], args[2],
                                       args.size() >= 4 ? args[3] : 0,
                                       GlkTextEncoding::latin1);
                }
                return glk_returned();
            case GlkSelector::request_line_event_uni:
                if (args.size() >= 3) {
                    request_line_event(machine, GlkWindowHandle{.id = args[0]},
                                       args[1], args[2],
                                       args.size() >= 4 ? args[3] : 0,
                                       GlkTextEncoding::unicode);
                }
                return glk_returned();
            case GlkSelector::select:
                return select(machine, args.empty() ? 0 : args[0]);
            case GlkSelector::char_to_lower:
                return glk_returned(args.empty() ? 0 : glk_char_to_lower(args[0]));
            case GlkSelector::char_to_upper:
                return glk_returned(args.empty() ? 0 : glk_char_to_upper(args[0]));
            default:
                throw std::runtime_error(
                    std::format("unsupported Glk selector 0x{:04x}", selector));
        }
    }

    u32 call_returned(Machine& machine, u32 selector, span<const u32> args) {
        const auto result = call(machine, selector, args);
        if (const auto* returned = std::get_if<GlkReturned>(&result)) {
            return returned->value;
        }
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            throw std::runtime_error(fatal->message);
        }
        throw std::runtime_error("Glk call blocked");
    }

    void put_char(Machine& machine, u32 ch) override {
        const auto result = write(machine, GlkTextChar{
                                               .value = ch,
                                               .encoding = GlkTextEncoding::unicode,
                                           });
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            throw std::runtime_error(fatal->message);
        }
        if (std::holds_alternative<GlkBlocked>(result)) {
            throw std::runtime_error("Glk output blocked outside @glk");
        }
    }

  private:
    struct HostStream {
        typename Host::Stream stream;
    };

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

    using StreamBacking =
        std::variant<std::monostate, HostStream, GlkMemoryStream,
                     GlkUnicodeMemoryStream>;

    struct WindowRecord {
        typename Host::Window window;
        u32 rock = 0;
        u32 type = 0;
        GlkStreamHandle stream = {};
    };

    struct StreamRecord {
        StreamBacking backing = {};
        u32 rock = 0;
        u32 pos = 0;
        u32 read_count = 0;
        u32 write_count = 0;

        bool allocated() const {
            return !std::holds_alternative<std::monostate>(backing);
        }
    };

    static u32 glk_char_to_lower(u32 ch) {
        if ((ch >= 0x41 && ch <= 0x5a) || (ch >= 0xc0 && ch <= 0xd6) ||
            (ch >= 0xd8 && ch <= 0xde)) {
            return ch + 0x20;
        }
        return ch;
    }

    static u32 glk_char_to_upper(u32 ch) {
        if ((ch >= 0x61 && ch <= 0x7a) || (ch >= 0xe0 && ch <= 0xf6) ||
            (ch >= 0xf8 && ch <= 0xfe)) {
            return ch - 0x20;
        }
        return ch;
    }

    static u32 text_length(const GlkTextData& text) {
        if (const auto* bytes = std::get_if<std::string>(&text)) {
            return static_cast<u32>(bytes->size());
        }
        return static_cast<u32>(std::get<std::vector<u32>>(text).size());
    }

    static void write_glk_ref(Machine& machine, u32 address, u32 value) {
        if (address == 0) {
            return;
        }
        if (address == 0xffffffffu) {
            machine.stack.push32(value);
            return;
        }
        machine.memory.write32(address, value);
    }

    static void write_event_field(Machine& machine,
                                  u32 address,
                                  u32 field,
                                  u32 value) {
        if (address == 0) {
            return;
        }
        if (address == 0xffffffffu) {
            machine.stack.push32(value);
            return;
        }
        machine.memory.write32(address + field * 4, value);
    }

    static GlkTextData materialize_text(Machine& machine, const GlkText& text) {
        return std::visit(
            [&](const auto& value) -> GlkTextData {
                return materialize_text(machine, value);
            },
            text);
    }

    static GlkTextData materialize_text(Machine&, const GlkTextChar& text) {
        if (text.encoding == GlkTextEncoding::unicode) {
            return std::vector<u32>{text.value};
        }
        return std::string{static_cast<char>(text.value & 0xffu)};
    }

    static GlkTextData materialize_text(Machine& machine,
                                        const GlkTextBuffer& text) {
        if (text.encoding == GlkTextEncoding::unicode) {
            auto codepoints = std::vector<u32>{};
            codepoints.reserve(text.length);
            for (u32 index = 0; index < text.length; ++index) {
                codepoints.push_back(
                    machine.memory.read32(text.address + index * 4));
            }
            return codepoints;
        }

        auto bytes = std::string{};
        bytes.reserve(text.length);
        for (u32 index = 0; index < text.length; ++index) {
            bytes.push_back(
                static_cast<char>(machine.memory.read8(text.address + index)));
        }
        return bytes;
    }

    static GlkTextData materialize_text(Machine& machine,
                                        const GlkTextString& text) {
        if (text.encoding == GlkTextEncoding::unicode) {
            auto codepoints = std::vector<u32>{};
            auto address = text.address;
            while (true) {
                const auto ch = machine.memory.read32(address);
                address += 4;
                if (ch == 0) {
                    return codepoints;
                }
                codepoints.push_back(ch);
            }
        }

        auto bytes = std::string{};
        auto address = text.address;
        while (true) {
            const auto ch = machine.memory.read8(address++);
            if (ch == 0) {
                return bytes;
            }
            bytes.push_back(static_cast<char>(ch));
        }
    }

    static u32 transform_unicode_buffer(Machine& machine,
                                        u32 address,
                                        u32 len,
                                        u32 numchars,
                                        u32 (*transform)(u32)) {
        const auto writable = std::min(len, numchars);
        for (u32 index = 0; index < writable; ++index) {
            const auto cell = address + index * 4;
            machine.memory.write32(cell, transform(machine.memory.read32(cell)));
        }
        return numchars;
    }

    static u32 title_case_unicode_buffer(Machine& machine,
                                         u32 address,
                                         u32 len,
                                         u32 numchars,
                                         bool lower_rest) {
        const auto writable = std::min(len, numchars);
        if (writable == 0) {
            return numchars;
        }

        machine.memory.write32(address,
                               glk_char_to_upper(machine.memory.read32(address)));
        if (lower_rest) {
            for (u32 index = 1; index < writable; ++index) {
                const auto cell = address + index * 4;
                machine.memory.write32(
                    cell, glk_char_to_lower(machine.memory.read32(cell)));
            }
        }
        return numchars;
    }

    WindowRecord& require_window(u32 id) {
        if (id == 0 || id > windows_.size() || !windows_[id - 1]) {
            throw std::runtime_error(
                std::format("invalid Glk window handle {}", id));
        }
        return *windows_[id - 1];
    }

    StreamRecord& require_stream(u32 id) {
        if (id == 0 || id > streams_.size() || !streams_[id - 1] ||
            !streams_[id - 1]->allocated()) {
            throw std::runtime_error(
                std::format("invalid Glk stream handle {}", id));
        }
        return *streams_[id - 1];
    }

    GlkStreamHandle allocate_stream(StreamRecord stream) {
        streams_.push_back(std::make_unique<StreamRecord>(std::move(stream)));
        return {.id = static_cast<u32>(streams_.size())};
    }

    GlkWindowHandle allocate_window(WindowRecord window) {
        windows_.push_back(std::make_unique<WindowRecord>(std::move(window)));
        return {.id = static_cast<u32>(windows_.size())};
    }

    GlkWindowHandle open_window(GlkWindowSpec spec) {
        if (spec.split.id != 0) {
            (void) require_window(spec.split.id);
        }
        auto opened = host_.open_window(spec);
        const auto stream = allocate_stream(StreamRecord{
            .backing = HostStream{.stream = std::move(opened.stream)},
            .rock = 0,
        });
        const auto window = allocate_window(WindowRecord{
            .window = std::move(opened.window),
            .rock = spec.rock,
            .type = spec.type,
            .stream = stream,
        });
        if (root_window_.id == 0) {
            root_window_ = window;
        }
        return window;
    }

    GlkStreamHandle open_memory_stream(GlkMemoryStream stream, u32 rock) {
        validate_memory_stream_mode(stream.mode);
        return allocate_stream(StreamRecord{
            .backing = stream,
            .rock = rock,
        });
    }

    GlkStreamHandle open_memory_stream(GlkUnicodeMemoryStream stream, u32 rock) {
        validate_memory_stream_mode(stream.mode);
        return allocate_stream(StreamRecord{
            .backing = stream,
            .rock = rock,
        });
    }

    static void validate_memory_stream_mode(u32 mode) {
        if (mode != 0 && mode != 1 && mode != 2 && mode != 3) {
            throw std::runtime_error(
                std::format("unsupported memory stream mode {}", mode));
        }
    }

    void close_stream(Machine& machine,
                      GlkStreamHandle handle,
                      u32 result_address) {
        auto& stream = require_stream(handle.id);
        if (std::holds_alternative<HostStream>(stream.backing)) {
            throw std::runtime_error("stream_close cannot close a window stream");
        }
        write_event_field(machine, result_address, 0, stream.read_count);
        write_event_field(machine, result_address, 1, stream.write_count);
        stream = {};
        if (current_stream_.id == handle.id) {
            current_stream_ = {};
        }
    }

    void set_current_stream(GlkStreamHandle stream) {
        if (stream.id != 0) {
            (void) require_stream(stream.id);
        }
        current_stream_ = stream;
    }

    void set_window(GlkWindowHandle window) {
        current_stream_ =
            window.id == 0 ? GlkStreamHandle{} : require_window(window.id).stream;
    }

    u32 iterate_windows(Machine& machine, u32 previous_id, u32 rock_address) {
        auto return_next = previous_id == 0;
        auto found_previous = previous_id == 0;
        for (auto index = std::size_t{0}; index < windows_.size(); ++index) {
            const auto& window = windows_[index];
            if (!window) {
                continue;
            }
            const auto id = static_cast<u32>(index + 1);
            if (return_next) {
                write_glk_ref(machine, rock_address, window->rock);
                return id;
            }
            if (id == previous_id) {
                found_previous = true;
                return_next = true;
            }
        }
        if (!found_previous) {
            throw std::runtime_error(std::format(
                "window_iterate received invalid window {}", previous_id));
        }
        write_glk_ref(machine, rock_address, 0);
        return 0;
    }

    u32 iterate_streams(Machine& machine, u32 previous_id, u32 rock_address) {
        auto return_next = previous_id == 0;
        auto found_previous = previous_id == 0;
        for (auto index = std::size_t{0}; index < streams_.size(); ++index) {
            const auto& stream = streams_[index];
            if (!stream || !stream->allocated()) {
                continue;
            }
            const auto id = static_cast<u32>(index + 1);
            if (return_next) {
                write_glk_ref(machine, rock_address, stream->rock);
                return id;
            }
            if (id == previous_id) {
                found_previous = true;
                return_next = true;
            }
        }
        if (!found_previous) {
            throw std::runtime_error(std::format(
                "stream_iterate received invalid stream {}", previous_id));
        }
        write_glk_ref(machine, rock_address, 0);
        return 0;
    }

    GlkCallResult write(Machine& machine, GlkText text) {
        return write_to(machine, current_stream_, std::move(text));
    }

    GlkCallResult write_to(Machine& machine,
                           GlkStreamHandle stream,
                           GlkText text) {
        if (stream.id == 0) {
            return glk_returned();
        }
        return write_to(machine, stream, materialize_text(machine, text));
    }

    GlkCallResult write_to(Machine& machine,
                           GlkStreamHandle handle,
                           const GlkTextData& text) {
        if (handle.id == 0) {
            return glk_returned();
        }
        auto& stream = require_stream(handle.id);
        if (auto* host_stream = std::get_if<HostStream>(&stream.backing)) {
            auto result = host_.write(host_stream->stream, text);
            if (std::holds_alternative<GlkReturned>(result)) {
                stream.write_count += text_length(text);
            }
            return result;
        }
        if (auto* memory = std::get_if<GlkMemoryStream>(&stream.backing)) {
            write_to_memory_stream(machine, stream, *memory, text);
            return glk_returned();
        }
        if (auto* memory = std::get_if<GlkUnicodeMemoryStream>(&stream.backing)) {
            write_to_unicode_memory_stream(machine, stream, *memory, text);
            return glk_returned();
        }
        throw std::runtime_error(
            std::format("invalid Glk stream handle {}", handle.id));
    }

    static void write_to_memory_stream(Machine& machine,
                                       StreamRecord& record,
                                       const GlkMemoryStream& stream,
                                       const GlkTextData& text) {
        if (stream.mode == 2) {
            throw std::runtime_error("cannot write to read-only memory stream");
        }
        if (const auto* bytes = std::get_if<std::string>(&text)) {
            for (const auto ch : *bytes) {
                write_codepoint_to_memory_stream(machine, record, stream,
                                                 static_cast<u8>(ch));
            }
            return;
        }
        for (const auto ch : std::get<std::vector<u32>>(text)) {
            write_codepoint_to_memory_stream(machine, record, stream, ch);
        }
    }

    static void write_codepoint_to_memory_stream(Machine& machine,
                                                StreamRecord& record,
                                                const GlkMemoryStream& stream,
                                                u32 ch) {
        if (record.pos < stream.len && stream.address != 0) {
            // Memory streams store bytes; out-of-range Unicode is truncated just
            // as Glk's byte-oriented memory streams specify.
            machine.memory.write8(stream.address + record.pos, static_cast<u8>(ch));
        }
        ++record.pos;
        ++record.write_count;
    }

    static void write_to_unicode_memory_stream(Machine& machine,
                                               StreamRecord& record,
                                               const GlkUnicodeMemoryStream& stream,
                                               const GlkTextData& text) {
        if (stream.mode == 2) {
            throw std::runtime_error(
                "cannot write to read-only unicode memory stream");
        }
        if (const auto* bytes = std::get_if<std::string>(&text)) {
            for (const auto ch : *bytes) {
                write_codepoint_to_unicode_memory_stream(
                    machine, record, stream, static_cast<u8>(ch));
            }
            return;
        }
        for (const auto ch : std::get<std::vector<u32>>(text)) {
            write_codepoint_to_unicode_memory_stream(machine, record, stream, ch);
        }
    }

    static void write_codepoint_to_unicode_memory_stream(
        Machine& machine,
        StreamRecord& record,
        const GlkUnicodeMemoryStream& stream,
        u32 ch) {
        if (record.pos < stream.len && stream.address != 0) {
            machine.memory.write32(stream.address + record.pos * 4, ch);
        }
        ++record.pos;
        ++record.write_count;
    }

    void request_line_event(Machine& machine,
                            GlkWindowHandle window,
                            u32 buffer_address,
                            u32 max_length,
                            u32 initial_length,
                            GlkTextEncoding encoding) {
        (void) require_window(window.id);
        pending_line_ = PendingLine{
            .window = window,
            .buffer_address = buffer_address,
            .max_length = max_length,
            .encoding = encoding,
        };
        event_interests_.clear();
        event_interests_.push_back(GlkLineInputRequest{
            .window = window,
            .max_length = max_length,
            .encoding = encoding,
            .initial_text = materialize_text(
                machine, GlkTextBuffer{
                             .address = buffer_address,
                             .length = initial_length,
                             .encoding = encoding,
                         }),
        });
    }

    GlkCallResult select(Machine& machine, u32 event_address) {
        const auto result = host_.select(
            GlkEventRequest{.interests = span<const GlkEventInterest>{
                                event_interests_.data(), event_interests_.size()}});
        if (std::holds_alternative<GlkBlocked>(result)) {
            return glk_blocked();
        }
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            return glk_fatal(fatal->message);
        }
        return write_event(machine, event_address, std::get<GlkHostEvent>(result));
    }

    struct PendingLine {
        GlkWindowHandle window = {};
        u32 buffer_address = 0;
        u32 max_length = 0;
        GlkTextEncoding encoding = GlkTextEncoding::latin1;
    };

    GlkCallResult write_event(Machine& machine,
                              u32 event_address,
                              const GlkHostEvent& event) {
        if (const auto* line = std::get_if<GlkLineInputEvent>(&event)) {
            return write_line_event(machine, event_address, *line);
        }
        if (const auto* ch = std::get_if<GlkCharInputEvent>(&event)) {
            write_event_field(machine, event_address, 0, 2);
            write_event_field(machine, event_address, 1, ch->window.id);
            write_event_field(machine, event_address, 2, ch->value);
            write_event_field(machine, event_address, 3, 0);
            return glk_returned();
        }
        write_event_field(machine, event_address, 0, 1);
        write_event_field(machine, event_address, 1, 0);
        write_event_field(machine, event_address, 2, 0);
        write_event_field(machine, event_address, 3, 0);
        return glk_returned();
    }

    GlkCallResult write_line_event(Machine& machine,
                                   u32 event_address,
                                   const GlkLineInputEvent& event) {
        if (!pending_line_ || pending_line_->window.id != event.window.id) {
            return glk_fatal("line input event without matching request");
        }

        const auto count = write_input_text(machine, *pending_line_, event.text);
        write_event_field(machine, event_address, 0, 3);
        write_event_field(machine, event_address, 1, event.window.id);
        write_event_field(machine, event_address, 2, count);
        write_event_field(machine, event_address, 3, 0);
        pending_line_.reset();
        event_interests_.clear();

        if (host_.echo_line_input()) {
            if (auto echo = echo_line(machine, event.window, event.text);
                !std::holds_alternative<GlkReturned>(echo)) {
                return echo;
            }
        }
        return glk_returned();
    }

    GlkCallResult echo_line(Machine& machine,
                            GlkWindowHandle window,
                            const GlkInputText& text) {
        const auto stream = require_window(window.id).stream;
        if (auto result = write_to(machine, stream, text);
            !std::holds_alternative<GlkReturned>(result)) {
            return result;
        }
        if (const auto* bytes = std::get_if<std::string>(&text)) {
            (void) bytes;
            return write_to(machine, stream, std::string{"\n"});
        }
        return write_to(machine, stream, std::vector<u32>{'\n'});
    }

    static u32 write_input_text(Machine& machine,
                                const PendingLine& pending,
                                const GlkInputText& text) {
        if (const auto* latin1 = std::get_if<std::string>(&text)) {
            const auto count = std::min<u32>(
                pending.max_length, static_cast<u32>(latin1->size()));
            for (u32 index = 0; index < count; ++index) {
                const auto ch = static_cast<u8>((*latin1)[index]);
                if (pending.encoding == GlkTextEncoding::unicode) {
                    machine.memory.write32(pending.buffer_address + index * 4, ch);
                } else {
                    machine.memory.write8(pending.buffer_address + index, ch);
                }
            }
            return count;
        }

        const auto& unicode = std::get<std::vector<u32>>(text);
        const auto count =
            std::min<u32>(pending.max_length, static_cast<u32>(unicode.size()));
        for (u32 index = 0; index < count; ++index) {
            if (pending.encoding == GlkTextEncoding::unicode) {
                machine.memory.write32(pending.buffer_address + index * 4,
                                       unicode[index]);
            } else {
                machine.memory.write8(pending.buffer_address + index,
                                      static_cast<u8>(unicode[index]));
            }
        }
        return count;
    }

    Host host_;
    GlkWindowHandle root_window_ = {};
    GlkStreamHandle current_stream_ = {};
    std::vector<std::unique_ptr<WindowRecord>> windows_;
    std::vector<std::unique_ptr<StreamRecord>> streams_;
    std::vector<GlkEventInterest> event_interests_;
    std::optional<PendingLine> pending_line_;
};

}  // namespace glupsk
