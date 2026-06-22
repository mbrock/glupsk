#pragma once

#include "core/error.hpp"
#include "core/glk.hpp"
#include "core/glk_dispatch.hpp"
#include "core/glk_events.hpp"
#include "core/glk_registry.hpp"
#include "core/glk_streams.hpp"
#include "core/glk_text.hpp"
#include "core/machine.hpp"

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
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
        { host.write(stream, text, GlkStyle::normal) } -> std::same_as<GlkCallResult>;
        { host.stylehint_set(value, GlkStyle::normal, value, value) } -> std::same_as<void>;
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
        // Every @glk call is an observe point: drain buffered per-character
        // output (under the style/stream current at this instant) before the
        // call's own effect — input, window changes, style changes, or its own
        // write-through output — can be observed out of order.
        flush(machine);
        const auto raw = GlkArgs{args};
        switch (static_cast<GlkSelector>(selector)) {
            case GlkSelector::exit:
                machine.halted = true;
                machine.running = false;
                return glk_returned();
            case GlkSelector::gestalt:
                return glk_returned(raw.empty() ? 0
                                                : host_.gestalt(raw.gestalt_query()));
            case GlkSelector::window_iterate:
                return glk_returned(registry_.iterate_windows(
                    machine, raw.get(0), raw.get(1)));
            case GlkSelector::window_get_rock:
                return glk_returned(raw.empty() ? 0
                                                : registry_.require_window(raw.get(0)).rock);
            case GlkSelector::window_get_root:
                return glk_returned(root_window_.id);
            case GlkSelector::window_open:
                return glk_returned(open_window(raw.window_spec()).id);
            case GlkSelector::window_get_size:
                if (raw.size() >= 3) {
                    const auto size =
                        host_.window_size(registry_.require_window(raw.get(0)).window);
                    glk_write_ref(machine, raw.get(1), size.width);
                    glk_write_ref(machine, raw.get(2), size.height);
                }
                return glk_returned();
            case GlkSelector::window_clear:
                if (!raw.empty()) {
                    host_.window_clear(registry_.require_window(raw.get(0)).window);
                }
                return glk_returned();
            case GlkSelector::window_move_cursor:
                if (raw.size() >= 3) {
                    host_.window_move_cursor(
                        registry_.require_window(raw.get(0)).window, raw.get(1),
                        raw.get(2));
                }
                return glk_returned();
            case GlkSelector::window_get_stream:
                return glk_returned(raw.empty()
                                        ? 0
                                        : registry_.require_window(raw.get(0)).stream.id);
            case GlkSelector::set_window:
                set_window(raw.empty() ? GlkWindowHandle{} : raw.window(0));
                return glk_returned();
            case GlkSelector::stream_iterate:
                return glk_returned(registry_.iterate_streams(
                    machine, raw.get(0), raw.get(1)));
            case GlkSelector::stream_get_rock:
                return glk_returned(raw.empty() ? 0
                                                : registry_.require_stream(raw.get(0)).rock);
            case GlkSelector::stream_open_memory:
                return glk_returned(
                    open_memory_stream(raw.memory_stream(), raw.get(3)).id);
            case GlkSelector::stream_open_memory_uni:
                return glk_returned(
                    open_memory_stream(raw.unicode_memory_stream(), raw.get(3)).id);
            case GlkSelector::stream_close:
                if (!raw.empty()) {
                    close_stream(machine, raw.stream(0), raw.get(1));
                }
                return glk_returned();
            case GlkSelector::stream_set_current:
                set_current_stream(raw.empty() ? GlkStreamHandle{} : raw.stream(0));
                return glk_returned();
            case GlkSelector::stream_get_current:
                return glk_returned(current_stream_.id);
            case GlkSelector::fileref_iterate:
                if (raw.size() >= 2) {
                    glk_write_ref(machine, raw.get(1), 0);
                }
                return glk_returned();
            case GlkSelector::fileref_get_rock:
                if (!raw.empty() && raw.get(0) != 0) {
                    fail("fileref_get_rock received invalid fileref");
                }
                return glk_returned();
            case GlkSelector::set_style:
                current_style_ = raw.empty() ? GlkStyle::normal
                                             : static_cast<GlkStyle>(raw.get(0));
                return glk_returned();
            case GlkSelector::stylehint_set:
                if (raw.size() >= 4) {
                    host_.stylehint_set(raw.get(0), static_cast<GlkStyle>(raw.get(1)),
                                        raw.get(2), raw.get(3));
                }
                return glk_returned();
            case GlkSelector::put_char:
                return raw.empty() ? glk_returned()
                                    : write(machine, GlkTextChar{.value = raw.get(0)});
            case GlkSelector::put_char_uni:
                return raw.empty()
                           ? glk_returned()
                           : write(machine, GlkTextChar{
                                                .value = raw.get(0),
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_char_stream:
                return raw.size() < 2
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextChar{.value = raw.get(1)});
            case GlkSelector::put_char_stream_uni:
                return raw.size() < 2
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextChar{
                                          .value = raw.get(1),
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::put_string:
                return raw.empty()
                           ? glk_returned()
                           : write(machine, GlkTextString{.address = raw.get(0) + 1});
            case GlkSelector::put_string_stream:
                return raw.size() < 2
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextString{.address = raw.get(1) + 1});
            case GlkSelector::put_string_uni:
                return raw.empty()
                           ? glk_returned()
                           : write(machine, GlkTextString{
                                                .address = raw.get(0) + 4,
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_string_stream_uni:
                return raw.size() < 2
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextString{
                                          .address = raw.get(1) + 4,
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::put_buffer:
                return raw.size() < 2
                           ? glk_returned()
                           : write(machine, GlkTextBuffer{.address = raw.get(0),
                                                          .length = raw.get(1)});
            case GlkSelector::put_buffer_stream:
                return raw.size() < 3
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextBuffer{.address = raw.get(1),
                                                    .length = raw.get(2)});
            case GlkSelector::put_buffer_uni:
                return raw.size() < 2
                           ? glk_returned()
                           : write(machine, GlkTextBuffer{
                                                .address = raw.get(0),
                                                .length = raw.get(1),
                                                .encoding = GlkTextEncoding::unicode,
                                            });
            case GlkSelector::put_buffer_stream_uni:
                return raw.size() < 3
                           ? glk_returned()
                           : write_to(machine, raw.stream(0),
                                      GlkTextBuffer{
                                          .address = raw.get(1),
                                          .length = raw.get(2),
                                          .encoding = GlkTextEncoding::unicode,
                                      });
            case GlkSelector::buffer_to_lower_case_uni:
                return glk_returned(raw.size() >= 3
                                        ? glk_transform_unicode_buffer(
                                              machine, raw.get(0), raw.get(1), raw.get(2),
                                              glk_char_to_lower)
                                        : 0);
            case GlkSelector::buffer_to_upper_case_uni:
                return glk_returned(raw.size() >= 3
                                        ? glk_transform_unicode_buffer(
                                              machine, raw.get(0), raw.get(1), raw.get(2),
                                              glk_char_to_upper)
                                        : 0);
            case GlkSelector::buffer_to_title_case_uni:
                return glk_returned(raw.size() >= 4
                                        ? glk_title_case_unicode_buffer(
                                              machine, raw.get(0), raw.get(1), raw.get(2),
                                              raw.get(3) != 0)
                                        : 0);
            case GlkSelector::buffer_canon_decompose_uni:
            case GlkSelector::buffer_canon_normalize_uni:
                return glk_returned(raw.size() >= 3 ? raw.get(2) : 0);
            case GlkSelector::request_line_event:
                if (raw.size() >= 3) {
                    request_line_event(machine, raw.window(0),
                                       raw.get(1), raw.get(2),
                                       raw.get(3),
                                       GlkTextEncoding::latin1);
                }
                return glk_returned();
            case GlkSelector::request_line_event_uni:
                if (raw.size() >= 3) {
                    request_line_event(machine, raw.window(0),
                                       raw.get(1), raw.get(2),
                                       raw.get(3),
                                       GlkTextEncoding::unicode);
                }
                return glk_returned();
            case GlkSelector::select:
                return select(machine, raw.get(0));
            case GlkSelector::char_to_lower:
                return glk_returned(raw.empty() ? 0 : glk_char_to_lower(raw.get(0)));
            case GlkSelector::char_to_upper:
                return glk_returned(raw.empty() ? 0 : glk_char_to_upper(raw.get(0)));
            default:
                fail("unsupported Glk selector");
        }
    }

    u32 call_returned(Machine& machine, u32 selector, span<const u32> args) {
        const auto result = call(machine, selector, args);
        if (const auto* returned = std::get_if<GlkReturned>(&result)) {
            return returned->value;
        }
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            fail(fatal->message);
        }
        fail("Glk call blocked");
    }

    // The VM string decoder emits output one codepoint at a time. For host
    // streams we enqueue into the stream's ring and let flush() coalesce a whole
    // string into a single host.write; other backings fall back to write-through.
    void put_char(Machine& machine, u32 ch) override {
        if (current_stream_.id != 0) {
            auto& record = registry_.require_stream(current_stream_.id);
            if (auto* host_stream = std::get_if<HostStream>(&record.backing);
                host_stream && record.buffer.capacity() != 0) {
                if (record.buffer.full()) {
                    drain(record, *host_stream);
                }
                record.buffer.push(ch);
                ++record.write_count;
                return;
            }
        }
        const auto result = write(machine, GlkTextChar{
                                               .value = ch,
                                               .encoding = GlkTextEncoding::unicode,
                                           });
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            fail(fatal->message);
        }
        if (std::holds_alternative<GlkBlocked>(result)) {
            fail("Glk output blocked outside @glk");
        }
    }

    void flush(Machine&) override {
        if (current_stream_.id == 0) {
            return;
        }
        auto& record = registry_.require_stream(current_stream_.id);
        if (auto* host_stream = std::get_if<HostStream>(&record.backing)) {
            drain(record, *host_stream);
        }
    }

  private:
    using WindowRecord = GlkWindowRecord<Host>;
    using StreamRecord = GlkStreamRecord<Host>;
    using HostStream = GlkHostStream<Host>;

    static constexpr std::size_t kHostStreamBufferCapacity = 512;

    // Drain a host stream's ring to its backend as one styled run. The chars
    // were counted into write_count at push time, so draining does not recount.
    void drain(StreamRecord& record, HostStream& host_stream) {
        if (record.buffer.empty()) {
            return;
        }
        auto codepoints = std::vector<u32>{};
        codepoints.reserve(record.buffer.size());
        while (!record.buffer.empty()) {
            const auto run = record.buffer.readable();
            codepoints.insert(codepoints.end(), run.begin(), run.end());
            record.buffer.consume(run.size());
        }
        const auto result = host_.write(
            host_stream.stream, GlkTextData{std::move(codepoints)}, current_style_);
        if (const auto* fatal = std::get_if<GlkFatal>(&result)) {
            fail(fatal->message);
        }
    }

    GlkWindowHandle open_window(GlkWindowSpec spec) {
        if (spec.split.id != 0) {
            (void) registry_.require_window(spec.split.id);
        }
        auto opened = host_.open_window(spec);
        const auto stream = registry_.allocate_stream(StreamRecord{
            .backing = HostStream{.stream = std::move(opened.stream)},
            .rock = 0,
            .buffer = Ring<u32>{kHostStreamBufferCapacity},
        });
        const auto window = registry_.allocate_window(WindowRecord{
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
        glk_validate_memory_stream_mode(stream.mode);
        return registry_.allocate_stream(StreamRecord{
            .backing = stream,
            .rock = rock,
        });
    }

    GlkStreamHandle open_memory_stream(GlkUnicodeMemoryStream stream, u32 rock) {
        glk_validate_memory_stream_mode(stream.mode);
        return registry_.allocate_stream(StreamRecord{
            .backing = stream,
            .rock = rock,
        });
    }

    // Current-stream operations stay here because they bind handles, window
    // streams, and stream result writeback to this session's registry.
    void close_stream(Machine& machine,
                      GlkStreamHandle handle,
                      u32 result_address) {
        auto& stream = registry_.require_stream(handle.id);
        if (std::holds_alternative<HostStream>(stream.backing)) {
            fail("stream_close cannot close a window stream");
        }
        glk_write_event_field(machine, result_address, 0, stream.read_count);
        glk_write_event_field(machine, result_address, 1, stream.write_count);
        stream = {};
        if (current_stream_.id == handle.id) {
            current_stream_ = {};
        }
    }

    void set_current_stream(GlkStreamHandle stream) {
        if (stream.id != 0) {
            (void) registry_.require_stream(stream.id);
        }
        current_stream_ = stream;
    }

    void set_window(GlkWindowHandle window) {
        current_stream_ =
            window.id == 0 ? GlkStreamHandle{} : registry_.require_window(window.id).stream;
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
        return write_to(machine, stream, glk_materialize_text(machine, text));
    }

    GlkCallResult write_to(Machine& machine,
                           GlkStreamHandle handle,
                           const GlkTextData& text) {
        if (handle.id == 0) {
            return glk_returned();
        }
        auto& stream = registry_.require_stream(handle.id);
        return glk_write_to_stream_record(
            host_, machine, stream, text, current_style_);
    }

    // Select orchestration is the point where host events become VM-visible
    // Glk event structs and, for line input, optional echo writes.
    void request_line_event(Machine& machine,
                            GlkWindowHandle window,
                            u32 buffer_address,
                            u32 max_length,
                            u32 initial_length,
                            GlkTextEncoding encoding) {
        (void) registry_.require_window(window.id);
        pending_line_ = GlkPendingLine{
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
            .initial_text = glk_materialize_text(
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

    GlkCallResult write_event(Machine& machine,
                              u32 event_address,
                              const GlkHostEvent& event) {
        if (const auto* line = std::get_if<GlkLineInputEvent>(&event)) {
            return write_line_event(machine, event_address, *line);
        }
        if (const auto* ch = std::get_if<GlkCharInputEvent>(&event)) {
            glk_write_event_field(machine, event_address, 0, 2);
            glk_write_event_field(machine, event_address, 1, ch->window.id);
            glk_write_event_field(machine, event_address, 2, ch->value);
            glk_write_event_field(machine, event_address, 3, 0);
            return glk_returned();
        }
        if (std::holds_alternative<GlkArrangeEvent>(event)) {
            glk_write_event_field(machine, event_address, 0, 5);
            glk_write_event_field(machine, event_address, 1, 0);
            glk_write_event_field(machine, event_address, 2, 0);
            glk_write_event_field(machine, event_address, 3, 0);
            return glk_returned();
        }
        glk_write_event_field(machine, event_address, 0, 1);
        glk_write_event_field(machine, event_address, 1, 0);
        glk_write_event_field(machine, event_address, 2, 0);
        glk_write_event_field(machine, event_address, 3, 0);
        return glk_returned();
    }

    GlkCallResult write_line_event(Machine& machine,
                                   u32 event_address,
                                   const GlkLineInputEvent& event) {
        if (!pending_line_ || pending_line_->window.id != event.window.id) {
            return glk_fatal("line input event without matching request");
        }

        const auto count = glk_write_input_text(machine, *pending_line_, event.text);
        glk_write_event_field(machine, event_address, 0, 3);
        glk_write_event_field(machine, event_address, 1, event.window.id);
        glk_write_event_field(machine, event_address, 2, count);
        glk_write_event_field(machine, event_address, 3, 0);
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
        const auto stream = registry_.require_window(window.id).stream;
        const auto previous_style = current_style_;
        current_style_ = GlkStyle::input;
        auto result = write_to(machine, stream, text);
        if (std::holds_alternative<GlkReturned>(result)) {
            result = std::holds_alternative<std::string>(text)
                         ? write_to(machine, stream, std::string{"\n"})
                         : write_to(machine, stream, std::vector<u32>{'\n'});
        }
        current_style_ = previous_style;
        return result;
    }

    Host host_;
    GlkRegistry<Host> registry_;
    GlkWindowHandle root_window_ = {};
    GlkStreamHandle current_stream_ = {};
    GlkStyle current_style_ = GlkStyle::normal;
    std::vector<GlkEventInterest> event_interests_;
    std::optional<GlkPendingLine> pending_line_;
};

}  // namespace glupsk
