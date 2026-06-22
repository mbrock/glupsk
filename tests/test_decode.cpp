#include "core/decode.hpp"
#include "core/machine.hpp"
#include "core/opcode_meta.hpp"

#include "tests/test.hpp"

#include <stdexcept>

namespace {

glupsk::Memory memory_from(glupsk::Bytes bytes) {
    const auto size = static_cast<glupsk::u32>(bytes.size());
    return {
        .bytes = std::move(bytes),
        .ramstart = size,
        .extstart = size,
        .endmem = size,
    };
}

}  // namespace

using namespace glupsk::test;

static suite decode_tests{"Decode", [] {
    "decodes one-byte opcode numbers"_test = [] {
        const auto memory = memory_from({0x00, 0x7f});

        auto opcode = glupsk::decode_opcode_header(memory, 0);
        expect(opcode.address == 0);
        expect(opcode.opcode == 0x00);
        expect(opcode.next_pc == 1);
        expect(opcode.encoded_size == 1);

        opcode = glupsk::decode_opcode_header(memory, 1);
        expect(opcode.address == 1);
        expect(opcode.opcode == 0x7f);
        expect(opcode.next_pc == 2);
        expect(opcode.encoded_size == 1);
    };

    "decodes two-byte opcode numbers"_test = [] {
        const auto memory = memory_from({0x80, 0x01, 0x92, 0x34});

        auto opcode = glupsk::decode_opcode_header(memory, 0);
        expect(opcode.opcode == 0x01);
        expect(opcode.next_pc == 2);
        expect(opcode.encoded_size == 2);

        opcode = glupsk::decode_opcode_header(memory, 2);
        expect(opcode.opcode == 0x1234);
        expect(opcode.next_pc == 4);
        expect(opcode.encoded_size == 2);
    };

    "decodes four-byte opcode numbers"_test = [] {
        const auto memory =
            memory_from({0xc0, 0x00, 0x00, 0x01, 0xca, 0xbc, 0xde, 0xf0});

        auto opcode = glupsk::decode_opcode_header(memory, 0);
        expect(opcode.opcode == 0x01);
        expect(opcode.next_pc == 4);
        expect(opcode.encoded_size == 4);

        opcode = glupsk::decode_opcode_header(memory, 4);
        expect(opcode.opcode == 0x0abcdef0);
        expect(opcode.next_pc == 8);
        expect(opcode.encoded_size == 4);
    };

    "fetches opcode header at machine pc"_test = [] {
        auto machine = glupsk::Machine{};
        machine.memory = memory_from({0x00, 0x80, 0x01});
        machine.regs.pc = 1;

        const auto opcode = glupsk::fetch_opcode_header(machine);
        expect(opcode.address == 1);
        expect(opcode.opcode == 1);
        expect(opcode.next_pc == 3);
    };

    "rejects truncated opcode headers"_test = [] {
        const auto memory = memory_from({0x80});

        expect(throws<std::runtime_error>(
            [&] { (void) glupsk::decode_opcode_header(memory, 0); }))
            << "two-byte opcode header should require two bytes";
    };

    "names opcodes and operand modes for tracing"_test = [] {
        expect(glupsk::opcode_name(glupsk::Opcode::glk) == "glk");
        expect(glupsk::opcode_name(glupsk::Opcode::return_) == "return");
        expect(glupsk::operand_mode_name(glupsk::OperandMode::local8) == "local8");
        expect(glupsk::operand_mode_name(glupsk::OperandMode::ram32) == "ram32");
    };
}};
