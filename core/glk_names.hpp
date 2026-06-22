#pragma once

#include "core/types.hpp"

#include <string_view>

namespace glupsk {

std::string_view glulx_gestalt_name(u32 selector);
std::string_view glk_selector_name(u32 selector);
std::string_view glk_gestalt_name(u32 selector);

}  // namespace glupsk
