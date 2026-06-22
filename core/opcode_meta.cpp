#include "core/opcode_meta.hpp"

#include "core/error.hpp"

namespace glupsk {

u8 operand_count_for(Opcode opcode) {
    switch (opcode) {
        using enum Opcode;
        case nop:
        case stkswap:
        case quit:
        case restart:
        case discardundo:
            return 0;

        case jump:
        case return_:
        case stkcopy:
        case streamchar:
        case streamnum:
        case streamstr:
        case streamunichar:
        case debugtrap:
        case setrandom:
        case verify:
        case saveundo:
        case restoreundo:
        case hasundo:
        case getstringtbl:
        case setstringtbl:
        case mfree:
            return 1;

        case neg:
        case bitnot_:
        case jz:
        case jnz:
        case catch_:
        case throw_:
        case tailcall:
        case copy:
        case copys:
        case copyb:
        case sexs:
        case sexb:
        case stkcount:
        case stkpeek:
        case stkroll:
        case getmemsize:
        case setmemsize:
        case jumpabs:
        case random:
        case save:
        case restore:
        case protect:
        case getiosys:
        case setiosys:
        case callf:
        case malloc:
        case accelfunc:
        case accelparam:
            return 2;

        case add:
        case sub:
        case mul:
        case div:
        case mod:
        case bitand_:
        case bitor_:
        case bitxor_:
        case shiftl:
        case sshiftr:
        case ushiftr:
        case jeq:
        case jne:
        case jlt:
        case jge:
        case jgt:
        case jle:
        case jltu:
        case jgeu:
        case jgtu:
        case jleu:
        case call:
        case aload:
        case aloads:
        case aloadb:
        case aloadbit:
        case astore:
        case astores:
        case astoreb:
        case astorebit:
        case gestalt:
        case glk:
        case callfi:
        case mzero:
        case mcopy:
            return 3;

        case callfii:
            return 4;
        case callfiii:
            return 5;
        case linkedsearch:
            return 7;
        case linearsearch:
        case binarysearch:
            return 8;
        default:
            fail("unknown opcode");
    }
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

bool is_store_operand(Opcode opcode, u8 index) {
    switch (opcode) {
        using enum Opcode;
        case add:
        case sub:
        case mul:
        case div:
        case mod:
        case bitand_:
        case bitor_:
        case bitxor_:
        case shiftl:
        case sshiftr:
        case ushiftr:
        case aload:
        case aloads:
        case aloadb:
        case aloadbit:
        case gestalt:
        case glk:
            return index == 2;

        case neg:
        case bitnot_:
        case copy:
        case copys:
        case copyb:
        case sexs:
        case sexb:
        case stkpeek:
        case getmemsize:
        case setmemsize:
        case random:
        case save:
        case restore:
        case malloc:
            return index == 1;

        case catch_:
        case stkcount:
        case verify:
        case saveundo:
        case restoreundo:
        case hasundo:
        case getstringtbl:
            return index == 0;

        case getiosys:
            return index == 0 || index == 1;

        case call:
            return index == 2;
        case callf:
            return index == 1;
        case callfi:
            return index == 2;
        case callfii:
            return index == 3;
        case callfiii:
            return index == 4;

        case linearsearch:
        case binarysearch:
            return index == 7;
        case linkedsearch:
            return index == 6;

        default:
            return false;
    }
}

u8 load_width_for(Opcode opcode, u8 index) {
    if (index == 0 && opcode == Opcode::copys) {
        return 2;
    }
    if (index == 0 && opcode == Opcode::copyb) {
        return 1;
    }
    return 4;
}

}  // namespace glupsk
