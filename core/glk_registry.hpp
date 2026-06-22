#pragma once

#include "core/error.hpp"
#include "core/glk.hpp"
#include "core/glk_events.hpp"
#include "core/machine.hpp"
#include "core/ring.hpp"
#include "core/types.hpp"

#include <memory>
#include <variant>
#include <vector>

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
    // Coalescing buffer for per-character output. Non-empty only on host
    // streams; memory streams leave it at capacity 0 (drain-immediately).
    Ring<u32> buffer = {};

    bool allocated() const {
        return !std::holds_alternative<std::monostate>(backing);
    }
};

template <typename Host>
class GlkRegistry {
  public:
    using WindowRecord = GlkWindowRecord<Host>;
    using StreamRecord = GlkStreamRecord<Host>;

    WindowRecord& require_window(u32 id) {
        if (id == 0 || id > windows_.size() || !windows_[id - 1]) {
            fail("invalid Glk window handle");
        }
        return *windows_[id - 1];
    }

    StreamRecord& require_stream(u32 id) {
        if (id == 0 || id > streams_.size() || !streams_[id - 1] ||
            !streams_[id - 1]->allocated()) {
            fail("invalid Glk stream handle");
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
                glk_write_ref(machine, rock_address, window->rock);
                return id;
            }
            if (id == previous_id) {
                found_previous = true;
                return_next = true;
            }
        }
        if (!found_previous) {
            fail("window_iterate received invalid window");
        }
        glk_write_ref(machine, rock_address, 0);
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
                glk_write_ref(machine, rock_address, stream->rock);
                return id;
            }
            if (id == previous_id) {
                found_previous = true;
                return_next = true;
            }
        }
        if (!found_previous) {
            fail("stream_iterate received invalid stream");
        }
        glk_write_ref(machine, rock_address, 0);
        return 0;
    }

  private:
    std::vector<std::unique_ptr<WindowRecord>> windows_;
    std::vector<std::unique_ptr<StreamRecord>> streams_;
};

}  // namespace glupsk
