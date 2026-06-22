#include "core/opcode_meta.hpp"

#include <stdexcept>

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
            throw std::runtime_error("unknown opcode");
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
