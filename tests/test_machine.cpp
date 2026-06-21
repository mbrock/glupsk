#include "core/bytes.hpp"
#include "core/machine.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

#include <stdexcept>

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
}};
