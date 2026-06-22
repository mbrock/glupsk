#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

bool accel_function_supported(u32 function);
u32 accelerated_function_for(const Machine& machine, u32 address);

void set_accelerated_function(Machine& machine, u32 function, u32 address);
void set_acceleration_param(Machine& machine, u32 index, u32 value);

// Inform-generated Glulx games can register small semantic shortcuts for
// property lookup and class checks. Unsupported function ids simply behave as
// absent acceleration entries.
u32 call_accelerated_function(Machine& machine,
                              u32 function,
                              span<const u32> args);

}  // namespace glupsk
