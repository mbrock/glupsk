#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

void output_char(Machine& machine, u32 ch);
void output_string_object(Machine& machine, u32 address);

}  // namespace glupsk
