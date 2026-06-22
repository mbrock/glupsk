#pragma once

#include "core/glk.hpp"
#include "core/machine.hpp"

#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace glupsk {

template <typename Host>
class GlkRegistry {
  public:
    using Window = typename Host::Window;
    using Stream = typename Host::Stream;
    using FileRef = typename Host::FileRef;

    GlkWindowHandle add_window(Window window) {
        windows_.push_back(std::make_unique<Window>(std::move(window)));
        return {.id = static_cast<u32>(windows_.size())};
    }

    GlkStreamHandle add_stream(Stream stream) {
        streams_.push_back(std::make_unique<Stream>(std::move(stream)));
        return {.id = static_cast<u32>(streams_.size())};
    }

    GlkFileRefHandle add_fileref(FileRef fileref) {
        filerefs_.push_back(std::make_unique<FileRef>(std::move(fileref)));
        return {.id = static_cast<u32>(filerefs_.size())};
    }

    Window& require_window(GlkWindowHandle handle) {
        return require(windows_, handle.id, "window");
    }

    Stream& require_stream(GlkStreamHandle handle) {
        return require(streams_, handle.id, "stream");
    }

    FileRef& require_fileref(GlkFileRefHandle handle) {
        return require(filerefs_, handle.id, "fileref");
    }

    GlkWindowHandle intern_window(Window& window) {
        return {.id = intern(windows_, window, "window")};
    }

    GlkStreamHandle intern_stream(Stream& stream) {
        return {.id = intern(streams_, stream, "stream")};
    }

    GlkFileRefHandle intern_fileref(FileRef& fileref) {
        return {.id = intern(filerefs_, fileref, "fileref")};
    }

  private:
    template <typename T>
    static T& require(std::vector<std::unique_ptr<T>>& objects,
                      u32 id,
                      const char* name) {
        if (id == 0 || id > objects.size() || !objects[id - 1]) {
            throw std::runtime_error(std::format("invalid Glk {} handle {}", name, id));
        }
        return *objects[id - 1];
    }

    template <typename T>
    static u32 intern(std::vector<std::unique_ptr<T>>& objects,
                      T& object,
                      const char* name) {
        for (auto index = std::size_t{0}; index < objects.size(); ++index) {
            if (objects[index].get() == &object) {
                return static_cast<u32>(index + 1);
            }
        }
        throw std::runtime_error(std::format("unregistered Glk {} object", name));
    }

    std::vector<std::unique_ptr<Window>> windows_;
    std::vector<std::unique_ptr<Stream>> streams_;
    std::vector<std::unique_ptr<FileRef>> filerefs_;
};

template <typename Host>
class GlkBridge {
  public:
    using Registry = typename Host::Registry;

    explicit GlkBridge(Host host = {}) : host_(std::move(host)) {}

    Host& host() { return host_; }
    const Host& host() const { return host_; }

    Registry& registry() { return registry_; }
    const Registry& registry() const { return registry_; }

    u32 gestalt(u32 selector, u32 value) {
        return host_.gestalt(GlkGestaltQuery{
            .selector = static_cast<GlkGestaltSelector>(selector),
            .value = value,
        });
    }

    GlkWindowHandle window_open(GlkWindowHandle split,
                                u32 method,
                                u32 size,
                                u32 type,
                                u32 rock) {
        return host_.window_open(registry_, split, method, size, type, rock);
    }

    GlkWindowHandle window_get_root() {
        return host_.window_get_root(registry_);
    }

    u32 window_get_rock(GlkWindowHandle window) {
        return host_.window_get_rock(registry_, window);
    }

    GlkStreamHandle window_get_stream(GlkWindowHandle window) {
        return host_.window_get_stream(registry_, window);
    }

    u32 stream_get_rock(GlkStreamHandle stream) {
        return host_.stream_get_rock(registry_, stream);
    }

    GlkStreamHandle current_stream() const { return current_stream_; }

    GlkCallResult set_current_stream(GlkStreamHandle stream) {
        if (stream.id != 0) {
            (void) registry_.require_stream(stream);
        }
        current_stream_ = stream;
        return glk_returned();
    }

    GlkCallResult set_window(GlkWindowHandle window) {
        if (window.id == 0) {
            current_stream_ = {};
            return glk_returned();
        }
        current_stream_ = host_.window_get_stream(registry_, window);
        return glk_returned();
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
        return host_.write(registry_, stream, materialize_text(machine, text));
    }

    void request_line_event(Machine& machine,
                            GlkWindowHandle window,
                            u32 buffer_address,
                            u32 max_length,
                            u32 initial_length,
                            GlkTextEncoding encoding) {
        (void) registry_.require_window(window);
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
            registry_,
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

  private:
    struct PendingLine {
        GlkWindowHandle window = {};
        u32 buffer_address = 0;
        u32 max_length = 0;
        GlkTextEncoding encoding = GlkTextEncoding::latin1;
    };

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
                codepoints.push_back(machine.memory.read32(text.address + index * 4));
            }
            return codepoints;
        }

        auto bytes = std::string{};
        bytes.reserve(text.length);
        for (u32 index = 0; index < text.length; ++index) {
            bytes.push_back(static_cast<char>(machine.memory.read8(text.address + index)));
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
        return glk_returned();
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
    Registry registry_;
    GlkStreamHandle current_stream_ = {};
    std::vector<GlkEventInterest> event_interests_;
    std::optional<PendingLine> pending_line_;
};

}  // namespace glupsk
