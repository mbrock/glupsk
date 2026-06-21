#include "core/bytes.hpp"
#include "core/machine.hpp"
#include "core/story.hpp"

#include "tests/test.hpp"

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
}};
