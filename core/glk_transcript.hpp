#pragma once

#include "core/glk_runtime.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glupsk {

struct TranscriptGlkHost {
    struct Window {};

    struct Stream {
        std::string text;
    };

    struct FileRef {};

    struct Write {
        GlkTextData text;
        GlkStyle style = GlkStyle::normal;
    };

    std::deque<GlkInputText> input_lines;
    std::vector<Write> writes;
    std::string text;

    void add_input_line(std::string line) {
        input_lines.push_back(std::move(line));
    }

    u32 gestalt(GlkGestaltQuery query) {
        return glk_default_gestalt(query);
    }

    GlkOpenedWindow<TranscriptGlkHost> open_window(GlkWindowSpec) {
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

    GlkCallResult write(Stream& stream,
                        const GlkTextData& data,
                        GlkStyle style) {
        append(stream.text, data);
        append(text, data);
        writes.push_back(Write{.text = data, .style = style});
        return glk_returned();
    }

    GlkEventResult select(GlkEventRequest request) {
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

    bool echo_line_input() {
        return true;
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

static_assert(GlkHost<TranscriptGlkHost>);

class TranscriptGlk : public GlkSession<TranscriptGlkHost> {
  public:
    TranscriptGlk() : GlkSession<TranscriptGlkHost>{}, transcript(host().text) {}

    void add_input_line(std::string line) {
        host().add_input_line(std::move(line));
    }

    std::string& transcript;
};

}  // namespace glupsk
