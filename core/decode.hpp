#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

#include <string_view>

namespace glupsk {

enum class Opcode : u32 {
    nop = 0x00,
    add = 0x10,
    sub = 0x11,
    mul = 0x12,
    div = 0x13,
    mod = 0x14,
    neg = 0x15,
    bitand_ = 0x18,
    bitor_ = 0x19,
    bitxor_ = 0x1a,
    bitnot_ = 0x1b,
    shiftl = 0x1c,
    sshiftr = 0x1d,
    ushiftr = 0x1e,
    jump = 0x20,
    jz = 0x22,
    jnz = 0x23,
    jeq = 0x24,
    jne = 0x25,
    jlt = 0x26,
    jge = 0x27,
    jgt = 0x28,
    jle = 0x29,
    jltu = 0x2a,
    jgeu = 0x2b,
    jgtu = 0x2c,
    jleu = 0x2d,
    call = 0x30,
    return_ = 0x31,
    catch_ = 0x32,
    throw_ = 0x33,
    tailcall = 0x34,
    copy = 0x40,
    copys = 0x41,
    copyb = 0x42,
    sexs = 0x44,
    sexb = 0x45,
    aload = 0x48,
    aloads = 0x49,
    aloadb = 0x4a,
    aloadbit = 0x4b,
    astore = 0x4c,
    astores = 0x4d,
    astoreb = 0x4e,
    astorebit = 0x4f,
    stkcount = 0x50,
    stkpeek = 0x51,
    stkswap = 0x52,
    stkroll = 0x53,
    stkcopy = 0x54,
    streamchar = 0x70,
    streamnum = 0x71,
    streamstr = 0x72,
    streamunichar = 0x73,
    gestalt = 0x100,
    debugtrap = 0x101,
    getmemsize = 0x102,
    setmemsize = 0x103,
    jumpabs = 0x104,
    random = 0x110,
    setrandom = 0x111,
    quit = 0x120,
    verify = 0x121,
    restart = 0x122,
    save = 0x123,
    restore = 0x124,
    saveundo = 0x125,
    restoreundo = 0x126,
    protect = 0x127,
    hasundo = 0x128,
    discardundo = 0x129,
    glk = 0x130,
    getstringtbl = 0x140,
    setstringtbl = 0x141,
    getiosys = 0x148,
    setiosys = 0x149,
    linearsearch = 0x150,
    binarysearch = 0x151,
    linkedsearch = 0x152,
    callf = 0x160,
    callfi = 0x161,
    callfii = 0x162,
    callfiii = 0x163,
    mzero = 0x170,
    mcopy = 0x171,
    malloc = 0x178,
    mfree = 0x179,
    accelfunc = 0x180,
    accelparam = 0x181,
};

enum class OperandMode : u8 {
    zero = 0x0,
    const8 = 0x1,
    const16 = 0x2,
    const32 = 0x3,
    mem8 = 0x5,
    mem16 = 0x6,
    mem32 = 0x7,
    stack = 0x8,
    local8 = 0x9,
    local16 = 0xa,
    local32 = 0xb,
    ram8 = 0xd,
    ram16 = 0xe,
    ram32 = 0xf,
};

struct OpcodeHeader {
    u32 address = 0;
    u32 opcode = 0;
    u32 next_pc = 0;
    u8 encoded_size = 0;
};

struct Operand {
    OperandMode mode = OperandMode::zero;
    u32 data = 0;
};

struct Instruction {
    u32 address = 0;
    Opcode opcode = Opcode::nop;
    Operand operands[8] = {};
    u8 operand_count = 0;
    u32 next_pc = 0;
};

OpcodeHeader decode_opcode_header(const Memory& memory, u32 pc);
OpcodeHeader fetch_opcode_header(const Machine& machine);
Instruction decode_instruction(const Memory& memory, u32 pc);
Instruction fetch_instruction(const Machine& machine);
std::string_view operand_mode_name(OperandMode mode);

}  // namespace glupsk
