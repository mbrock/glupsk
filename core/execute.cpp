#include "core/execute.hpp"

#include "core/decode.hpp"
#include "core/error.hpp"
#include "core/opcode_meta.hpp"
#include "core/vm_accel.hpp"
#include "core/vm_frames.hpp"
#include "core/vm_operands.hpp"
#include "core/vm_search.hpp"
#include "core/vm_strings.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <variant>

namespace glupsk {
namespace {

enum class GestaltSelector : u32 {
    glulx_version = 0,
    terp_version = 1,
    resize_mem = 2,
    undo = 3,
    io_system = 4,
    unicode = 5,
    mem_copy = 6,
    malloc_ = 7,
    malloc_heap = 8,
    acceleration = 9,
    accel_func = 10,
    float_ = 11,
    ext_undo = 12,
    double_ = 13,
};

u32 glk_char_to_lower(u32 ch) {
    if ((ch >= 0x41 && ch <= 0x5a) || (ch >= 0xc0 && ch <= 0xd6) ||
        (ch >= 0xd8 && ch <= 0xde)) {
        return ch + 0x20;
    }
    return ch;
}

u32 glk_char_to_upper(u32 ch) {
    if ((ch >= 0x61 && ch <= 0x7a) || (ch >= 0xe0 && ch <= 0xf6) ||
        (ch >= 0xf8 && ch <= 0xfe)) {
        return ch - 0x20;
    }
    return ch;
}

u32 next_random(Machine& machine) {
    machine.rng_state = machine.rng_state * 1664525u + 1013904223u;
    return machine.rng_state;
}

u32 random_in_range(Machine& machine, u32 bound) {
    const auto value = next_random(machine);
    if (bound == 0) {
        return value;
    }
    if ((bound & 0x80000000u) == 0) {
        return value % bound;
    }
    const auto count = 0u - bound;
    return 0u - (value % count);
}

std::int32_t as_signed(u32 value) {
    return static_cast<std::int32_t>(value);
}

void swap_stack_words(Stack& stack, u32 left, u32 right) {
    const auto value = stack.read32(left);
    stack.write32(left, stack.read32(right));
    stack.write32(right, value);
}

void reverse_stack_words(Stack& stack, u32 start, u32 count) {
    if (count == 0) {
        return;
    }

    auto left = start;
    auto right = start + (count - 1) * 4;
    while (left < right) {
        swap_stack_words(stack, left, right);
        left += 4;
        right -= 4;
    }
}

u32 gestalt(u32 selector, u32 arg) {
    switch (static_cast<GestaltSelector>(selector)) {
        case GestaltSelector::glulx_version:
            return 0x00030103;
        case GestaltSelector::terp_version:
            return 0x00000001;
        case GestaltSelector::undo:
        case GestaltSelector::malloc_:
        case GestaltSelector::ext_undo:
            return 0;
        case GestaltSelector::resize_mem:
            return 1;
        case GestaltSelector::io_system:
            return (arg == 0 || arg == 1 || arg == 2) ? 1 : 0;
        case GestaltSelector::unicode:
        case GestaltSelector::mem_copy:
        case GestaltSelector::acceleration:
        case GestaltSelector::float_:
        case GestaltSelector::double_:
            return 1;
        case GestaltSelector::malloc_heap:
            return 0;
        case GestaltSelector::accel_func:
            return accel_function_supported(arg) ? 1 : 0;
        default:
            return 0;
    }
}

void execute_instruction(Machine& machine, const Instruction& insn) {
    machine.regs.pc = insn.next_pc;
    const auto op = insn.opcode;
    const auto& a = insn.operands;
    auto values = std::array<u32, 8>{};
    for (u8 index = 0; index < insn.operand_count; ++index) {
        if (!is_store_operand(op, index)) {
            values[index] = load_operand(machine, a[index], load_width_for(op, index));
        }
    }

    auto l = [&](int index) {
        return values[static_cast<std::size_t>(index)];
    };
    auto s = [&](int index, u32 value, u8 width = 4) {
        store_value(machine, a[index], value, width);
    };
    auto b = [&](bool condition, u32 offset) {
        if (condition) {
            branch_to(machine, insn.next_pc, offset);
        }
    };

    switch (op) {
        case Opcode::nop:
            return;
        case Opcode::add:
            s(2, l(0) + l(1));
            return;
        case Opcode::sub:
            s(2, l(0) - l(1));
            return;
        case Opcode::mul:
            s(2, l(0) * l(1));
            return;
        case Opcode::div:
            s(2, static_cast<u32>(as_signed(l(0)) / as_signed(l(1))));
            return;
        case Opcode::mod:
            s(2, static_cast<u32>(as_signed(l(0)) % as_signed(l(1))));
            return;
        case Opcode::neg:
            s(1, -l(0));
            return;
        case Opcode::bitand_:
            s(2, l(0) & l(1));
            return;
        case Opcode::bitor_:
            s(2, l(0) | l(1));
            return;
        case Opcode::bitxor_:
            s(2, l(0) ^ l(1));
            return;
        case Opcode::bitnot_:
            s(1, ~l(0));
            return;
        case Opcode::shiftl: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) << amount);
            return;
        }
        case Opcode::sshiftr: {
            const auto value = l(0);
            const auto amount = l(1);
            if (amount >= 32) {
                s(2, (value & 0x80000000u) ? 0xffffffffu : 0);
            } else {
                s(2, static_cast<u32>(as_signed(value) >> amount));
            }
            return;
        }
        case Opcode::ushiftr: {
            const auto amount = l(1);
            s(2, amount >= 32 ? 0 : l(0) >> amount);
            return;
        }
        case Opcode::jump:
            branch_to(machine, insn.next_pc, l(0));
            return;
        case Opcode::jz:
            b(l(0) == 0, l(1));
            return;
        case Opcode::jnz:
            b(l(0) != 0, l(1));
            return;
        case Opcode::jeq:
            b(l(0) == l(1), l(2));
            return;
        case Opcode::jne:
            b(l(0) != l(1), l(2));
            return;
        case Opcode::jlt:
            b(as_signed(l(0)) < as_signed(l(1)), l(2));
            return;
        case Opcode::jge:
            b(as_signed(l(0)) >= as_signed(l(1)), l(2));
            return;
        case Opcode::jgt:
            b(as_signed(l(0)) > as_signed(l(1)), l(2));
            return;
        case Opcode::jle:
            b(as_signed(l(0)) <= as_signed(l(1)), l(2));
            return;
        case Opcode::jltu:
            b(l(0) < l(1), l(2));
            return;
        case Opcode::jgeu:
            b(l(0) >= l(1), l(2));
            return;
        case Opcode::jgtu:
            b(l(0) > l(1), l(2));
            return;
        case Opcode::jleu:
            b(l(0) <= l(1), l(2));
            return;
        case Opcode::call: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            const auto args = pop_arguments(machine, argc);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args.as_span());
            return;
        }
        case Opcode::return_:
            finish_return(machine, l(0));
            return;
        case Opcode::catch_: {
            const auto offset = l(1);
            const auto dest = store_ref(machine, a[0]);
            push_call_stub(machine, dest, insn.next_pc);
            const auto token = machine.stack.sp;
            store_value(machine, dest, token);
            branch_to(machine, insn.next_pc, offset);
            return;
        }
        case Opcode::throw_: {
            const auto value = l(0);
            const auto token = l(1);
            machine.stack.sp = token;
            pop_call_stub(machine, value);
            return;
        }
        case Opcode::tailcall: {
            const auto fn = l(0);
            const auto argc = l(1);
            const auto args = pop_arguments(machine, argc);
            machine.stack.sp = machine.regs.frame_ptr;
            enter_function(machine, fn, args.as_span());
            return;
        }
        case Opcode::copy:
            s(1, l(0));
            return;
        case Opcode::copys:
            s(1, l(0), 2);
            return;
        case Opcode::copyb:
            s(1, l(0), 1);
            return;
        case Opcode::sexs:
            s(1, sign_extend(l(0) & 0xffff, 16));
            return;
        case Opcode::sexb:
            s(1, sign_extend(l(0) & 0xff, 8));
            return;
        case Opcode::aload:
            s(2, machine.memory.read32(l(0) + 4 * as_signed(l(1))));
            return;
        case Opcode::aloads:
            s(2, machine.memory.read16(l(0) + 2 * as_signed(l(1))));
            return;
        case Opcode::aloadb:
            s(2, machine.memory.read8(l(0) + as_signed(l(1))));
            return;
        case Opcode::aloadbit: {
            const auto bit_index = as_signed(l(1));
            auto address = l(0) + bit_index / 8;
            auto bit = bit_index % 8;
            if (bit < 0) {
                --address;
                bit += 8;
            }
            s(2, (machine.memory.read8(address) >> bit) & 1);
            return;
        }
        case Opcode::astore:
            machine.memory.write32(l(0) + 4 * as_signed(l(1)), l(2));
            return;
        case Opcode::astores:
            machine.memory.write16(l(0) + 2 * as_signed(l(1)),
                                   static_cast<u16>(l(2)));
            return;
        case Opcode::astoreb:
            machine.memory.write8(l(0) + as_signed(l(1)), static_cast<u8>(l(2)));
            return;
        case Opcode::astorebit: {
            const auto value = l(2);
            const auto bit_index = as_signed(l(1));
            auto address = l(0) + bit_index / 8;
            auto bit = bit_index % 8;
            if (bit < 0) {
                --address;
                bit += 8;
            }
            auto byte = machine.memory.read8(address);
            if (value) {
                byte |= static_cast<u8>(1u << bit);
            } else {
                byte &= static_cast<u8>(~(1u << bit));
            }
            machine.memory.write8(address, byte);
            return;
        }
        case Opcode::stkcount:
            s(0, (machine.stack.sp - value_stack_base(machine)) / 4);
            return;
        case Opcode::stkpeek: {
            const auto index = l(0);
            s(1, machine.stack.read32(machine.stack.sp - 4 - 4 * index));
            return;
        }
        case Opcode::stkswap: {
            const auto first = machine.stack.pop32();
            const auto second = machine.stack.pop32();
            machine.stack.push32(first);
            machine.stack.push32(second);
            return;
        }
        case Opcode::stkroll: {
            const auto count = l(0);
            const auto places_signed = as_signed(l(1));
            if (count == 0 || places_signed == 0) {
                return;
            }
            auto places = places_signed % static_cast<std::int32_t>(count);
            if (places < 0) {
                places += static_cast<std::int32_t>(count);
            }
            const auto start = machine.stack.sp - count * 4;
            reverse_stack_words(machine.stack, start, count);
            reverse_stack_words(machine.stack, start, static_cast<u32>(places));
            reverse_stack_words(machine.stack, start + static_cast<u32>(places) * 4,
                                count - static_cast<u32>(places));
            return;
        }
        case Opcode::stkcopy: {
            const auto count = l(0);
            const auto start = machine.stack.sp - count * 4;
            for (u32 index = 0; index < count; ++index) {
                machine.stack.push32(machine.stack.read32(start + index * 4));
            }
            return;
        }
        case Opcode::streamchar:
            output_char(machine, l(0) & 0xff);
            return;
        case Opcode::streamnum:
        {
            auto buffer = std::array<char, 16>{};
            const auto value = as_signed(l(0));
            const auto [end, ec] =
                std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (ec != std::errc{}) {
                fail("streamnum conversion failed");
            }
            for (auto cursor = buffer.data(); cursor != end; ++cursor) {
                const auto ch = *cursor;
                output_char(machine, static_cast<u8>(ch));
            }
            return;
        }
        case Opcode::streamstr:
            output_string_object(machine, l(0));
            return;
        case Opcode::streamunichar:
            output_char(machine, l(0));
            return;
        case Opcode::gestalt:
            s(2, gestalt(l(0), l(1)));
            return;
        case Opcode::debugtrap:
            fail("debugtrap");
        case Opcode::getmemsize:
            s(0, static_cast<u32>(machine.memory.bytes.size()));
            return;
        case Opcode::setmemsize: {
            const auto new_size = l(0);
            if ((new_size & 0xffu) != 0 || new_size < machine.memory.endmem) {
                s(1, 1);
                return;
            }
            machine.memory.bytes.resize(new_size);
            s(1, 0);
            return;
        }
        case Opcode::jumpabs:
            machine.regs.pc = l(0);
            return;
        case Opcode::random:
            s(1, random_in_range(machine, l(0)));
            return;
        case Opcode::setrandom:
            machine.rng_state = l(0) == 0 ? 0x6d2b79f5 : l(0);
            return;
        case Opcode::quit:
            machine.halted = true;
            machine.running = false;
            return;
        case Opcode::verify:
            s(0, 0);
            return;
        case Opcode::saveundo:
        case Opcode::restoreundo:
        case Opcode::hasundo:
            s(0, 1);
            return;
        case Opcode::protect:
        case Opcode::discardundo:
            return;
        case Opcode::glk: {
            const auto selector = l(0);
            const auto argc = l(1);
            const auto dest = store_ref(machine, a[2]);
            auto args = pop_arguments(machine, argc);
            auto result = u32{0};
            if (static_cast<GlkSelector>(selector) == GlkSelector::char_to_lower &&
                args.count == 1) {
                result = glk_char_to_lower(args.values[0]);
            } else if (static_cast<GlkSelector>(selector) == GlkSelector::char_to_upper &&
                       args.count == 1) {
                result = glk_char_to_upper(args.values[0]);
            } else if (machine.glk) {
                const auto call_result =
                    machine.glk->call(machine, selector, args.as_span());
                if (std::holds_alternative<GlkBlocked>(call_result)) {
                    restore_arguments(machine, args);
                    machine.regs.pc = insn.address;
                    machine.blocked = true;
                    return;
                }
                if (const auto* fatal = std::get_if<GlkFatal>(&call_result)) {
                    fail(fatal->message);
                }
                result = std::get<GlkReturned>(call_result).value;
            }
            store_value(machine, dest, result);
            return;
        }
        case Opcode::getstringtbl:
            s(0, machine.regs.string_table);
            return;
        case Opcode::setstringtbl:
            machine.regs.string_table = l(0);
            return;
        case Opcode::getiosys:
            store_value(machine, a[0], machine.regs.iosys_mode);
            store_value(machine, a[1], machine.regs.iosys_rock);
            return;
        case Opcode::setiosys:
            machine.regs.iosys_mode = l(0);
            machine.regs.iosys_rock = l(1);
            if (machine.regs.iosys_mode != 0 && machine.regs.iosys_mode != 2) {
                machine.regs.iosys_mode = 0;
            }
            return;
        case Opcode::linearsearch:
            s(7, linear_search(machine, l(0), l(1), l(2), l(3), l(4), l(5),
                                l(6)));
            return;
        case Opcode::binarysearch:
            s(7, binary_search(machine, l(0), l(1), l(2), l(3), l(4), l(5), l(6)));
            return;
        case Opcode::linkedsearch:
            s(6, linked_search(machine, l(0), l(1), l(2), l(3), l(4), l(5)));
            return;
        case Opcode::callf: {
            const auto fn = l(0);
            const auto dest = store_ref(machine, a[1]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, span<const u32>{});
            return;
        }
        case Opcode::callfi: {
            const auto fn = l(0);
            const auto args = std::array<u32, 1>{l(1)};
            const auto dest = store_ref(machine, a[2]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case Opcode::callfii: {
            const auto fn = l(0);
            const auto args = std::array<u32, 2>{l(1), l(2)};
            const auto dest = store_ref(machine, a[3]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case Opcode::callfiii: {
            const auto fn = l(0);
            const auto args = std::array<u32, 3>{l(1), l(2), l(3)};
            const auto dest = store_ref(machine, a[4]);
            push_call_stub(machine, dest, insn.next_pc);
            enter_function(machine, fn, args);
            return;
        }
        case Opcode::mzero: {
            const auto count = l(0);
            const auto address = l(1);
            for (u32 offset = 0; offset < count; ++offset) {
                machine.memory.write8(address + offset, 0);
            }
            return;
        }
        case Opcode::mcopy: {
            const auto count = l(0);
            const auto source = l(1);
            const auto dest = l(2);
            if (dest < source) {
                for (u32 offset = 0; offset < count; ++offset) {
                    machine.memory.write8(dest + offset,
                                          machine.memory.read8(source + offset));
                }
            } else {
                for (u32 offset = count; offset > 0; --offset) {
                    machine.memory.write8(
                        dest + offset - 1,
                        machine.memory.read8(source + offset - 1));
                }
            }
            return;
        }
        case Opcode::malloc:
            fail("malloc is not implemented");
        case Opcode::mfree:
            fail("mfree is not implemented");
        case Opcode::accelfunc:
            set_accelerated_function(machine, l(0), l(1));
            return;
        case Opcode::accelparam:
            set_acceleration_param(machine, l(0), l(1));
            return;
        default:
            fail("unimplemented opcode");
    }
}

}  // namespace

void start(Machine& machine) {
    if (machine.running) {
        return;
    }
    const auto start_func = machine.regs.pc;
    machine.stack.sp = 0;
    machine.regs.frame_ptr = 0;
    push_call_stub(machine, {.type = StoreDest::discard, .address = 0}, 0);
    enter_function(machine, start_func, span<const u32>{});
    machine.running = true;
    machine.blocked = false;
    machine.halted = false;
}

void resume(Machine& machine) {
    if (!machine.halted) {
        machine.blocked = false;
    }
}

void step(Machine& machine) {
    if (!machine.running) {
        start(machine);
    }
    if (machine.halted || machine.blocked) {
        return;
    }
    const auto insn = fetch_instruction(machine);
    execute_instruction(machine, insn);
}

RunResult run_until_blocked(Machine& machine, u64 max_steps) {
    RunResult result;
    if (!machine.running) {
        start(machine);
    }
    while (!machine.halted && !machine.blocked && result.steps < max_steps) {
        step(machine);
        ++result.steps;
    }
    // Drain output buffered since the last @glk call so a halt (VM quit) or a
    // blocked input prompt is visible before control returns to the host.
    if (machine.glk) {
        machine.glk->flush(machine);
    }
    result.blocked = machine.blocked;
    result.halted = machine.halted;
    return result;
}

RunResult run_until_halted(Machine& machine, u64 max_steps) {
    return run_until_blocked(machine, max_steps);
}

}  // namespace glupsk
