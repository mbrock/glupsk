#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

struct RunResult {
    u64 steps = 0;
    bool halted = false;
};

void start(Machine& machine);
void step(Machine& machine);
RunResult run_until_halted(Machine& machine, u64 max_steps);

}  // namespace glupsk
