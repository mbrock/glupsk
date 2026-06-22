#include "core/vm_accel.hpp"

#include "core/vm_search.hpp"

#include <stdexcept>

namespace glupsk {
namespace {

u32 accel_arg(span<const u32> args, std::size_t index) {
    return index < args.size() ? args[index] : 0;
}

bool accel_obj_in_class(const Machine& machine, u32 obj) {
    return machine.memory.read32(obj + 13 + machine.accel_params[7]) ==
           machine.accel_params[2];
}

u32 accel_z_region(const Machine& machine, u32 address) {
    if (address < 36 ||
        static_cast<std::size_t>(address) >= machine.memory.bytes.size()) {
        return 0;
    }
    const auto type = machine.memory.read8(address);
    if (type >= 0xe0) {
        return 3;
    }
    if (type >= 0xc0) {
        return 2;
    }
    if (type >= 0x70 && type <= 0x7f && address >= machine.memory.ramstart) {
        return 1;
    }
    return 0;
}

u32 accel_cp_tab_at(Machine& machine, u32 obj, u32 id, u32 prop_table_offset) {
    if (accel_z_region(machine, obj) != 1) {
        return 0;
    }
    auto table = machine.memory.read32(obj + prop_table_offset);
    if (table == 0) {
        return 0;
    }
    const auto count = machine.memory.read32(table);
    table += 4;
    return binary_search(machine, id, 2, table, 10, count, 0, 0);
}

u32 accel_cp_tab(Machine& machine, u32 obj, u32 id) {
    return accel_cp_tab_at(machine, obj, id, 16);
}

u32 accel_cp_tab_new(Machine& machine, u32 obj, u32 id) {
    const auto offset = 4 * (3 + machine.accel_params[7] / 4);
    return accel_cp_tab_at(machine, obj, id, offset);
}

u32 accel_get_prop(Machine& machine, u32 obj, u32 id, bool newer);
u32 accel_oc_cl(Machine& machine, u32 obj, u32 cla, bool newer);

u32 accel_get_prop(Machine& machine, u32 obj, u32 id, bool newer) {
    auto cla = u32{0};
    if ((id & 0xffff0000u) != 0) {
        cla = machine.memory.read32(machine.accel_params[0] +
                                    (id & 0xffffu) * 4);
        if (accel_oc_cl(machine, obj, cla, newer) == 0) {
            return 0;
        }
        id >>= 16;
        obj = cla;
    }

    const auto prop =
        newer ? accel_cp_tab_new(machine, obj, id) : accel_cp_tab(machine, obj, id);
    if (prop == 0) {
        return 0;
    }
    if (accel_obj_in_class(machine, obj) && cla == 0) {
        if (id < machine.accel_params[1] || id >= machine.accel_params[1] + 8) {
            return 0;
        }
    }
    if (machine.memory.read32(machine.accel_params[6]) != obj &&
        (machine.memory.read8(prop + 9) & 1) != 0) {
        return 0;
    }
    return prop;
}

u32 accel_ra_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto prop = accel_get_prop(machine, obj, id, newer);
    return prop == 0 ? 0 : machine.memory.read32(prop + 4);
}

u32 accel_rl_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto prop = accel_get_prop(machine, obj, id, newer);
    return prop == 0 ? 0 : 4 * machine.memory.read16(prop + 2);
}

u32 accel_oc_cl(Machine& machine, u32 obj, u32 cla, bool newer) {
    const auto region = accel_z_region(machine, obj);
    if (region == 3) {
        return cla == machine.accel_params[5] ? 1 : 0;
    }
    if (region == 2) {
        return cla == machine.accel_params[4] ? 1 : 0;
    }
    if (region != 1) {
        return 0;
    }

    if (cla == machine.accel_params[2]) {
        return (accel_obj_in_class(machine, obj) || obj == machine.accel_params[2] ||
                obj == machine.accel_params[5] || obj == machine.accel_params[4] ||
                obj == machine.accel_params[3])
                   ? 1
                   : 0;
    }
    if (cla == machine.accel_params[3]) {
        return (accel_obj_in_class(machine, obj) || obj == machine.accel_params[2] ||
                obj == machine.accel_params[5] || obj == machine.accel_params[4] ||
                obj == machine.accel_params[3])
                   ? 0
                   : 1;
    }
    if (cla == machine.accel_params[5] || cla == machine.accel_params[4]) {
        return 0;
    }
    if (!accel_obj_in_class(machine, cla)) {
        return 0;
    }

    const auto prop = accel_get_prop(machine, obj, 2, newer);
    if (prop == 0) {
        return 0;
    }
    const auto list = machine.memory.read32(prop + 4);
    if (list == 0) {
        return 0;
    }
    const auto len = machine.memory.read16(prop + 2);
    for (u32 index = 0; index < len; ++index) {
        if (machine.memory.read32(list + index * 4) == cla) {
            return 1;
        }
    }
    return 0;
}

u32 accel_rv_pr(Machine& machine, span<const u32> args, bool newer) {
    const auto id = accel_arg(args, 1);
    const auto address = accel_ra_pr(machine, accel_arg(args, 0), id, newer);
    if (address == 0) {
        if (id > 0 && id < machine.accel_params[1]) {
            return machine.memory.read32(machine.accel_params[8] + id * 4);
        }
        return 0;
    }
    return machine.memory.read32(address);
}

u32 accel_op_pr(Machine& machine, u32 obj, u32 id, bool newer) {
    const auto region = accel_z_region(machine, obj);
    if (region == 3) {
        return (id == machine.accel_params[1] + 6 ||
                id == machine.accel_params[1] + 7)
                   ? 1
                   : 0;
    }
    if (region == 2) {
        return id == machine.accel_params[1] + 5 ? 1 : 0;
    }
    if (region != 1) {
        return 0;
    }
    if (id >= machine.accel_params[1] && id < machine.accel_params[1] + 8 &&
        accel_obj_in_class(machine, obj)) {
        return 1;
    }
    return accel_ra_pr(machine, obj, id, newer) != 0 ? 1 : 0;
}

}  // namespace

bool accel_function_supported(u32 function) {
    return function >= 1 && function <= 13;
}

u32 accelerated_function_for(const Machine& machine, u32 address) {
    for (const auto entry : machine.accel_entries) {
        if (entry.address == address) {
            return entry.function;
        }
    }
    return 0;
}

void set_accelerated_function(Machine& machine, u32 function, u32 address) {
    for (auto& entry : machine.accel_entries) {
        if (entry.address == address) {
            entry.function = accel_function_supported(function) ? function : 0;
            return;
        }
    }
    if (!accel_function_supported(function)) {
        return;
    }
    for (auto& entry : machine.accel_entries) {
        if (entry.address == 0) {
            entry = {.address = address, .function = function};
            return;
        }
    }
    throw std::runtime_error("acceleration table is full");
}

void set_acceleration_param(Machine& machine, u32 index, u32 value) {
    if (index < machine.accel_params.size()) {
        machine.accel_params[index] = value;
    }
}

u32 call_accelerated_function(Machine& machine,
                              u32 function,
                              span<const u32> args) {
    switch (function) {
        case 1:
            return accel_z_region(machine, accel_arg(args, 0));
        case 2:
            return accel_cp_tab(machine, accel_arg(args, 0), accel_arg(args, 1));
        case 3:
            return accel_ra_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 4:
            return accel_rl_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 5:
            return accel_oc_cl(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 6:
            return accel_rv_pr(machine, args, false);
        case 7:
            return accel_op_pr(machine, accel_arg(args, 0), accel_arg(args, 1), false);
        case 8:
            return accel_cp_tab_new(machine, accel_arg(args, 0), accel_arg(args, 1));
        case 9:
            return accel_ra_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 10:
            return accel_rl_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 11:
            return accel_oc_cl(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        case 12:
            return accel_rv_pr(machine, args, true);
        case 13:
            return accel_op_pr(machine, accel_arg(args, 0), accel_arg(args, 1), true);
        default:
            return 0;
    }
}

}  // namespace glupsk
