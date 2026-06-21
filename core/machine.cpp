#include "core/machine.hpp"

namespace glupsk {

Machine Machine::from_story(const Story& story) {
    const auto& header = story.header();
    auto story_bytes = story.bytes();

    Machine machine;
    machine.memory.bytes = Bytes(story_bytes.begin(), story_bytes.end());
    machine.memory.bytes.resize(header.endmem);
    machine.memory.ramstart = header.ramstart;
    machine.memory.extstart = header.extstart;
    machine.memory.endmem = header.endmem;

    machine.stack.bytes.resize(header.stack_size);

    machine.regs.pc = header.start_func;
    machine.regs.string_table = header.decoding_table;

    return machine;
}

}  // namespace glupsk
