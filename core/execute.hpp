#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

struct RunResult {
    u64 steps = 0;
    bool blocked = false;
    bool halted = false;
};

void start(Machine& machine);
void resume(Machine& machine);
void step(Machine& machine);
RunResult run_until_halted(Machine& machine, u64 max_steps);
RunResult run_until_blocked(Machine& machine, u64 max_steps);

}  // namespace glupsk
