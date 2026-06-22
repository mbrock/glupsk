#pragma once

#include <string>
#include <utility>

#ifndef GLUPSK_NO_EXCEPTIONS
#include <stdexcept>
#endif

namespace glupsk {

[[noreturn]] inline void fail(std::string message) {
#ifdef GLUPSK_NO_EXCEPTIONS
    (void) message;
    __builtin_trap();
    __builtin_unreachable();
#else
    throw std::runtime_error(std::move(message));
#endif
}

}  // namespace glupsk
