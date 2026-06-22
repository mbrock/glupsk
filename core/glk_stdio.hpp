#pragma once

#include "core/glk_runtime.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace glupsk {

struct StdioGlkHost {
    struct Window {};
    struct Stream {};
    struct FileRef {};

    std::istream* input = &std::cin;
    std::ostream* output = &std::cout;

    u32 gestalt(GlkGestaltQuery query) {
        return glk_default_gestalt(query);
    }

    GlkOpenedWindow<StdioGlkHost> open_window(GlkWindowSpec) {
        return {
            .window = Window{},
            .stream = Stream{},
        };
    }

    GlkWindowSize window_size(Window&) {
        return {};
    }

    void window_clear(Window&) {}

    void window_move_cursor(Window&, u32, u32) {}

    GlkCallResult write(Stream&, const GlkTextData& text) {
        std::visit([&](const auto& value) { write_text(value); }, text);
        output->flush();
        return glk_returned();
    }

    GlkEventResult select(GlkEventRequest request) {
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

static_assert(GlkHost<StdioGlkHost>);

using StdioGlk = GlkSession<StdioGlkHost>;

}  // namespace glupsk
