#include "core/decode.hpp"
#include "core/execute.hpp"
#include "core/glk_runtime.hpp"
#include "core/glk_text.hpp"
#include "core/opcode_meta.hpp"
#include "core/story.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <exception>
#include <fstream>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct QuietGlkHost {
    struct Window {};
    struct Stream {};
    struct FileRef {};

    std::deque<glupsk::GlkInputText> input_lines;
    glupsk::u64 bytes_written = 0;

    void add_input_line(std::string line) {
        input_lines.push_back(std::move(line));
    }

    glupsk::u32 gestalt(glupsk::GlkGestaltQuery query) {
        return glupsk::glk_default_gestalt(query);
    }

    glupsk::GlkOpenedWindow<QuietGlkHost> open_window(glupsk::GlkWindowSpec) {
        return {
            .window = Window{},
            .stream = Stream{},
        };
    }

    glupsk::GlkWindowSize window_size(Window&) {
        return {};
    }

    void window_clear(Window&) {}

    void window_move_cursor(Window&, glupsk::u32, glupsk::u32) {}

    void stylehint_set(glupsk::u32, glupsk::GlkStyle, glupsk::u32, glupsk::u32) {}

    glupsk::GlkCallResult write(Stream&,
                                const glupsk::GlkTextData& text,
                                glupsk::GlkStyle) {
        bytes_written += glupsk::glk_text_length(text);
        return glupsk::glk_returned();
    }

    glupsk::GlkEventResult select(glupsk::GlkEventRequest request) {
        for (const auto& interest : request.interests) {
            if (const auto* line = std::get_if<glupsk::GlkLineInputRequest>(&interest)) {
                if (input_lines.empty()) {
                    return glupsk::GlkBlocked{};
                }
                auto text = std::move(input_lines.front());
                input_lines.pop_front();
                return glupsk::GlkHostEvent{glupsk::GlkLineInputEvent{
                    .window = line->window,
                    .text = std::move(text),
                }};
            }
        }
        return glupsk::GlkBlocked{};
    }

    bool echo_line_input() {
        return true;
    }
};

static_assert(glupsk::GlkHost<QuietGlkHost>);

class QuietGlk : public glupsk::GlkSession<QuietGlkHost> {
  public:
    void add_input_line(std::string line) {
        host().add_input_line(std::move(line));
    }

    glupsk::u64 bytes_written() const {
        return host().bytes_written;
    }
};

struct Options {
    const char* story_path = nullptr;
    glupsk::u64 max_steps = 50'000'000;
    std::size_t top = 16;
    std::vector<std::string> input_lines;
};

struct Counters {
    glupsk::u64 steps = 0;
    std::unordered_map<glupsk::u32, glupsk::u64> opcodes;
    std::unordered_map<glupsk::u32, glupsk::u64> pcs;
};

struct Segment {
    std::string label;
    glupsk::u64 steps = 0;
    glupsk::u64 bytes = 0;
    double ms = 0.0;
    bool blocked = false;
    bool halted = false;
};

void usage() {
    std::println(stderr,
                 "usage: glupsk-profile [--input LINE] [--input-file PATH] "
                 "[--max-steps N] [--top N] STORY.ulx");
}

glupsk::u64 parse_u64(std::string_view text) {
    auto value = glupsk::u64{};
    for (const auto ch : text) {
        if (ch < '0' || ch > '9') {
            throw std::runtime_error("invalid integer option");
        }
        value = value * 10 + static_cast<glupsk::u64>(ch - '0');
    }
    return value;
}

void read_input_file(Options& options, const char* path) {
    auto in = std::ifstream{path};
    if (!in) {
        throw std::runtime_error(std::string{"unable to open input file: "} + path);
    }
    auto line = std::string{};
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line.front() == '#') {
            continue;
        }
        options.input_lines.push_back(std::move(line));
    }
}

Options parse_options(int argc, char** argv) {
    auto options = Options{};
    for (auto index = 1; index < argc; ++index) {
        const auto arg = std::string_view{argv[index]};
        auto need_value = [&](std::string_view name) -> const char* {
            if (++index >= argc) {
                throw std::runtime_error(std::string{name} + " requires a value");
            }
            return argv[index];
        };

        if (arg == "--input") {
            options.input_lines.emplace_back(need_value(arg));
        } else if (arg == "--input-file") {
            read_input_file(options, need_value(arg));
        } else if (arg == "--max-steps") {
            options.max_steps = parse_u64(need_value(arg));
        } else if (arg == "--top") {
            options.top = static_cast<std::size_t>(parse_u64(need_value(arg)));
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

template <typename Map>
std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>> sorted_top(
    const Map& values,
    std::size_t count) {
    auto items =
        std::vector<std::pair<typename Map::key_type, typename Map::mapped_type>>{};
    items.reserve(values.size());
    for (const auto& item : values) {
        items.push_back(item);
    }
    std::ranges::sort(items, [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });
    if (items.size() > count) {
        items.resize(count);
    }
    return items;
}

Segment run_segment(glupsk::Machine& machine,
                    QuietGlk& glk,
                    Counters& counters,
                    std::string label,
                    glupsk::u64 max_steps) {
    if (!machine.running) {
        glupsk::start(machine);
    }

    const auto start_bytes = glk.bytes_written();
    const auto started = std::chrono::steady_clock::now();
    auto segment = Segment{.label = std::move(label)};

    while (!machine.halted && !machine.blocked && segment.steps < max_steps) {
        const auto instruction = glupsk::fetch_instruction(machine);
        ++counters.opcodes[static_cast<glupsk::u32>(instruction.opcode)];
        ++counters.pcs[instruction.address];
        glupsk::step(machine);
        ++segment.steps;
    }

    if (machine.glk) {
        machine.glk->flush(machine);
    }

    const auto stopped = std::chrono::steady_clock::now();
    segment.ms = std::chrono::duration<double, std::milli>(stopped - started).count();
    segment.bytes = glk.bytes_written() - start_bytes;
    segment.blocked = machine.blocked;
    segment.halted = machine.halted;
    counters.steps += segment.steps;
    return segment;
}

double steps_per_ms(glupsk::u64 steps, double ms) {
    return ms <= 0.0 ? 0.0 : static_cast<double>(steps) / ms;
}

void print_segment(const Segment& segment) {
    std::println("{:<24} {:>10} steps {:>9.3f} ms {:>10.0f} steps/ms {:>8} bytes "
                 "blocked={} halted={}",
                 segment.label, segment.steps, segment.ms,
                 steps_per_ms(segment.steps, segment.ms), segment.bytes,
                 segment.blocked, segment.halted);
}

void print_opcode_top(const Counters& counters, std::size_t top) {
    std::println("\ntop opcodes");
    for (const auto [opcode, count] : sorted_top(counters.opcodes, top)) {
        auto name = std::string_view{"<unknown>"};
        try {
            name = glupsk::opcode_name(static_cast<glupsk::Opcode>(opcode));
        } catch (const std::exception&) {
        }
        const auto pct = counters.steps == 0
                             ? 0.0
                             : 100.0 * static_cast<double>(count) /
                                   static_cast<double>(counters.steps);
        std::println("  {:<14} 0x{:04x} {:>10} {:>6.2f}%", name, opcode, count, pct);
    }
}

void print_pc_top(const Counters& counters, std::size_t top) {
    std::println("\ntop pcs");
    for (const auto [pc, count] : sorted_top(counters.pcs, top)) {
        const auto pct = counters.steps == 0
                             ? 0.0
                             : 100.0 * static_cast<double>(count) /
                                   static_cast<double>(counters.steps);
        std::println("  0x{:08x} {:>10} {:>6.2f}%", pc, count, pct);
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto story = glupsk::Story::load(options.story_path);
        auto machine = glupsk::Machine::from_story(story);
        auto glk = QuietGlk{};
        machine.glk = &glk;

        auto counters = Counters{};
        auto segments = std::vector<Segment>{};
        segments.push_back(
            run_segment(machine, glk, counters, "<startup>", options.max_steps));

        for (const auto& line : options.input_lines) {
            if (machine.halted) {
                break;
            }
            glk.add_input_line(line);
            glupsk::resume(machine);
            segments.push_back(run_segment(machine, glk, counters, line,
                                           options.max_steps));
        }

        for (const auto& segment : segments) {
            print_segment(segment);
        }
        const auto total_ms = std::ranges::fold_left(
            segments, 0.0, [](double sum, const Segment& segment) {
                return sum + segment.ms;
            });
        std::println("\ntotal {:>17} steps {:>9.3f} ms {:>10.0f} steps/ms",
                     counters.steps, total_ms, steps_per_ms(counters.steps, total_ms));
        print_opcode_top(counters, options.top);
        print_pc_top(counters, options.top);
    } catch (const std::exception& ex) {
        std::println(stderr, "glupsk-profile: {}", ex.what());
        return 1;
    }
    return 0;
}
