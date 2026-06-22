#pragma once

#include "core/machine.hpp"
#include "core/types.hpp"

namespace glupsk {

u32 linear_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options);

u32 binary_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 struct_size,
                  u32 num_structs,
                  u32 key_offset,
                  u32 options);

u32 linked_search(Machine& machine,
                  u32 key,
                  u32 key_size,
                  u32 start,
                  u32 key_offset,
                  u32 next_offset,
                  u32 options);

}  // namespace glupsk
