#include "core/decode.hpp"

#include "core/opcode_meta.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

u32 read_operand_data(const Memory& memory, u32& pc, OperandMode mode) {
    switch (mode) {
        using enum OperandMode;
        case zero:
        case stack:
            return 0;
        case const8:
        case mem8:
        case local8:
        case ram8: {
            const auto value = memory.read8(pc);
            pc += 1;
            return value;
        }
        case const16:
        case mem16:
        case local16:
        case ram16: {
            const auto value = memory.read16(pc);
            pc += 2;
            return value;
        }
        case const32:
        case mem32:
        case local32:
        case ram32: {
            const auto value = memory.read32(pc);
            pc += 4;
            return value;
        }
        default:
            throw std::runtime_error("unused operand addressing mode");
    }
}

}  // namespace

OpcodeHeader decode_opcode_header(const Memory& memory, u32 pc) {
    const auto first = memory.read8(pc);

    if ((first & 0x80) == 0) {
        return {
            .address = pc,
            .opcode = first,
            .next_pc = pc + 1,
            .encoded_size = 1,
        };
    }

    if ((first & 0xc0) == 0x80) {
        return {
            .address = pc,
            .opcode = static_cast<u32>(memory.read16(pc) & 0x3fff),
            .next_pc = pc + 2,
            .encoded_size = 2,
        };
    }

    return {
        .address = pc,
        .opcode = memory.read32(pc) & 0x0fffffff,
        .next_pc = pc + 4,
        .encoded_size = 4,
    };
}

OpcodeHeader fetch_opcode_header(const Machine& machine) {
    return decode_opcode_header(machine.memory, machine.regs.pc);
}

Instruction decode_instruction(const Memory& memory, u32 pc) {
    const auto header = decode_opcode_header(memory, pc);
    const auto opcode = static_cast<Opcode>(header.opcode);
    Instruction instruction;
    instruction.address = pc;
    instruction.opcode = opcode;
    instruction.operand_count = operand_count_for(opcode);

    auto cursor = header.next_pc;
    OperandMode modes[8] = {};
    for (u8 index = 0; index < instruction.operand_count; index += 2) {
        const auto byte = memory.read8(cursor++);
        modes[index] = static_cast<OperandMode>(byte & 0x0f);
        if (index + 1 < instruction.operand_count) {
            modes[index + 1] = static_cast<OperandMode>((byte >> 4) & 0x0f);
        }
    }

    for (u8 index = 0; index < instruction.operand_count; ++index) {
        instruction.operands[index].mode = modes[index];
        instruction.operands[index].data =
            read_operand_data(memory, cursor, modes[index]);
    }
    instruction.next_pc = cursor;
    return instruction;
}

Instruction fetch_instruction(const Machine& machine) {
    return decode_instruction(machine.memory, machine.regs.pc);
}

std::string_view opcode_name(Opcode opcode) {
    switch (opcode) {
        using enum Opcode;
        case nop: return "nop";
        case add: return "add";
        case sub: return "sub";
        case mul: return "mul";
        case div: return "div";
        case mod: return "mod";
        case neg: return "neg";
        case bitand_: return "bitand";
        case bitor_: return "bitor";
        case bitxor_: return "bitxor";
        case bitnot_: return "bitnot";
        case shiftl: return "shiftl";
        case sshiftr: return "sshiftr";
        case ushiftr: return "ushiftr";
        case jump: return "jump";
        case jz: return "jz";
        case jnz: return "jnz";
        case jeq: return "jeq";
        case jne: return "jne";
        case jlt: return "jlt";
        case jge: return "jge";
        case jgt: return "jgt";
        case jle: return "jle";
        case jltu: return "jltu";
        case jgeu: return "jgeu";
        case jgtu: return "jgtu";
        case jleu: return "jleu";
        case call: return "call";
        case return_: return "return";
        case catch_: return "catch";
        case throw_: return "throw";
        case tailcall: return "tailcall";
        case copy: return "copy";
        case copys: return "copys";
        case copyb: return "copyb";
        case sexs: return "sexs";
        case sexb: return "sexb";
        case aload: return "aload";
        case aloads: return "aloads";
        case aloadb: return "aloadb";
        case aloadbit: return "aloadbit";
        case astore: return "astore";
        case astores: return "astores";
        case astoreb: return "astoreb";
        case astorebit: return "astorebit";
        case stkcount: return "stkcount";
        case stkpeek: return "stkpeek";
        case stkswap: return "stkswap";
        case stkroll: return "stkroll";
        case stkcopy: return "stkcopy";
        case streamchar: return "streamchar";
        case streamnum: return "streamnum";
        case streamstr: return "streamstr";
        case streamunichar: return "streamunichar";
        case gestalt: return "gestalt";
        case debugtrap: return "debugtrap";
        case getmemsize: return "getmemsize";
        case setmemsize: return "setmemsize";
        case jumpabs: return "jumpabs";
        case random: return "random";
        case setrandom: return "setrandom";
        case quit: return "quit";
        case verify: return "verify";
        case restart: return "restart";
        case save: return "save";
        case restore: return "restore";
        case saveundo: return "saveundo";
        case restoreundo: return "restoreundo";
        case protect: return "protect";
        case hasundo: return "hasundo";
        case discardundo: return "discardundo";
        case glk: return "glk";
        case getstringtbl: return "getstringtbl";
        case setstringtbl: return "setstringtbl";
        case getiosys: return "getiosys";
        case setiosys: return "setiosys";
        case linearsearch: return "linearsearch";
        case binarysearch: return "binarysearch";
        case linkedsearch: return "linkedsearch";
        case callf: return "callf";
        case callfi: return "callfi";
        case callfii: return "callfii";
        case callfiii: return "callfiii";
        case mzero: return "mzero";
        case mcopy: return "mcopy";
        case malloc: return "malloc";
        case mfree: return "mfree";
        case accelfunc: return "accelfunc";
        case accelparam: return "accelparam";
        default: return "unknown";
    }
}

std::string_view operand_mode_name(OperandMode mode) {
    switch (mode) {
        using enum OperandMode;
        case zero: return "zero";
        case const8: return "const8";
        case const16: return "const16";
        case const32: return "const32";
        case mem8: return "mem8";
        case mem16: return "mem16";
        case mem32: return "mem32";
        case stack: return "stack";
        case local8: return "local8";
        case local16: return "local16";
        case local32: return "local32";
        case ram8: return "ram8";
        case ram16: return "ram16";
        case ram32: return "ram32";
        default: return "unused";
    }
}

}  // namespace glupsk
