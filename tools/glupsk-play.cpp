#include "core/execute.hpp"
#include "core/glk_stdio.hpp"
#include "core/story.hpp"

#include <exception>
#include <iostream>
#include <print>
#include <string>
#include <utility>

namespace {

constexpr auto kStepsPerRun = glupsk::u64{50'000'000};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::println(stderr, "usage: glupsk-play STORY.ulx");
        return 64;
    }

    try {
        const auto story = glupsk::Story::load(argv[1]);
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::StdioGlk{};
        machine.glk = &glk;

        while (true) {
            const auto result = glupsk::run_until_blocked(machine, kStepsPerRun);
            if (result.halted) {
                return 0;
            }
            if (!result.blocked) {
                std::println(stderr,
                             "glupsk-play: step budget exhausted without halt or IO");
                return 2;
            }
            if (!glk.wants_line_input()) {
                std::println(stderr,
                             "glupsk-play: story blocked on an unsupported event");
                return 2;
            }

            auto line = std::string{};
            if (!std::getline(std::cin, line)) {
                return 0;
            }
            glk.add_input_line(std::move(line));
            glupsk::resume(machine);
        }
    } catch (const std::exception& ex) {
        std::println(stderr, "glupsk-play: {}", ex.what());
        return 1;
    }
}
