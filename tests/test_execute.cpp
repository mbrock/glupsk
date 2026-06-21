#include "core/execute.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <cctype>
#include <string>

using namespace glupsk::test;

static suite execute_tests{"Execute", [] {
    "runs dictflagtest to a passing transcript"_test = [] {
        const auto story =
            glupsk::Story::load("refdata/zarf-glulx/dictflagtest.ulx");
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};
        machine.glk = &glk;

        const auto result = glupsk::run_until_halted(machine, 1'000'000);
        auto transcript = glk.transcript;
        while (!transcript.empty() &&
               std::isspace(static_cast<unsigned char>(transcript.back()))) {
            transcript.pop_back();
        }

        expect(result.halted) << "dictflagtest did not halt within step limit";
        expect(transcript.ends_with("Passed.")) << glk.transcript;
        expect(glk.transcript.find("FAILED") == std::string::npos)
            << glk.transcript;
    };
}};
