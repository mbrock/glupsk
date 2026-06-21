#include "core/bytes.hpp"
#include "core/execute.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <cctype>
#include <string>

namespace {

glupsk::Bytes story_with_stack_operand_subtraction() {
    auto bytes = glupsk::Bytes(256, 0);
    glupsk::write_u32_be(bytes, 0, glupsk::kGlulxMagic);
    glupsk::write_u32_be(bytes, 4, 0x00030103);
    glupsk::write_u32_be(bytes, 8, 256);
    glupsk::write_u32_be(bytes, 12, static_cast<glupsk::u32>(bytes.size()));
    glupsk::write_u32_be(bytes, 16, 320);
    glupsk::write_u32_be(bytes, 20, 4096);
    glupsk::write_u32_be(bytes, 24, 0x40);
    glupsk::write_u32_be(bytes, 28, 0x80);

    auto cursor = std::size_t{0x40};
    bytes[cursor++] = 0xc1;
    bytes[cursor++] = 0;
    bytes[cursor++] = 0;

    bytes[cursor++] = 0x40;
    bytes[cursor++] = 0x81;
    bytes[cursor++] = 10;

    bytes[cursor++] = 0x40;
    bytes[cursor++] = 0x81;
    bytes[cursor++] = 3;

    bytes[cursor++] = 0x11;
    bytes[cursor++] = 0x88;
    bytes[cursor++] = 0x0f;
    glupsk::write_u32_be(bytes, cursor, 0);
    cursor += 4;

    glupsk::write_u16_be(bytes, cursor, 0x8120);

    glupsk::write_u32_be(bytes, 32, glupsk::compute_glulx_checksum(bytes));
    return bytes;
}

}  // namespace

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

    "evaluates stack operands from left to right"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(story_with_stack_operand_subtraction());
        auto machine = glupsk::Machine::from_story(story);

        const auto result = glupsk::run_until_halted(machine, 100);
        expect(result.halted);
        expect(machine.memory.read32(machine.memory.ramstart) == 0xfffffff9u);
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

    "runs the tiny Inform 7 command script"_test = [&] {
        auto glk = glupsk::TranscriptGlk{};
        glk.add_input_line("look");
        glk.add_input_line("inventory");
        glk.add_input_line("take apple");
        glk.add_input_line("drop apple");
        glk.add_input_line("examine apple");
        auto machine = load_with_transcript("refdata/tiny-i7/apple.ulx", glk);

        const auto result = glupsk::run_until_blocked(machine, 1'200'000);
        expect(result.blocked) << glk.transcript;
        expect(glk.transcript.find("That's not a verb I recognise") ==
               std::string::npos) << glk.transcript;
        expect(glk.transcript.find("You are carrying nothing.") !=
               std::string::npos) << glk.transcript;
        expect(glk.transcript.find("Taken.") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("Dropped.") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("A crisp red apple.") != std::string::npos)
            << glk.transcript;
    };
}};
