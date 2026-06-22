#pragma once

#include "core/glk_bridge.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace glupsk {

struct StdioGlkHost {
    struct Window {
        u32 rock = 0;
        GlkStreamHandle stream = {};
    };

    struct Stream {
        u32 rock = 0;
    };

    struct FileRef {
        u32 rock = 0;
    };

    using Registry = GlkRegistry<StdioGlkHost>;

    std::istream* input = &std::cin;
    std::ostream* output = &std::cout;
    GlkWindowHandle root_window = {};

    u32 gestalt(GlkGestaltQuery query) {
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

    GlkWindowHandle window_open(Registry& registry,
                                GlkWindowHandle,
                                u32,
                                u32,
                                u32,
                                u32 rock) {
        const auto stream = registry.add_stream(Stream{});
        const auto window = registry.add_window(Window{
            .rock = rock,
            .stream = stream,
        });
        if (root_window.id == 0) {
            root_window = window;
        }
        return window;
    }

    GlkWindowHandle window_get_root(Registry&) {
        return root_window;
    }

    u32 window_get_rock(Registry& registry, GlkWindowHandle window) {
        return registry.require_window(window).rock;
    }

    GlkStreamHandle window_get_stream(Registry& registry,
                                      GlkWindowHandle window) {
        return registry.require_window(window).stream;
    }

    u32 stream_get_rock(Registry& registry, GlkStreamHandle stream) {
        return registry.require_stream(stream).rock;
    }

    GlkCallResult write(Registry& registry,
                        GlkStreamHandle stream,
                        const GlkTextData& text) {
        (void) registry.require_stream(stream);
        std::visit([&](const auto& value) { write_text(value); }, text);
        output->flush();
        return glk_returned();
    }

    GlkEventResult select(Registry& registry,
                          GlkEventRequest request) {
        (void) registry;
        for (const auto& interest : request.interests) {
            if (const auto* line =
                    std::get_if<GlkLineInputRequest>(&interest)) {
                auto text = std::string{};
                if (!std::getline(*input, text)) {
                    return GlkFatal{.message = "stdio input stream ended"};
                }
                return GlkHostEvent{GlkLineInputEvent{
                    .window = line->window,
                    .text = std::move(text),
                }};
            }
        }
        return GlkBlocked{};
    }

  private:
    void write_text(std::string_view text) {
        for (const auto ch : text) {
            write_codepoint(static_cast<u8>(ch));
        }
    }

    void write_text(const std::vector<u32>& text) {
        for (const auto ch : text) {
            write_codepoint(ch);
        }
    }

    void write_codepoint(u32 ch) {
        if (ch == '\r') {
            ch = '\n';
        }
        *output << (ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
};

static_assert(SemanticGlkHost<StdioGlkHost>);

}  // namespace glupsk
