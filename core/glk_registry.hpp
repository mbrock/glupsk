#pragma once

#include "core/glk.hpp"
#include "core/types.hpp"

#include <variant>

namespace glupsk {

template <typename Host>
struct GlkHostStream {
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

template <typename Host>
using GlkStreamBacking =
    std::variant<std::monostate, GlkHostStream<Host>, GlkMemoryStream,
                 GlkUnicodeMemoryStream>;

template <typename Host>
struct GlkWindowRecord {
    typename Host::Window window;
    u32 rock = 0;
    u32 type = 0;
    GlkStreamHandle stream = {};
};

template <typename Host>
struct GlkStreamRecord {
    GlkStreamBacking<Host> backing = {};
    u32 rock = 0;
    u32 pos = 0;
    u32 read_count = 0;
    u32 write_count = 0;

    bool allocated() const {
        return !std::holds_alternative<std::monostate>(backing);
    }
};

}  // namespace glupsk
