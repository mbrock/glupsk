#pragma once

#include "core/glk_runtime.hpp"

#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glupsk {

struct StdioGlkHost {
    struct Window {
        u32 type = 0;
    };

    struct Stream {
        bool visible = true;
    };

    struct FileRef {};

    std::ostream* output = &std::cout;
    std::deque<std::string> input_lines;
    bool waiting_for_line_input = false;

    void add_input_line(std::string line) {
        input_lines.push_back(std::move(line));
        waiting_for_line_input = false;
    }

    bool wants_line_input() const {
        return waiting_for_line_input;
    }

    u32 gestalt(GlkGestaltQuery query) {
        return glk_default_gestalt(query);
    }

    GlkOpenedWindow<StdioGlkHost> open_window(GlkWindowSpec spec) {
        return {
            .window = Window{.type = spec.type},
            .stream = Stream{.visible = spec.type !=
                                          static_cast<u32>(
                                              GlkWindowType::text_grid)},
        };
    }

    GlkWindowSize window_size(Window&) {
        return {};
    }

    void window_clear(Window&) {}

    void window_move_cursor(Window&, u32, u32) {}

    GlkCallResult write(Stream& stream, const GlkTextData& text) {
        if (!stream.visible) {
            return glk_returned();
        }
        std::visit([&](const auto& value) { write_text(value); }, text);
        output->flush();
        return glk_returned();
    }

    GlkEventResult select(GlkEventRequest request) {
        for (const auto& interest : request.interests) {
            if (const auto* line =
                    std::get_if<GlkLineInputRequest>(&interest)) {
                if (input_lines.empty()) {
                    waiting_for_line_input = true;
                    return GlkBlocked{};
                }
                auto text = std::move(input_lines.front());
                input_lines.pop_front();
                waiting_for_line_input = false;
                return GlkHostEvent{GlkLineInputEvent{
                    .window = line->window,
                    .text = std::move(text),
                }};
            }
        }
        waiting_for_line_input = false;
        return GlkBlocked{};
    }

    bool echo_line_input() {
        return false;
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

class StdioGlk : public GlkSession<StdioGlkHost> {
  public:
    using GlkSession<StdioGlkHost>::GlkSession;

    void add_input_line(std::string line) {
        host().add_input_line(std::move(line));
    }

    bool wants_line_input() const {
        return host().wants_line_input();
    }
};

}  // namespace glupsk
