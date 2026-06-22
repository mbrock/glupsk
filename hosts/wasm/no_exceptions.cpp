#include <cstddef>
#include <cstdlib>

extern "C" void* __cxa_allocate_exception(std::size_t size) {
    auto* ptr = std::malloc(size == 0 ? 1 : size);
    if (!ptr) {
        __builtin_trap();
    }
    return ptr;
}

extern "C" void __cxa_free_exception(void* ptr) {
    std::free(ptr);
}

extern "C" [[noreturn]] void __cxa_throw(void*, void*, void (*)(void*)) {
    __builtin_trap();
    __builtin_unreachable();
}
