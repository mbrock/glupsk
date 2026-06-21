#include "core/execute.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <cctype>
#include <string>

using namespace glupsk::test;

static suite execute_tests{"Execute", [] {
    auto load_with_transcript = [](const char* path, glupsk::TranscriptGlk& glk) {
        const auto story = glupsk::Story::load(path);
        auto machine = glupsk::Machine::from_story(story);
        machine.glk = &glk;
        return machine;
    };

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

    "blocks and resumes line input for randomgen"_test = [&] {
        auto glk = glupsk::TranscriptGlk{};
        auto machine =
            load_with_transcript("refdata/zarf-glulx/randomgen.ulx", glk);

        auto result = glupsk::run_until_blocked(machine, 10'000);
        expect(result.blocked) << glk.transcript;
        expect(glk.transcript.find(">") != std::string::npos) << glk.transcript;

        glk.add_input_line("help");
        glupsk::resume(machine);
        result = glupsk::run_until_blocked(machine, 10'000);
        expect(result.blocked) << glk.transcript;
        expect(glk.transcript.find(">help") != std::string::npos)
            << glk.transcript;

        glk.add_input_line("quit");
        glupsk::resume(machine);
        result = glupsk::run_until_blocked(machine, 10'000);
        expect(result.halted) << glk.transcript;
        expect(glk.transcript.find("Goodbye!") != std::string::npos)
            << glk.transcript;
    };

    "runs inputfeaturetest with scripted line input"_test = [&] {
        auto glk = glupsk::TranscriptGlk{};
        glk.add_input_line("help");
        glk.add_input_line("quit");
        auto machine =
            load_with_transcript("refdata/zarf-glulx/inputfeaturetest.ulx", glk);

        const auto result = glupsk::run_until_blocked(machine, 10'000);
        expect(result.halted) << glk.transcript;
        expect(!result.blocked) << glk.transcript;
        expect(glk.transcript.find(">help") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("Goodbye!") != std::string::npos)
            << glk.transcript;
    };

    "prints the AA startup text without feature-check fatals"_test = [&] {
        auto glk = glupsk::TranscriptGlk{};
        auto machine = load_with_transcript("refdata/aa.ulx", glk);

        const auto result = glupsk::run_until_blocked(machine, 100'000);
        expect(!result.halted) << glk.transcript;
        expect(glk.transcript.find("November. A Tuesday.") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("Active Approach") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("Fatal Error") == std::string::npos)
            << glk.transcript;
    };
}};
