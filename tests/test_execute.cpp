#include "core/bytes.hpp"
#include "core/decode.hpp"
#include "core/execute.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace {

struct EncodedOperand {
    glupsk::OperandMode mode = glupsk::OperandMode::zero;
    glupsk::u32 data = 0;
};

class SyntheticStory {
  public:
    SyntheticStory() : bytes_(256, 0) {
        glupsk::write_u32_be(bytes_, 0, glupsk::kGlulxMagic);
        glupsk::write_u32_be(bytes_, 4, 0x00030103);
        glupsk::write_u32_be(bytes_, 8, 256);
        glupsk::write_u32_be(bytes_, 12,
                             static_cast<glupsk::u32>(bytes_.size()));
        glupsk::write_u32_be(bytes_, 16, 320);
        glupsk::write_u32_be(bytes_, 20, 4096);
        glupsk::write_u32_be(bytes_, 24, 0x40);
        glupsk::write_u32_be(bytes_, 28, 0x80);

        bytes_[cursor_++] = 0xc1;
        bytes_[cursor_++] = 0;
        bytes_[cursor_++] = 0;
    }

    void emit(glupsk::Opcode opcode,
              std::initializer_list<EncodedOperand> operands = {}) {
        emit_opcode(static_cast<glupsk::u32>(opcode));
        auto it = operands.begin();
        while (it != operands.end()) {
            auto modes = static_cast<glupsk::u8>(it->mode);
            ++it;
            if (it != operands.end()) {
                modes |= static_cast<glupsk::u8>(static_cast<glupsk::u8>(it->mode)
                                                 << 4);
                ++it;
            }
            bytes_[cursor_++] = modes;
        }
        for (const auto operand : operands) {
            emit_operand_data(operand);
        }
    }

    glupsk::Bytes finish() {
        emit(glupsk::Opcode::quit);
        glupsk::write_u32_be(bytes_, 32, glupsk::compute_glulx_checksum(bytes_));
        return bytes_;
    }

  private:
    void emit_opcode(glupsk::u32 opcode) {
        if (opcode <= 0x7f) {
            bytes_[cursor_++] = static_cast<glupsk::u8>(opcode);
            return;
        }
        if (opcode <= 0x3fff) {
            glupsk::write_u16_be(bytes_, cursor_,
                                  static_cast<glupsk::u16>(0x8000u | opcode));
            cursor_ += 2;
            return;
        }
        glupsk::write_u32_be(bytes_, cursor_, 0xc0000000u | opcode);
        cursor_ += 4;
    }

    void emit_operand_data(EncodedOperand operand) {
        switch (operand.mode) {
            using enum glupsk::OperandMode;
            case zero:
            case stack:
                return;
            case const8:
            case mem8:
            case local8:
            case ram8:
                bytes_[cursor_++] = static_cast<glupsk::u8>(operand.data);
                return;
            case const16:
            case mem16:
            case local16:
            case ram16:
                glupsk::write_u16_be(bytes_, cursor_,
                                      static_cast<glupsk::u16>(operand.data));
                cursor_ += 2;
                return;
            case const32:
            case mem32:
            case local32:
            case ram32:
                glupsk::write_u32_be(bytes_, cursor_, operand.data);
                cursor_ += 4;
                return;
            default:
                throw std::runtime_error("unsupported synthetic operand mode");
        }
    }

    glupsk::Bytes bytes_;
    std::size_t cursor_ = 0x40;
};

EncodedOperand stack() {
    return {.mode = glupsk::OperandMode::stack};
}

EncodedOperand const8(glupsk::u32 value) {
    return {.mode = glupsk::OperandMode::const8, .data = value};
}

EncodedOperand const16(glupsk::u32 value) {
    return {.mode = glupsk::OperandMode::const16, .data = value};
}

EncodedOperand ram32(glupsk::u32 offset) {
    return {.mode = glupsk::OperandMode::ram32, .data = offset};
}

glupsk::Bytes story_with_stack_operand_subtraction() {
    SyntheticStory story;
    story.emit(glupsk::Opcode::copy, {const8(10), stack()});
    story.emit(glupsk::Opcode::copy, {const8(3), stack()});
    story.emit(glupsk::Opcode::sub, {stack(), stack(), ram32(0)});
    return story.finish();
}

glupsk::Bytes story_with_memory_resize() {
    SyntheticStory story;
    story.emit(glupsk::Opcode::setmemsize, {const16(512), ram32(0)});
    story.emit(glupsk::Opcode::getmemsize, {ram32(4)});
    story.emit(glupsk::Opcode::setmemsize, {const16(384), ram32(8)});
    return story.finish();
}

glupsk::Bytes story_with_random_sequence() {
    SyntheticStory story;
    story.emit(glupsk::Opcode::setrandom, {const8(123)});
    story.emit(glupsk::Opcode::random, {const8(10), ram32(0)});
    story.emit(glupsk::Opcode::random, {const8(static_cast<glupsk::u8>(-5)),
                                        ram32(4)});
    story.emit(glupsk::Opcode::setrandom, {const8(123)});
    story.emit(glupsk::Opcode::random, {const8(10), ram32(8)});
    return story.finish();
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

    "resizes memory with setmemsize"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(story_with_memory_resize());
        auto machine = glupsk::Machine::from_story(story);

        const auto result = glupsk::run_until_halted(machine, 100);
        expect(result.halted) << "resize fixture did not halt";
        expect(machine.memory.read32(machine.memory.ramstart) == 0)
            << "setmemsize 512 should succeed";
        expect(machine.memory.read32(machine.memory.ramstart + 4) == 512)
            << "getmemsize should report the grown size";
        expect(machine.memory.read32(machine.memory.ramstart + 8) == 1)
            << "setmemsize 384 should fail because it is not 256-aligned";
        expect(machine.memory.bytes.size() == 512)
            << "failed resize should leave memory size unchanged";
    };

    "generates deterministic random sequences"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(story_with_random_sequence());
        auto machine = glupsk::Machine::from_story(story);

        const auto result = glupsk::run_until_halted(machine, 100);
        expect(result.halted) << "random fixture did not halt";
        expect(machine.memory.read32(machine.memory.ramstart) == 8)
            << "first seeded random value modulo 10";
        expect(machine.memory.read32(machine.memory.ramstart + 4) == 0xffffffffu)
            << "negative random range should produce a value from -4 through 0";
        expect(machine.memory.read32(machine.memory.ramstart + 8) ==
               machine.memory.read32(machine.memory.ramstart))
            << "re-seeding should reproduce the same sequence";
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

    "runs an AA opening command script"_test = [&] {
        auto glk = glupsk::TranscriptGlk{};
        glk.add_input_line("south");
        glk.add_input_line("south");
        glk.add_input_line("look");
        glk.add_input_line("listen");
        glk.add_input_line("quit");
        glk.add_input_line("y");
        auto machine = load_with_transcript("refdata/aa.ulx", glk);

        const auto result = glupsk::run_until_blocked(machine, 20'000'000);
        expect(result.halted) << glk.transcript;
        expect(!result.blocked) << glk.transcript;
        expect(glk.transcript.find("Carlyle Lobby") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("Bemelmans Bar") != std::string::npos)
            << glk.transcript;
        expect(glk.transcript.find("You listen.") != std::string::npos)
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
