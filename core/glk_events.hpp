#pragma once

#include "core/glk.hpp"
#include "core/machine.hpp"

namespace glupsk {

struct GlkPendingLine {
    GlkWindowHandle window = {};
    u32 buffer_address = 0;
    u32 max_length = 0;
    GlkTextEncoding encoding = GlkTextEncoding::latin1;
};

void glk_write_ref(Machine& machine, u32 address, u32 value);
void glk_write_event_field(Machine& machine, u32 address, u32 field, u32 value);
u32 glk_write_input_text(Machine& machine,
                         const GlkPendingLine& pending,
                         const GlkInputText& text);

}  // namespace glupsk
