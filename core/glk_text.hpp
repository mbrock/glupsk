#pragma once

#include "core/glk.hpp"
#include "core/machine.hpp"

namespace glupsk {

u32 glk_char_to_lower(u32 ch);
u32 glk_char_to_upper(u32 ch);

u32 glk_text_length(const GlkTextData& text);
bool glk_text_is_latin1(const GlkTextData& text);
GlkTextData glk_own_text(const GlkTextData& text);
GlkTextData glk_materialize_text(Machine& machine, const GlkText& text);
GlkTextData glk_materialize_text(Machine& machine, const GlkTextChar& text);
GlkTextData glk_materialize_text(Machine& machine, const GlkTextBuffer& text);
GlkTextData glk_materialize_text(Machine& machine, const GlkTextString& text);

u32 glk_transform_unicode_buffer(Machine& machine,
                                 u32 address,
                                 u32 len,
                                 u32 numchars,
                                 u32 (*transform)(u32));
u32 glk_title_case_unicode_buffer(Machine& machine,
                                  u32 address,
                                  u32 len,
                                  u32 numchars,
                                  bool lower_rest);

}  // namespace glupsk
