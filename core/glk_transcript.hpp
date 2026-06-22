#pragma once

#include "core/glk_bridge.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace glupsk {

struct TranscriptGlkHost {
    struct Window {
        u32 rock = 0;
        u32 type = 0;
        GlkStreamHandle stream = {};
    };

    struct Stream {
        u32 rock = 0;
        std::string text;
    };

    struct FileRef {
        u32 rock = 0;
    };

    struct Write {
        GlkStreamHandle stream = {};
        GlkTextData text;
    };

    std::deque<GlkInputText> input_lines;
    std::vector<Write> writes;
    std::string text;
    GlkWindowHandle root_window = {};

    void add_input_line(std::string line) {
        input_lines.push_back(std::move(line));
    }

    u32 gestalt(u32 selector, u32) {
        switch (static_cast<GlkGestaltSelector>(selector)) {
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

    template <typename Registry>
    GlkWindowHandle window_open(Registry& registry,
                                GlkWindowHandle,
                                u32,
                                u32,
                                u32 type,
                                u32 rock) {
        const auto stream = registry.add_stream(Stream{});
        const auto window = registry.add_window(Window{
            .rock = rock,
            .type = type,
            .stream = stream,
        });
        if (root_window.id == 0) {
            root_window = window;
        }
        return window;
    }

    template <typename Registry>
    GlkWindowHandle window_get_root(Registry&) {
        return root_window;
    }

    template <typename Registry>
    u32 window_get_rock(Registry& registry, GlkWindowHandle window) {
        return registry.require_window(window).rock;
    }

    template <typename Registry>
    GlkStreamHandle window_get_stream(Registry& registry,
                                      GlkWindowHandle window) {
        return registry.require_window(window).stream;
    }

    template <typename Registry>
    u32 stream_get_rock(Registry& registry, GlkStreamHandle stream) {
        return registry.require_stream(stream).rock;
    }

    template <typename Registry>
    GlkCallResult write(Registry& registry,
                        GlkStreamHandle stream,
                        const GlkTextData& data) {
        auto& native_stream = registry.require_stream(stream);
        append(native_stream.text, data);
        append(text, data);
        writes.push_back(Write{.stream = stream, .text = data});
        return glk_returned();
    }

    template <typename Registry>
    GlkEventResult select(Registry& registry, GlkEventRequest request) {
        (void) registry;
        for (const auto& interest : request.interests) {
            if (const auto* line =
                    std::get_if<GlkLineInputRequest>(&interest)) {
                if (input_lines.empty()) {
                    return GlkBlocked{};
                }
                auto text = std::move(input_lines.front());
                input_lines.pop_front();
                return GlkHostEvent{GlkLineInputEvent{
                    .window = line->window,
                    .text = std::move(text),
                }};
            }
        }
        return GlkBlocked{};
    }

  private:
    static void append(std::string& out, const GlkTextData& data) {
        std::visit([&](const auto& value) { append(out, value); }, data);
    }

    static void append(std::string& out, const std::string& text) {
        append(out, std::string_view{text});
    }

    static void append(std::string& out, std::string_view text) {
        for (auto ch : text) {
            out.push_back(ch == '\r' ? '\n' : ch);
        }
    }

    static void append(std::string& out, const std::vector<u32>& text) {
        for (auto ch : text) {
            if (ch == '\r') {
                ch = '\n';
            }
            out.push_back(ch <= 0x7fu ? static_cast<char>(ch) : '?');
        }
    }
};

static_assert(SemanticGlkHost<
              TranscriptGlkHost,
              GlkObjectRegistry<TranscriptGlkHost::Window,
                                TranscriptGlkHost::Stream,
                                TranscriptGlkHost::FileRef>>);

}  // namespace glupsk
