#pragma once

#include "core/glk_bridge.hpp"

#include <iostream>
#include <string>

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

    std::istream* input = &std::cin;
    std::ostream* output = &std::cout;
    GlkWindowHandle root_window = {};

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
    GlkCallResult write(Machine& machine,
                        Registry& registry,
                        GlkStreamHandle stream,
                        GlkText text) {
        (void) registry.require_stream(stream);
        std::visit([&](const auto& value) { write_text(machine, value); }, text);
        output->flush();
        return glk_returned();
    }

    template <typename Registry>
    GlkEventResult select(Machine&,
                          Registry& registry,
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
    void write_text(Machine& machine, const GlkTextChar& text) {
        (void) machine;
        write_codepoint(text.value);
    }

    void write_text(Machine& machine, const GlkTextBuffer& text) {
        for (u32 index = 0; index < text.length; ++index) {
            if (text.encoding == GlkTextEncoding::unicode) {
                write_codepoint(machine.memory.read32(text.address + index * 4));
            } else {
                write_codepoint(machine.memory.read8(text.address + index));
            }
        }
    }

    void write_text(Machine& machine, const GlkTextString& text) {
        auto address = text.address;
        while (true) {
            if (text.encoding == GlkTextEncoding::unicode) {
                const auto ch = machine.memory.read32(address);
                address += 4;
                if (ch == 0) {
                    return;
                }
                write_codepoint(ch);
            } else {
                const auto ch = machine.memory.read8(address++);
                if (ch == 0) {
                    return;
                }
                write_codepoint(ch);
            }
        }
    }

    void write_codepoint(u32 ch) {
        if (ch == '\r') {
            ch = '\n';
        }
        *output << (ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
};

static_assert(SemanticGlkHost<
              StdioGlkHost,
              GlkObjectRegistry<StdioGlkHost::Window, StdioGlkHost::Stream,
                                StdioGlkHost::FileRef>>);

}  // namespace glupsk
