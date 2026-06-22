#include "core/bytes.hpp"
#include "core/glk_stdio.hpp"
#include "core/glk_transcript.hpp"
#include "core/machine.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <array>
#include <stdexcept>
#include <sstream>

namespace {

glupsk::Bytes synthetic_story_with_extra_memory() {
    glupsk::Bytes bytes(256, 0);
    glupsk::write_u32_be(bytes, 0, glupsk::kGlulxMagic);
    glupsk::write_u32_be(bytes, 4, 0x00030103);
    glupsk::write_u32_be(bytes, 8, 256);
    glupsk::write_u32_be(bytes, 12, static_cast<glupsk::u32>(bytes.size()));
    glupsk::write_u32_be(bytes, 16, 320);
    glupsk::write_u32_be(bytes, 20, 4096);
    glupsk::write_u32_be(bytes, 24, 0x40);
    glupsk::write_u32_be(bytes, 28, 0x80);

    bytes[255] = 0x7f;
    glupsk::write_u32_be(bytes, 32, glupsk::compute_glulx_checksum(bytes));
    return bytes;
}

}  // namespace

using namespace glupsk::test;

static suite machine_tests{"Machine", [] {
    "initializes flat memory from a story"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        const auto machine = glupsk::Machine::from_story(story);

        expect(machine.memory.bytes.size() == 320);
        expect(machine.memory.bytes[0] == 'G');
        expect(machine.memory.bytes[1] == 'l');
        expect(machine.memory.bytes[2] == 'u');
        expect(machine.memory.bytes[3] == 'l');
        expect(machine.memory.bytes[255] == 0x7f);
        expect(machine.memory.ramstart == 256);
        expect(machine.memory.extstart == 256);
        expect(machine.memory.endmem == 320);
    };

    "zero-fills memory after the story image"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        const auto machine = glupsk::Machine::from_story(story);

        for (auto offset = machine.memory.extstart;
             offset < machine.memory.endmem; ++offset) {
            expect(machine.memory.bytes[offset] == 0);
        }
    };

    "initializes stack and registers from the header"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        const auto machine = glupsk::Machine::from_story(story);

        expect(machine.stack.bytes.size() == 4096);
        expect(machine.stack.sp == 0);
        expect(machine.stack.empty());
        expect(machine.stack.capacity() == 4096);
        expect(machine.regs.pc == 0x40);
        expect(machine.regs.string_table == 0x80);
    };

    "constructs initial state for the committed AA story"_test = [] {
        const auto story = glupsk::Story::load("refdata/aa.ulx");
        const auto machine = glupsk::Machine::from_story(story);

        expect(machine.memory.bytes.size() == story.header().endmem);
        expect(machine.memory.ramstart == 669952);
        expect(machine.memory.extstart == 874240);
        expect(machine.memory.endmem == 874240);
        expect(machine.stack.bytes.size() == 65536);
        expect(machine.regs.pc == 0x3c);
        expect(machine.regs.string_table == 0x690b1);
    };

    "reads memory in big-endian order"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);

        machine.memory.bytes[300] = 0x12;
        machine.memory.bytes[301] = 0x34;
        machine.memory.bytes[302] = 0x56;
        machine.memory.bytes[303] = 0x78;

        expect(machine.memory.read8(300) == 0x12);
        expect(machine.memory.read16(300) == 0x1234);
        expect(machine.memory.read32(300) == 0x12345678);
    };

    "writes only to RAM in big-endian order"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);

        machine.memory.write8(256, 0xab);
        machine.memory.write16(258, 0xcdef);
        machine.memory.write32(260, 0x12345678);

        expect(machine.memory.bytes[256] == 0xab);
        expect(machine.memory.bytes[258] == 0xcd);
        expect(machine.memory.bytes[259] == 0xef);
        expect(machine.memory.read32(260) == 0x12345678);
    };

    "rejects out-of-range memory access and ROM writes"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);

        expect(throws<std::runtime_error>(
            [&] { (void) machine.memory.read8(320); }))
            << "read at ENDMEM should fail";
        expect(throws<std::runtime_error>(
            [&] { (void) machine.memory.read32(318); }))
            << "wide read past ENDMEM should fail";
        expect(throws<std::runtime_error>(
            [&] { machine.memory.write8(255, 0); }))
            << "ROM writes should fail";
    };

    "pushes and pops stack words"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);

        machine.stack.push32(0x12345678);
        machine.stack.push32(0xaabbccdd);

        expect(machine.stack.sp == 8);
        expect(machine.stack.bytes[0] == 0x12);
        expect(machine.stack.bytes[1] == 0x34);
        expect(machine.stack.bytes[2] == 0x56);
        expect(machine.stack.bytes[3] == 0x78);
        expect(machine.stack.pop32() == 0xaabbccdd);
        expect(machine.stack.pop32() == 0x12345678);
        expect(machine.stack.empty());
    };

    "rejects stack underflow and overflow"_test = [] {
        auto stack = glupsk::Stack{.bytes = glupsk::Bytes(4)};

        expect(throws<std::runtime_error>([&] { (void) stack.pop32(); }))
            << "empty stack pop should fail";
        stack.push32(0x11223344);
        expect(throws<std::runtime_error>([&] { stack.push32(0x55667788); }))
            << "push past stack capacity should fail";
    };

    "tracks TranscriptGlk windows and streams by handle"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};

        auto open_root_args = std::array<glupsk::u32, 5>{0, 0, 0, 3, 201};
        const auto root = glk.call_returned(machine, 0x23, open_root_args);
        expect(root != 0);
        expect(glk.call_returned(machine, 0x22, {}) == root);
        expect(glk.call_returned(machine, 0x21,
                                 std::array<glupsk::u32, 1>{root}) == 201);

        auto open_status_args = std::array<glupsk::u32, 5>{root, 0x12, 1, 4, 202};
        const auto status = glk.call_returned(machine, 0x23, open_status_args);
        expect(status != 0);
        expect(status != root);

        constexpr auto rock_address = glupsk::u32{256};
        auto iterate_args = std::array<glupsk::u32, 2>{0, rock_address};
        expect(glk.call_returned(machine, 0x20, iterate_args) == root);
        expect(machine.memory.read32(rock_address) == 201);

        iterate_args[0] = root;
        expect(glk.call_returned(machine, 0x20, iterate_args) == status);
        expect(machine.memory.read32(rock_address) == 202);

        iterate_args[0] = status;
        expect(glk.call_returned(machine, 0x20, iterate_args) == 0);
        expect(machine.memory.read32(rock_address) == 0);

        const auto root_stream =
            glk.call_returned(machine, 0x2c, std::array<glupsk::u32, 1>{root});
        expect(root_stream != 0);
        expect(glk.call_returned(machine, 0x41,
                                 std::array<glupsk::u32, 1>{root_stream}) ==
               0);

        auto stream_iterate_args = std::array<glupsk::u32, 2>{0, rock_address};
        expect(glk.call_returned(machine, 0x40, stream_iterate_args) == root_stream);
        expect(machine.memory.read32(rock_address) == 0);
    };

    "rejects unsupported TranscriptGlk selectors and invalid handles"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};

        expect(throws<std::runtime_error>([&] {
            (void) glk.call_returned(machine, 0xffff, {});
        }))
            << "unsupported Glk selectors should be visible";
        expect(throws<std::runtime_error>([&] {
            (void) glk.call_returned(machine, 0x2c, std::array<glupsk::u32, 1>{99});
        })) << "invalid nonzero window handles should fail";
        expect(throws<std::runtime_error>([&] {
            (void) glk.call_returned(machine, 0x41, std::array<glupsk::u32, 1>{99});
        })) << "invalid nonzero stream handles should fail";
    };

    "writes Glk output references to the stack when requested"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};

        const auto stream = glk.call_returned(
            machine, 0x43,
            std::array<glupsk::u32, 4>{machine.memory.ramstart, 8, 1, 0});
        glk.call_returned(machine, 0x81, std::array<glupsk::u32, 2>{stream, 'x'});
        glk.call_returned(machine, 0x44, std::array<glupsk::u32, 2>{stream, 0xffffffffu});

        expect(machine.stack.pop32() == 1);
        expect(machine.stack.pop32() == 0);
    };

    "runs stdio Glk through the common runtime"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto output = std::ostringstream{};
        auto host = glupsk::StdioGlkHost{};
        host.output = &output;
        auto glk = glupsk::StdioGlk{std::move(host)};

        const auto root = glk.call_returned(
            machine, 0x23, std::array<glupsk::u32, 5>{0, 0, 0, 0, 77});
        expect(root != 0);
        expect(glk.call_returned(machine, 0x21,
                                 std::array<glupsk::u32, 1>{root}) == 77);

        const auto stream =
            glk.call_returned(machine, 0x2c, std::array<glupsk::u32, 1>{root});
        expect(stream != 0);
        glk.call_returned(machine, 0x47, std::array<glupsk::u32, 1>{stream});

        const auto text = machine.memory.ramstart;
        machine.memory.write8(text, 'O');
        machine.memory.write8(text + 1, 'K');
        auto result =
            glk.call(machine, 0x84, std::array<glupsk::u32, 2>{text, 2});
        expect(std::holds_alternative<glupsk::GlkReturned>(result));
        expect(output.str() == "OK");

        const auto line_buffer = machine.memory.ramstart + 16;
        const auto event = machine.memory.ramstart + 32;
        glk.call_returned(
            machine, 0xd0,
            std::array<glupsk::u32, 4>{root, line_buffer, 8, 0});
        result = glk.call(machine, 0xc0, std::array<glupsk::u32, 1>{event});
        expect(std::holds_alternative<glupsk::GlkBlocked>(result));
        expect(glk.wants_line_input());

        glk.add_input_line("look");
        result = glk.call(machine, 0xc0, std::array<glupsk::u32, 1>{event});
        expect(std::holds_alternative<glupsk::GlkReturned>(result));
        expect(machine.memory.read32(event) == 3);
        expect(machine.memory.read32(event + 4) == root);
        expect(machine.memory.read32(event + 8) == 4);
        expect(machine.memory.read8(line_buffer) == 'l');
        expect(machine.memory.read8(line_buffer + 1) == 'o');
        expect(machine.memory.read8(line_buffer + 2) == 'o');
        expect(machine.memory.read8(line_buffer + 3) == 'k');
        expect(output.str() == "OK");
    };

    "records transcript runtime writes as materialized text"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};

        const auto root = glk.call_returned(
            machine, 0x23, std::array<glupsk::u32, 5>{0, 0, 0, 0, 9});
        glk.call_returned(machine, 0x2f, std::array<glupsk::u32, 1>{root});

        const auto text = machine.memory.ramstart;
        machine.memory.write8(text, 'H');
        machine.memory.write8(text + 1, 'i');

        auto result =
            glk.call(machine, 0x84, std::array<glupsk::u32, 2>{text, 2});
        expect(std::holds_alternative<glupsk::GlkReturned>(result));
        expect(glk.transcript == "Hi");
        expect(glk.host().writes.size() == 1);
        expect(std::get<std::string>(glk.host().writes.front().text) == "Hi");
    };

    "handles Glk Unicode buffer case selectors"_test = [] {
        const auto story =
            glupsk::Story::from_bytes(synthetic_story_with_extra_memory());
        auto machine = glupsk::Machine::from_story(story);
        auto glk = glupsk::TranscriptGlk{};

        constexpr auto text = glupsk::u32{256};
        machine.memory.write32(text, 'L');
        machine.memory.write32(text + 4, 'o');
        machine.memory.write32(text + 8, 'O');
        machine.memory.write32(text + 12, 'K');

        expect(glk.call_returned(machine, 0x0120,
                                 std::array<glupsk::u32, 3>{text, 4, 4}) ==
               4);
        expect(machine.memory.read32(text) == 'l');
        expect(machine.memory.read32(text + 4) == 'o');
        expect(machine.memory.read32(text + 8) == 'o');
        expect(machine.memory.read32(text + 12) == 'k');

        const auto root = glk.call_returned(
            machine, 0x23, std::array<glupsk::u32, 5>{0, 0, 0, 0, 0});
        glk.call_returned(machine, 0x2f, std::array<glupsk::u32, 1>{root});
        glk.call_returned(machine, 0x0128, std::array<glupsk::u32, 1>{'x'});
        expect(glk.transcript == "x");
    };
}};
