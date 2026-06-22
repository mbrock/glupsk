#include "core/decode.hpp"
#include "core/execute.hpp"
#include "core/glk_names.hpp"
#include "core/glk_transcript.hpp"
#include "core/opcode_meta.hpp"
#include "core/story.hpp"
#include "core/vm_operands.hpp"

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
                return std::format(
                    "const8({})",
                    static_cast<std::int32_t>(glupsk::sign_extend(operand.data, 8)));
            case const16:
                return std::format(
                    "const16({})",
                    static_cast<std::int32_t>(glupsk::sign_extend(operand.data, 16)));
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
                const auto value = glupsk::peek_operand(machine, operand, stack_index);
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
                                   glupsk::peek_operand(machine, operand, stack_index));
            }
            case mem8:
            case mem16:
            case mem32:
                return std::format("{}(@0x{:x})=0x{:08x}",
                                   glupsk::operand_mode_name(mode), operand.data,
                                   glupsk::peek_operand(machine, operand, stack_index));
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
                       glupsk::glulx_gestalt_name(operand_values[0]), operand_values[0],
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
                       glupsk::glk_selector_name(selector), selector, argc);
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
                std::print(" gestalt={}", glupsk::glk_gestalt_name(gestalt_selector));
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
            operand_values[index] = glupsk::peek_operand(machine, operand, stack_index);
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
