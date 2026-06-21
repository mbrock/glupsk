#include "core/decode.hpp"
#include "core/execute.hpp"
#include "core/story.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Watch {
    glupsk::u32 address = 0;
    glupsk::u32 words = 4;
};

struct Options {
    const char* story_path = nullptr;
    glupsk::u64 skip = 0;
    glupsk::u64 count = 200;
    glupsk::u64 max_steps = 1'000'000;
    std::optional<glupsk::u32> start_pc;
    bool print_transcript_tail = false;
    std::vector<std::string> input_lines;
    std::vector<Watch> watches;
};

void usage() {
    std::println(stderr,
                 "usage: glupsk-trace [--skip N] [--count N] [--max-steps N] "
                 "[--start-pc ADDR] [--input LINE] [--watch ADDR[:WORDS]] "
                 "[--transcript-tail] STORY.ulx");
}

glupsk::u64 parse_u64(std::string_view text) {
    auto value = glupsk::u64{};
    auto base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        text.remove_prefix(2);
        base = 16;
    }
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [cursor, error] = std::from_chars(begin, end, value, base);
    if (error != std::errc{} || cursor != end) {
        throw std::runtime_error("invalid integer option");
    }
    return value;
}

Watch parse_watch(std::string_view text) {
    auto watch = Watch{};
    if (const auto colon = text.find(':'); colon != std::string_view::npos) {
        watch.address = static_cast<glupsk::u32>(parse_u64(text.substr(0, colon)));
        watch.words = static_cast<glupsk::u32>(parse_u64(text.substr(colon + 1)));
        return watch;
    }
    watch.address = static_cast<glupsk::u32>(parse_u64(text));
    return watch;
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const auto arg = std::string_view{argv[index]};
        auto need_value = [&](std::string_view name) -> std::string_view {
            if (++index >= argc) {
                throw std::runtime_error(std::string{name} + " requires a value");
            }
            return argv[index];
        };

        if (arg == "--skip") {
            options.skip = parse_u64(need_value(arg));
        } else if (arg == "--count") {
            options.count = parse_u64(need_value(arg));
        } else if (arg == "--max-steps") {
            options.max_steps = parse_u64(need_value(arg));
        } else if (arg == "--start-pc") {
            options.start_pc = static_cast<glupsk::u32>(parse_u64(need_value(arg)));
        } else if (arg == "--input") {
            options.input_lines.emplace_back(need_value(arg));
        } else if (arg == "--watch") {
            options.watches.push_back(parse_watch(need_value(arg)));
        } else if (arg == "--transcript-tail") {
            options.print_transcript_tail = true;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        } else if (!arg.starts_with("-") && options.story_path == nullptr) {
            options.story_path = argv[index];
        } else {
            throw std::runtime_error("unexpected argument");
        }
    }

    if (options.story_path == nullptr) {
        throw std::runtime_error("missing story path");
    }
    return options;
}

glupsk::u32 sign_extend(glupsk::u32 value, int bits) {
    const auto mask = glupsk::u32{1} << (bits - 1);
    return (value ^ mask) - mask;
}

std::string_view glulx_gestalt_name(glupsk::u32 selector) {
    switch (selector) {
        case 0: return "GlulxVersion";
        case 1: return "TerpVersion";
        case 2: return "ResizeMem";
        case 3: return "Undo";
        case 4: return "IOSystem";
        case 5: return "Unicode";
        case 6: return "MemCopy";
        case 7: return "MAlloc";
        case 8: return "MAllocHeap";
        case 9: return "Acceleration";
        case 10: return "AccelFunc";
        case 11: return "Float";
        case 12: return "ExtUndo";
        case 13: return "Double";
        default: return "unknown";
    }
}

std::string_view glk_selector_name(glupsk::u32 selector) {
    switch (selector) {
        case 0x0001: return "exit";
        case 0x0004: return "gestalt";
        case 0x0020: return "window_iterate";
        case 0x0021: return "window_get_rock";
        case 0x0022: return "window_get_root";
        case 0x0023: return "window_open";
        case 0x0025: return "window_get_size";
        case 0x002a: return "window_clear";
        case 0x002b: return "window_move_cursor";
        case 0x002c: return "window_get_stream";
        case 0x002f: return "set_window";
        case 0x0040: return "stream_iterate";
        case 0x0041: return "stream_get_rock";
        case 0x0043: return "stream_open_memory";
        case 0x0044: return "stream_close";
        case 0x0047: return "stream_set_current";
        case 0x0048: return "stream_get_current";
        case 0x0064: return "fileref_iterate";
        case 0x0065: return "fileref_get_rock";
        case 0x0080: return "put_char";
        case 0x0081: return "put_char_stream";
        case 0x0082: return "put_string";
        case 0x0083: return "put_string_stream";
        case 0x0084: return "put_buffer";
        case 0x0085: return "put_buffer_stream";
        case 0x0086: return "set_style";
        case 0x00a0: return "char_to_lower";
        case 0x00a1: return "char_to_upper";
        case 0x00b0: return "stylehint_set";
        case 0x00c0: return "select";
        case 0x00d0: return "request_line_event";
        case 0x0120: return "buffer_to_lower_case_uni";
        case 0x0121: return "buffer_to_upper_case_uni";
        case 0x0122: return "buffer_to_title_case_uni";
        case 0x0123: return "buffer_canon_decompose_uni";
        case 0x0124: return "buffer_canon_normalize_uni";
        case 0x0128: return "put_char_uni";
        case 0x0129: return "put_string_uni";
        case 0x012a: return "put_buffer_uni";
        case 0x012b: return "put_char_stream_uni";
        case 0x012c: return "put_string_stream_uni";
        case 0x012d: return "put_buffer_stream_uni";
        case 0x0139: return "stream_open_memory_uni";
        case 0x0141: return "request_line_event_uni";
        default: return "unknown";
    }
}

std::string_view glk_gestalt_name(glupsk::u32 selector) {
    switch (selector) {
        case 0: return "Version";
        case 1: return "CharInput";
        case 2: return "LineInput";
        case 3: return "CharOutput";
        case 4: return "MouseInput";
        case 5: return "Timer";
        case 6: return "Graphics";
        case 7: return "DrawImage";
        case 8: return "Sound";
        case 9: return "SoundVolume";
        case 10: return "SoundNotify";
        case 11: return "Hyperlinks";
        case 12: return "HyperlinkInput";
        case 15: return "Unicode";
        case 16: return "UnicodeNorm";
        default: return "unknown";
    }
}

glupsk::u32 locals_base(const glupsk::Machine& machine) {
    if (machine.regs.frame_ptr + 8 > machine.stack.sp) {
        return 0;
    }
    return machine.regs.frame_ptr + machine.stack.read32(machine.regs.frame_ptr + 4);
}

glupsk::u32 operand_value(const glupsk::Machine& machine,
                          glupsk::Operand operand,
                          glupsk::u32 stack_index) {
    using enum glupsk::OperandMode;
    switch (operand.mode) {
        case zero:
            return 0;
        case const8:
            return sign_extend(operand.data, 8);
        case const16:
            return sign_extend(operand.data, 16);
        case const32:
            return operand.data;
        case stack: {
            const auto needed = 4 * (stack_index + 1);
            if (machine.stack.sp < needed) {
                throw std::runtime_error("stack operand is outside stack");
            }
            const auto offset = machine.stack.sp - needed;
            return machine.stack.read32(offset);
        }
        case local8:
        case local16:
        case local32:
            return machine.stack.read32(locals_base(machine) + operand.data);
        case ram8:
        case ram16:
        case ram32:
            return machine.memory.read32(machine.memory.ramstart + operand.data);
        case mem8:
        case mem16:
        case mem32:
            return machine.memory.read32(operand.data);
        default:
            throw std::runtime_error("unused operand addressing mode");
    }
}

std::string operand_detail(const glupsk::Machine& machine,
                           glupsk::Operand operand,
                           glupsk::u32 stack_index) {
    using enum glupsk::OperandMode;
    const auto mode = operand.mode;
    try {
        switch (mode) {
            case zero:
                return "zero";
            case const8:
                return std::format("const8({})", static_cast<std::int32_t>(
                                                     sign_extend(operand.data, 8)));
            case const16:
                return std::format("const16({})", static_cast<std::int32_t>(
                                                      sign_extend(operand.data, 16)));
            case const32:
                return std::format("const32(0x{:08x})", operand.data);
            case stack: {
                const auto offset = machine.stack.sp - 4 * (stack_index + 1);
                const auto value = machine.stack.sp >= 4 * (stack_index + 1)
                                       ? machine.stack.read32(offset)
                                       : 0;
                return std::format("stack[{}]=0x{:08x}", stack_index, value);
            }
            case local8:
            case local16:
            case local32: {
                const auto address = locals_base(machine) + operand.data;
                const auto value = machine.stack.read32(address);
                return std::format("{}(+0x{:x})=0x{:x}",
                                   glupsk::operand_mode_name(mode), operand.data,
                                   value);
            }
            case ram8:
            case ram16:
            case ram32: {
                const auto address = machine.memory.ramstart + operand.data;
                return std::format("{}(@0x{:x})=0x{:08x}",
                                   glupsk::operand_mode_name(mode), address,
                                   machine.memory.read32(address));
            }
            case mem8:
            case mem16:
            case mem32:
                return std::format("{}(@0x{:x})=0x{:08x}",
                                   glupsk::operand_mode_name(mode), operand.data,
                                   machine.memory.read32(operand.data));
            default:
                return std::format("{}(0x{:x})", glupsk::operand_mode_name(mode),
                                   operand.data);
        }
    } catch (const std::exception& ex) {
        return std::format("{}(0x{:x}; {})", glupsk::operand_mode_name(mode),
                           operand.data, ex.what());
    }
}

void print_stack_top(const glupsk::Machine& machine) {
    std::print(" stack=[");
    const auto words = std::min<glupsk::u32>(4, machine.stack.sp / 4);
    for (glupsk::u32 index = 0; index < words; ++index) {
        if (index != 0) {
            std::print(" ");
        }
        const auto address = machine.stack.sp - 4 * (index + 1);
        std::print("0x{:08x}", machine.stack.read32(address));
    }
    std::print("]");
}

void print_watches(const glupsk::Machine& machine, std::span<const Watch> watches) {
    for (const auto watch : watches) {
        std::print("watch @0x{:08x}=[", watch.address);
        for (glupsk::u32 index = 0; index < watch.words; ++index) {
            if (index != 0) {
                std::print(" ");
            }
            const auto address = watch.address + index * 4;
            try {
                std::print("0x{:08x}", machine.memory.read32(address));
            } catch (const std::exception& ex) {
                std::print("err:{}", ex.what());
                break;
            }
        }
        std::println("]");
    }
}

void print_opcode_annotation(
    const glupsk::Machine& machine,
    const glupsk::Instruction& instruction,
    const std::array<glupsk::u32, 8>& operand_values) {
    switch (instruction.opcode) {
        using enum glupsk::Opcode;
        case gestalt:
            std::print(" ; gestalt {}({}) arg={}",
                       glulx_gestalt_name(operand_values[0]), operand_values[0],
                       operand_values[1]);
            return;
        case setiosys:
            std::print(" ; iosys mode={} rock=0x{:08x}",
                       operand_values[0], operand_values[1]);
            return;
        case glk: {
            const auto selector = operand_values[0];
            const auto argc = operand_values[1];
            std::print(" ; glk {}(0x{:04x}) argc={} args=[",
                       glk_selector_name(selector), selector, argc);
            const auto shown = std::min<glupsk::u32>(argc, 6);
            for (glupsk::u32 index = 0; index < shown; ++index) {
                if (index != 0) {
                    std::print(" ");
                }
                const auto needed = 4 * (index + 1);
                const auto value = machine.stack.sp >= needed
                                       ? machine.stack.read32(machine.stack.sp - needed)
                                       : 0;
                std::print("0x{:08x}", value);
            }
            if (argc > shown) {
                std::print(" ...");
            }
            std::print("]");
            if (selector == 0x0004 && argc > 0 && machine.stack.sp >= 4) {
                const auto gestalt_selector = machine.stack.read32(machine.stack.sp - 4);
                std::print(" gestalt={}", glk_gestalt_name(gestalt_selector));
            }
            return;
        }
        default:
            return;
    }
}

void print_instruction(const glupsk::Machine& machine,
                       const glupsk::TranscriptGlk& glk,
                       const glupsk::Instruction& instruction,
                       glupsk::u64 step) {
    std::print("#{:08} pc=0x{:08x} next=0x{:08x} {:<14}",
               step, instruction.address, instruction.next_pc,
               glupsk::opcode_name(instruction.opcode));

    auto stack_index = glupsk::u32{0};
    auto operand_values = std::array<glupsk::u32, 8>{};
    for (glupsk::u8 index = 0; index < instruction.operand_count; ++index) {
        const auto operand = instruction.operands[index];
        try {
            operand_values[index] = operand_value(machine, operand, stack_index);
        } catch (const std::exception&) {
            operand_values[index] = 0;
        }
        std::print(" {}:{}", index, operand_detail(machine, operand, stack_index));
        if (operand.mode == glupsk::OperandMode::stack) {
            ++stack_index;
        }
    }

    print_opcode_annotation(machine, instruction, operand_values);

    std::print(" sp={} fp={} iosys={}:0x{:08x} out={}",
               machine.stack.sp, machine.regs.frame_ptr, machine.regs.iosys_mode,
               machine.regs.iosys_rock, glk.transcript.size());
    print_stack_top(machine);
    std::println("");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto story = glupsk::Story::load(options.story_path);
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};
        for (auto& line : options.input_lines) {
            glk.add_input_line(line);
        }
        machine.glk = &glk;

        if (!machine.running) {
            glupsk::start(machine);
        }

        auto printed = glupsk::u64{0};
        auto tracing_started = !options.start_pc.has_value();
        auto start_step = options.skip;
        for (glupsk::u64 step = 0; step < options.max_steps; ++step) {
            if (machine.halted || machine.blocked) {
                break;
            }

            const auto instruction = glupsk::fetch_instruction(machine);
            if (!tracing_started && instruction.address == *options.start_pc) {
                tracing_started = true;
                start_step = step + options.skip;
            }
            const auto should_trace =
                tracing_started && step >= start_step && printed < options.count;
            if (should_trace) {
                print_instruction(machine, glk, instruction, step);
                ++printed;
            }
            glupsk::step(machine);
            if (should_trace && !options.watches.empty()) {
                print_watches(machine, options.watches);
            }
        }

        std::println("status halted={} blocked={} pc=0x{:08x} sp={} fp={} out={}",
                     machine.halted, machine.blocked, machine.regs.pc,
                     machine.stack.sp, machine.regs.frame_ptr,
                     glk.transcript.size());

        if (options.print_transcript_tail) {
            const auto tail_size = std::min<std::size_t>(glk.transcript.size(), 2000);
            std::println("transcript tail:\n{}",
                         glk.transcript.substr(glk.transcript.size() - tail_size));
        }
    } catch (const std::exception& ex) {
        std::println(stderr, "glupsk-trace: {}", ex.what());
        return 1;
    }

    return 0;
}
