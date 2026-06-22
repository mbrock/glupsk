#include "core/machine.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace glupsk {
namespace {

void put_latin1(Machine& machine, TranscriptGlk& glk, std::string_view text) {
    for (const auto ch : text) {
        glk.put_char(machine, static_cast<unsigned char>(ch));
    }
}

TranscriptStream* find_stream(TranscriptGlk& glk, u32 id) {
    if (id == 0) {
        return nullptr;
    }
    for (auto& stream : glk.streams) {
        if (stream.allocated() && stream.id == id) {
            return &stream;
        }
    }
    return nullptr;
}

TranscriptWindow* find_window(TranscriptGlk& glk, u32 id) {
    if (id == 0) {
        return nullptr;
    }
    for (auto& window : glk.windows) {
        if (window.id == id) {
            return &window;
        }
    }
    return nullptr;
}

TranscriptFileRef* find_fileref(TranscriptGlk& glk, u32 id) {
    if (id == 0) {
        return nullptr;
    }
    for (auto& fileref : glk.filerefs) {
        if (fileref.id == id) {
            return &fileref;
        }
    }
    return nullptr;
}

TranscriptWindow& require_window_record(TranscriptGlk& glk,
                                        u32 id,
                                        std::string_view operation) {
    if (auto* window = find_window(glk, id)) {
        return *window;
    }
    throw std::runtime_error(
        std::format("{} received invalid window {}", operation, id));
}

TranscriptStream& require_stream_record(TranscriptGlk& glk,
                                        u32 id,
                                        std::string_view operation) {
    if (auto* stream = find_stream(glk, id)) {
        return *stream;
    }
    throw std::runtime_error(
        std::format("{} received invalid stream {}", operation, id));
}

TranscriptFileRef& require_fileref_record(TranscriptGlk& glk,
                                          u32 id,
                                          std::string_view operation) {
    if (auto* fileref = find_fileref(glk, id)) {
        return *fileref;
    }
    throw std::runtime_error(
        std::format("{} received invalid fileref {}", operation, id));
}

void write_glk_ref(Machine& machine, u32 address, u32 value) {
    if (address == 0) {
        return;
    }
    if (address == 0xffffffffu) {
        machine.stack.push32(value);
        return;
    }
    machine.memory.write32(address, value);
}

void write_glk_struct_field(Machine& machine, u32 address, u32 field, u32 value) {
    if (address == 0) {
        return;
    }
    if (address == 0xffffffffu) {
        machine.stack.push32(value);
        return;
    }
    machine.memory.write32(address + field * 4, value);
}

u32 memory_stream_mode(const GlkStreamBacking& backing) {
    if (const auto* stream = std::get_if<GlkMemoryStream>(&backing)) {
        return stream->mode;
    }
    if (const auto* stream = std::get_if<GlkUnicodeMemoryStream>(&backing)) {
        return stream->mode;
    }
    return 0;
}

TranscriptStream& allocate_stream(TranscriptGlk& glk,
                                  GlkStreamBacking backing,
                                  u32 rock) {
    const auto mode = memory_stream_mode(backing);
    if (mode != 0 && mode != 1 && mode != 2 && mode != 3) {
        throw std::runtime_error(
            std::format("unsupported memory stream mode {}", mode));
    }
    for (auto& stream : glk.streams) {
        if (!stream.allocated()) {
            stream = {
                .backing = std::move(backing),
                .id = glk.next_stream_id++,
                .pos = 0,
                .read_count = 0,
                .write_count = 0,
                .rock = rock,
            };
            return stream;
        }
    }
    throw std::runtime_error("transcript Glk stream table is full");
}

u32 allocate_window(TranscriptGlk& glk, u32 type, u32 rock) {
    for (auto& window : glk.windows) {
        if (window.id == 0) {
            auto& stream = allocate_stream(glk, GlkWindowStream{}, 0);
            window = {
                .id = glk.next_window_id++,
                .rock = rock,
                .stream_id = stream.id,
                .type = type,
            };
            if (glk.root_window == 0) {
                glk.root_window = window.id;
            }
            return window.id;
        }
    }
    throw std::runtime_error("transcript Glk window table is full");
}

u32 iterate_windows(TranscriptGlk& glk, u32 previous_id, u32 rock_address,
                    Machine& machine) {
    auto return_next = previous_id == 0;
    auto found_previous = previous_id == 0;
    for (const auto& window : glk.windows) {
        if (window.id == 0) {
            continue;
        }
        if (return_next) {
            write_glk_ref(machine, rock_address, window.rock);
            return window.id;
        }
        if (window.id == previous_id) {
            found_previous = true;
            return_next = true;
        }
    }
    if (!found_previous) {
        throw std::runtime_error(
            std::format("window_iterate received invalid window {}", previous_id));
    }
    write_glk_ref(machine, rock_address, 0);
    return 0;
}

u32 iterate_streams(TranscriptGlk& glk, u32 previous_id, u32 rock_address,
                    Machine& machine) {
    auto return_next = previous_id == 0;
    auto found_previous = previous_id == 0;
    for (const auto& stream : glk.streams) {
        if (!stream.allocated()) {
            continue;
        }
        if (return_next) {
            write_glk_ref(machine, rock_address, stream.rock);
            return stream.id;
        }
        if (stream.id == previous_id) {
            found_previous = true;
            return_next = true;
        }
    }
    if (!found_previous) {
        throw std::runtime_error(
            std::format("stream_iterate received invalid stream {}", previous_id));
    }
    write_glk_ref(machine, rock_address, 0);
    return 0;
}

void close_stream(Machine& machine, TranscriptStream& stream, u32 result_address) {
    if (std::holds_alternative<GlkWindowStream>(stream.backing)) {
        throw std::runtime_error("stream_close cannot close a window stream");
    }
    write_glk_struct_field(machine, result_address, 0, stream.read_count);
    write_glk_struct_field(machine, result_address, 1, stream.write_count);
    stream = {};
}

void write_stream_char(Machine& machine,
                       TranscriptGlk& glk,
                       TranscriptStream& stream,
                       u32 ch) {
    if (std::holds_alternative<GlkWindowStream>(stream.backing)) {
        glk.transcript.push_back(ch <= 0x7f ? static_cast<char>(ch) : '?');
        ++stream.write_count;
        return;
    }
    if (const auto* memory = std::get_if<GlkMemoryStream>(&stream.backing)) {
        if (memory->mode == 2) {
            throw std::runtime_error("cannot write to read-only memory stream");
        }
        if (stream.pos < memory->len && memory->address != 0) {
            machine.memory.write8(memory->address + stream.pos, static_cast<u8>(ch));
        }
        ++stream.pos;
        ++stream.write_count;
        return;
    }
    if (const auto* memory =
            std::get_if<GlkUnicodeMemoryStream>(&stream.backing)) {
        if (memory->mode == 2) {
            throw std::runtime_error(
                "cannot write to read-only unicode memory stream");
        }
        if (stream.pos < memory->len && memory->address != 0) {
            machine.memory.write32(memory->address + stream.pos * 4, ch);
        }
        ++stream.pos;
        ++stream.write_count;
    }
}

void put_c_string(TranscriptGlk& glk, Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read8(address++);
        if (ch == 0) {
            return;
        }
        glk.put_char(machine, ch);
    }
}

void put_uni_string(TranscriptGlk& glk, Machine& machine, u32 address) {
    while (true) {
        const auto ch = machine.memory.read32(address);
        address += 4;
        if (ch == 0) {
            return;
        }
        glk.put_char(machine, ch);
    }
}

void put_byte_buffer(TranscriptGlk& glk,
                     Machine& machine,
                     u32 address,
                     u32 len) {
    for (u32 index = 0; index < len; ++index) {
        glk.put_char(machine, machine.memory.read8(address + index));
    }
}

void put_uni_buffer(TranscriptGlk& glk,
                    Machine& machine,
                    u32 address,
                    u32 len) {
    for (u32 index = 0; index < len; ++index) {
        glk.put_char(machine, machine.memory.read32(address + index * 4));
    }
}

u32 glk_char_to_lower(u32 ch) {
    if ((ch >= 0x41 && ch <= 0x5a) || (ch >= 0xc0 && ch <= 0xd6) ||
        (ch >= 0xd8 && ch <= 0xde)) {
        return ch + 0x20;
    }
    return ch;
}

u32 glk_char_to_upper(u32 ch) {
    if ((ch >= 0x61 && ch <= 0x7a) || (ch >= 0xe0 && ch <= 0xf6) ||
        (ch >= 0xf8 && ch <= 0xfe)) {
        return ch - 0x20;
    }
    return ch;
}

u32 transform_unicode_buffer(Machine& machine,
                             u32 address,
                             u32 len,
                             u32 numchars,
                             u32 (*transform)(u32)) {
    const auto writable = std::min(len, numchars);
    for (u32 index = 0; index < writable; ++index) {
        const auto cell = address + index * 4;
        machine.memory.write32(cell, transform(machine.memory.read32(cell)));
    }
    return numchars;
}

u32 title_case_unicode_buffer(Machine& machine,
                              u32 address,
                              u32 len,
                              u32 numchars,
                              bool lower_rest) {
    const auto writable = std::min(len, numchars);
    if (writable == 0) {
        return numchars;
    }

    machine.memory.write32(address, glk_char_to_upper(machine.memory.read32(address)));
    if (lower_rest) {
        for (u32 index = 1; index < writable; ++index) {
            const auto cell = address + index * 4;
            machine.memory.write32(cell, glk_char_to_lower(machine.memory.read32(cell)));
        }
    }
    return numchars;
}

}  // namespace

void TranscriptGlk::add_input_line(std::string line) {
    input_lines.push_back(std::move(line));
}

TranscriptWindow& TranscriptGlk::require_window(GlkWindowHandle window) {
    return require_window_record(*this, window.id, "registry require_window");
}

TranscriptStream& TranscriptGlk::require_stream(GlkStreamHandle stream) {
    return require_stream_record(*this, stream.id, "registry require_stream");
}

TranscriptFileRef& TranscriptGlk::require_fileref(GlkFileRefHandle fileref) {
    return require_fileref_record(*this, fileref.id, "registry require_fileref");
}

GlkWindowHandle TranscriptGlk::intern_window(TranscriptWindow& window) {
    if (window.id == 0) {
        throw std::runtime_error("cannot intern unallocated window");
    }
    return {.id = window.id};
}

GlkStreamHandle TranscriptGlk::intern_stream(TranscriptStream& stream) {
    if (!stream.allocated() || stream.id == 0) {
        throw std::runtime_error("cannot intern unallocated stream");
    }
    return {.id = stream.id};
}

GlkFileRefHandle TranscriptGlk::intern_fileref(TranscriptFileRef& fileref) {
    if (fileref.id == 0) {
        throw std::runtime_error("cannot intern unallocated fileref");
    }
    return {.id = fileref.id};
}

bool TranscriptGlk::select_would_block() const {
    return !line_pending || input_lines.empty();
}

u32 TranscriptGlk::call(Machine& machine, u32 selector, span<const u32> args) {
    switch (static_cast<GlkSelector>(selector)) {
        case GlkSelector::exit:
            machine.halted = true;
            machine.running = false;
            return 0;
        case GlkSelector::gestalt:
            if (args.empty()) {
                return 0;
            }
            switch (static_cast<GlkGestaltSelector>(args[0])) {
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
        case GlkSelector::window_iterate:
            return iterate_windows(*this, args.empty() ? 0 : args[0],
                                   args.size() >= 2 ? args[1] : 0, machine);
        case GlkSelector::window_get_rock:
            if (!args.empty()) {
                return require_window_record(*this, args[0], "window_get_rock").rock;
            }
            return 0;
        case GlkSelector::window_get_root:
            return root_window;
        case GlkSelector::window_open:
            if (args.size() >= 5) {
                return allocate_window(*this, args[3], args[4]);
            }
            return allocate_window(*this, 0, 0);
        case GlkSelector::window_get_size:
            if (args.size() >= 3) {
                (void) require_window_record(*this, args[0], "window_get_size");
                write_glk_ref(machine, args[1], 80);
                write_glk_ref(machine, args[2], 24);
            }
            return 0;
        case GlkSelector::window_clear:
            if (!args.empty()) {
                (void) require_window_record(*this, args[0], "window_clear");
            }
            return 0;
        case GlkSelector::window_move_cursor:
            if (!args.empty()) {
                (void) require_window_record(*this, args[0], "window_move_cursor");
            }
            return 0;
        case GlkSelector::window_get_stream:
            if (args.empty()) {
                return 0;
            }
            return require_window_record(*this, args[0], "window_get_stream").stream_id;
        case GlkSelector::set_window:
            current_stream = 0;
            if (!args.empty() && args[0] != 0) {
                current_stream =
                    require_window_record(*this, args[0], "set_window").stream_id;
            }
            return 0;
        case GlkSelector::stream_iterate:
            return iterate_streams(*this, args.empty() ? 0 : args[0],
                                   args.size() >= 2 ? args[1] : 0, machine);
        case GlkSelector::stream_get_rock:
            if (!args.empty()) {
                return require_stream_record(*this, args[0], "stream_get_rock").rock;
            }
            return 0;
        case GlkSelector::stream_open_memory:
            if (args.size() >= 4) {
                return allocate_stream(
                           *this,
                           GlkMemoryStream{.address = args[0],
                                           .len = args[1],
                                           .mode = args[2]},
                           args[3])
                    .id;
            }
            return 0;
        case GlkSelector::stream_open_memory_uni:
            if (args.size() >= 4) {
                return allocate_stream(
                           *this,
                           GlkUnicodeMemoryStream{.address = args[0],
                                                  .len = args[1],
                                                  .mode = args[2]},
                           args[3])
                    .id;
            }
            return 0;
        case GlkSelector::stream_close:
            if (!args.empty()) {
                auto& stream = require_stream_record(*this, args[0], "stream_close");
                if (current_stream == stream.id) {
                    current_stream = 0;
                }
                close_stream(machine, stream, args.size() >= 2 ? args[1] : 0);
            }
            return 0;
        case GlkSelector::stream_set_current:
            current_stream = 0;
            if (!args.empty() && args[0] != 0) {
                (void) require_stream_record(*this, args[0], "stream_set_current");
                current_stream = args[0];
            }
            return 0;
        case GlkSelector::stream_get_current:
            return current_stream;
        case GlkSelector::fileref_iterate:
            if (args.size() >= 2) {
                write_glk_ref(machine, args[1], 0);
            }
            return 0;
        case GlkSelector::fileref_get_rock:
            if (!args.empty() && args[0] != 0) {
                throw std::runtime_error(
                    std::format("fileref_get_rock received invalid fileref {}",
                                args[0]));
            }
            return 0;
        case GlkSelector::set_style:
        case GlkSelector::stylehint_set:
            return 0;
        case GlkSelector::put_char:
        case GlkSelector::put_char_uni:
            if (!args.empty()) {
                put_char(machine, args[0]);
            }
            return 0;
        case GlkSelector::put_char_stream:
        case GlkSelector::put_char_stream_uni:
            if (args.size() >= 2) {
                (void) require_stream_record(*this, args[0], "put_char_stream");
                const auto old_stream = current_stream;
                current_stream = args[0];
                put_char(machine, args[1]);
                current_stream = old_stream;
            }
            return 0;
        case GlkSelector::put_string:
        case GlkSelector::put_string_stream:
            if (!args.empty()) {
                const auto old_stream = current_stream;
                if (static_cast<GlkSelector>(selector) == GlkSelector::put_string_stream &&
                    args.size() >= 2) {
                    (void) require_stream_record(*this, args[0], "put_string_stream");
                    current_stream = args[0];
                }
                put_c_string(*this, machine, args.back() + 1);
                current_stream = old_stream;
            }
            return 0;
        case GlkSelector::put_string_uni:
        case GlkSelector::put_string_stream_uni:
            if (!args.empty()) {
                const auto old_stream = current_stream;
                if (static_cast<GlkSelector>(selector) ==
                        GlkSelector::put_string_stream_uni &&
                    args.size() >= 2) {
                    (void) require_stream_record(*this, args[0],
                                                 "put_string_stream_uni");
                    current_stream = args[0];
                }
                put_uni_string(*this, machine, args.back() + 4);
                current_stream = old_stream;
            }
            return 0;
        case GlkSelector::put_buffer:
            if (args.size() >= 2) {
                put_byte_buffer(*this, machine, args[0], args[1]);
            }
            return 0;
        case GlkSelector::put_buffer_stream:
            if (args.size() >= 3) {
                (void) require_stream_record(*this, args[0], "put_buffer_stream");
                const auto old_stream = current_stream;
                current_stream = args[0];
                put_byte_buffer(*this, machine, args[1], args[2]);
                current_stream = old_stream;
            }
            return 0;
        case GlkSelector::put_buffer_uni:
            if (args.size() >= 2) {
                put_uni_buffer(*this, machine, args[0], args[1]);
            }
            return 0;
        case GlkSelector::put_buffer_stream_uni:
            if (args.size() >= 3) {
                (void) require_stream_record(*this, args[0], "put_buffer_stream_uni");
                const auto old_stream = current_stream;
                current_stream = args[0];
                put_uni_buffer(*this, machine, args[1], args[2]);
                current_stream = old_stream;
            }
            return 0;
        case GlkSelector::buffer_to_lower_case_uni:
            if (args.size() >= 3) {
                return transform_unicode_buffer(machine, args[0], args[1], args[2],
                                                glk_char_to_lower);
            }
            return 0;
        case GlkSelector::buffer_to_upper_case_uni:
            if (args.size() >= 3) {
                return transform_unicode_buffer(machine, args[0], args[1], args[2],
                                                glk_char_to_upper);
            }
            return 0;
        case GlkSelector::buffer_to_title_case_uni:
            if (args.size() >= 4) {
                return title_case_unicode_buffer(machine, args[0], args[1], args[2],
                                                 args[3] != 0);
            }
            return 0;
        case GlkSelector::buffer_canon_decompose_uni:
        case GlkSelector::buffer_canon_normalize_uni:
            return args.size() >= 3 ? args[2] : 0;
        case GlkSelector::request_line_event:
            if (args.size() >= 3) {
                (void) require_window_record(*this, args[0], "request_line_event");
                line_pending = true;
                line_unicode = false;
                line_window = args[0];
                line_buffer = args[1];
                line_maxlen = args[2];
                line_initial_len = args.size() >= 4 ? args[3] : 0;
            }
            return 0;
        case GlkSelector::request_line_event_uni:
            if (args.size() >= 3) {
                (void) require_window_record(*this, args[0], "request_line_event_uni");
                line_pending = true;
                line_unicode = true;
                line_window = args[0];
                line_buffer = args[1];
                line_maxlen = args[2];
                line_initial_len = args.size() >= 4 ? args[3] : 0;
            }
            return 0;
        case GlkSelector::select: {
            if (args.empty() || !line_pending || input_lines.empty()) {
                return 0;
            }

            const auto event_address = args[0];
            auto line = std::move(input_lines.front());
            input_lines.pop_front();

            const auto count = std::min<u32>(
                line_maxlen, static_cast<u32>(line.size()));
            for (u32 index = 0; index < count; ++index) {
                const auto ch = static_cast<u8>(line[index]);
                if (line_unicode) {
                    machine.memory.write32(line_buffer + index * 4, ch);
                } else {
                    machine.memory.write8(line_buffer + index, ch);
                }
            }
            write_glk_struct_field(machine, event_address, 0, 3);
            write_glk_struct_field(machine, event_address, 1, line_window);
            write_glk_struct_field(machine, event_address, 2, count);
            write_glk_struct_field(machine, event_address, 3, 0);
            line_pending = false;
            put_latin1(machine, *this, line);
            put_char(machine, '\n');
            return 0;
        }
        case GlkSelector::char_to_lower:
        case GlkSelector::char_to_upper:
            if (args.empty()) {
                return 0;
            }
            return static_cast<GlkSelector>(selector) == GlkSelector::char_to_lower
                       ? glk_char_to_lower(args[0])
                       : glk_char_to_upper(args[0]);
        default:
            throw std::runtime_error(
                std::format("unsupported Glk selector 0x{:04x}", selector));
    }
}

void TranscriptGlk::put_char(Machine& machine, u32 ch) {
    if (ch == '\r') {
        ch = '\n';
    }
    if (auto* stream = find_stream(*this, current_stream)) {
        write_stream_char(machine, *this, *stream, ch);
        return;
    }
    if (current_stream != 0) {
        throw std::runtime_error(
            std::format("current stream {} is invalid", current_stream));
    }
    if (ch <= 0x7f) {
        transcript.push_back(static_cast<char>(ch));
        return;
    }
    transcript.push_back('?');
}

}  // namespace glupsk
