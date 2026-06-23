#include "core/execute.hpp"
#include "core/glk.hpp"
#include "core/glk_runtime.hpp"
#include "core/machine.hpp"
#include "core/story.hpp"
#include "core/types.hpp"

#include <cstdint>
#include <cstdlib>
#ifndef GLUPSK_NO_EXCEPTIONS
#include <exception>
#endif
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

extern "C" void glupsk_host_window_open(std::uint32_t window,
                                         std::uint32_t stream,
                                         std::uint32_t split,
                                         std::uint32_t method,
                                         std::uint32_t size,
                                         std::uint32_t type,
                                         std::uint32_t rock);
extern "C" std::uint32_t glupsk_host_window_width(std::uint32_t window);
extern "C" std::uint32_t glupsk_host_window_height(std::uint32_t window);
extern "C" void glupsk_host_window_clear(std::uint32_t window);
extern "C" void glupsk_host_window_move_cursor(std::uint32_t window,
                                                std::uint32_t x,
                                                std::uint32_t y);
extern "C" void glupsk_host_stylehint_set(std::uint32_t window_type,
                                           std::uint32_t style,
                                           std::uint32_t hint,
                                           std::uint32_t value);
extern "C" std::uint32_t glupsk_host_poll_arrange();
extern "C" void glupsk_host_write_latin1(std::uint32_t window,
                                          std::uint32_t stream,
                                          std::uint32_t style,
                                          const std::uint8_t* bytes,
                                          std::uint32_t length);
extern "C" void glupsk_host_write_unicode(std::uint32_t window,
                                           std::uint32_t stream,
                                           std::uint32_t style,
                                           const std::uint32_t* codepoints,
                                           std::uint32_t length);
extern "C" std::uint32_t glupsk_host_read_line_latin1(std::uint32_t window,
                                                       std::uint8_t* bytes,
                                                       std::uint32_t max_length);
extern "C" std::uint32_t glupsk_host_read_line_unicode(
    std::uint32_t window,
    std::uint32_t* codepoints,
    std::uint32_t max_length);

namespace {

enum WasmStatus : std::uint32_t {
    VM_OK = 0,
    VM_BLOCKED = 1,
    VM_HALTED = 2,
    VM_ERROR = 3,
    VM_BAD_HANDLE = 4,
    VM_UNSUPPORTED = 5,
};

constexpr auto HOST_INPUT_BLOCKED = std::numeric_limits<std::uint32_t>::max();

struct DenoTerminalHost {
    struct Window {
        glupsk::u32 id = 0;
        glupsk::u32 type = 0;
    };
    struct Stream {
        glupsk::u32 id = 0;
        glupsk::u32 window = 0;
    };
    struct FileRef {};

    glupsk::u32 next_window = 1;
    glupsk::u32 next_stream = 1;

    glupsk::u32 gestalt(glupsk::GlkGestaltQuery query) {
        return glupsk::glk_default_gestalt(query);
    }

    glupsk::GlkOpenedWindow<DenoTerminalHost> open_window(
        glupsk::GlkWindowSpec spec) {
        const auto window = next_window++;
        const auto stream = next_stream++;
        glupsk_host_window_open(window, stream, spec.split.id, spec.method,
                                spec.size, spec.type, spec.rock);
        return {
            .window = Window{.id = window, .type = spec.type},
            .stream = Stream{
                .id = stream,
                .window = window,
            },
        };
    }

    glupsk::GlkWindowSize window_size(Window& window) {
        return {
            .width = glupsk_host_window_width(window.id),
            .height = glupsk_host_window_height(window.id),
        };
    }

    void window_clear(Window& window) {
        glupsk_host_window_clear(window.id);
    }

    void window_move_cursor(Window& window, glupsk::u32 x, glupsk::u32 y) {
        glupsk_host_window_move_cursor(window.id, x, y);
    }

    void stylehint_set(glupsk::u32 window_type,
                       glupsk::GlkStyle style,
                       glupsk::u32 hint,
                       glupsk::u32 value) {
        glupsk_host_stylehint_set(window_type,
                                  static_cast<std::uint32_t>(style), hint,
                                  value);
    }

    glupsk::GlkCallResult write(Stream& stream,
                                const glupsk::GlkTextData& text,
                                glupsk::GlkStyle style) {
        const auto style_id = static_cast<std::uint32_t>(style);
        if (const auto* bytes = std::get_if<std::string>(&text)) {
            glupsk_host_write_latin1(
                stream.window,
                stream.id,
                style_id,
                reinterpret_cast<const std::uint8_t*>(bytes->data()),
                static_cast<std::uint32_t>(bytes->size()));
            return glupsk::glk_returned();
        }

        const auto write_codepoints = [&](std::span<const glupsk::u32> codepoints) {
            glupsk_host_write_unicode(
                stream.window,
                stream.id,
                style_id,
                codepoints.data(),
                static_cast<std::uint32_t>(codepoints.size()));
        };

        if (const auto* codepoints = std::get_if<std::vector<glupsk::u32>>(&text)) {
            write_codepoints(*codepoints);
            return glupsk::glk_returned();
        }
        if (const auto* codepoints = std::get_if<std::span<const glupsk::u32>>(&text)) {
            write_codepoints(*codepoints);
            return glupsk::glk_returned();
        }
        for (const auto chunk : std::get<glupsk::GlkCodepointChunks>(text).chunks) {
            write_codepoints(chunk);
        }
        return glupsk::glk_returned();
    }

    glupsk::GlkEventResult select(glupsk::GlkEventRequest request) {
        if (glupsk_host_poll_arrange() != 0) {
            return glupsk::GlkArrangeEvent{};
        }
        for (const auto& interest : request.interests) {
            if (const auto* line =
                    std::get_if<glupsk::GlkLineInputRequest>(&interest)) {
                if (line->encoding == glupsk::GlkTextEncoding::unicode) {
                    auto codepoints = std::vector<glupsk::u32>(line->max_length);
                    const auto count = glupsk_host_read_line_unicode(
                        line->window.id, codepoints.data(), line->max_length);
                    if (count == HOST_INPUT_BLOCKED) {
                        return glupsk::GlkBlocked{};
                    }
                    codepoints.resize(count);
                    return glupsk::GlkLineInputEvent{
                        .window = line->window,
                        .text = std::move(codepoints),
                    };
                }

                auto bytes = std::string(line->max_length, '\0');
                const auto count = glupsk_host_read_line_latin1(
                    line->window.id,
                    reinterpret_cast<std::uint8_t*>(bytes.data()),
                    line->max_length);
                if (count == HOST_INPUT_BLOCKED) {
                    return glupsk::GlkBlocked{};
                }
                bytes.resize(count);
                return glupsk::GlkLineInputEvent{
                    .window = line->window,
                    .text = std::move(bytes),
                };
            }
        }
        return glupsk::GlkTimerEvent{};
    }

    bool echo_line_input() {
        return false;
    }
};

struct WasmVm {
    glupsk::GlkSession<DenoTerminalHost> glk;
    std::unique_ptr<glupsk::Machine> machine;
    std::string last_error;

    WasmVm() : glk(DenoTerminalHost{}) {}

    WasmStatus status() const {
        if (!machine) {
            return VM_ERROR;
        }
        if (machine->blocked) {
            return VM_BLOCKED;
        }
        if (machine->halted) {
            return VM_HALTED;
        }
        return VM_OK;
    }
};

std::string global_last_error;

WasmVm* as_vm(std::uint32_t handle) {
    return reinterpret_cast<WasmVm*>(static_cast<std::uintptr_t>(handle));
}

std::uint32_t as_handle(WasmVm* vm) {
    const auto raw = reinterpret_cast<std::uintptr_t>(vm);
    if (raw > std::numeric_limits<std::uint32_t>::max()) {
        delete vm;
        global_last_error = "wasm pointer does not fit in the exported 32-bit handle";
        return 0;
    }
    return static_cast<std::uint32_t>(raw);
}

WasmStatus fail(WasmVm* vm, std::string message) {
    if (vm) {
        vm->last_error = std::move(message);
    } else {
        global_last_error = std::move(message);
    }
    return VM_ERROR;
}

template <typename Fn>
WasmStatus with_vm(std::uint32_t handle, Fn fn) {
    auto* vm = as_vm(handle);
    if (!vm || !vm->machine) {
        return VM_BAD_HANDLE;
    }
#ifdef GLUPSK_NO_EXCEPTIONS
    return fn(*vm);
#else
    try {
        return fn(*vm);
    } catch (const std::exception& ex) {
        return fail(vm, ex.what());
    } catch (...) {
        return fail(vm, "unknown VM error");
    }
#endif
}

}  // namespace

extern "C" void* vm_alloc(std::uint32_t size) {
    auto* ptr = std::malloc(size == 0 ? 1 : size);
    if (!ptr) {
        global_last_error = "allocation failed";
    }
    return ptr;
}

extern "C" void vm_free(void* ptr, std::uint32_t) {
    std::free(ptr);
}

extern "C" std::uint32_t vm_create() {
#ifdef GLUPSK_NO_EXCEPTIONS
    return as_handle(new WasmVm{});
#else
    try {
        return as_handle(new WasmVm{});
    } catch (const std::exception& ex) {
        global_last_error = ex.what();
        return 0;
    } catch (...) {
        global_last_error = "unknown VM creation error";
        return 0;
    }
#endif
}

extern "C" void vm_destroy(std::uint32_t handle) {
    delete as_vm(handle);
}

extern "C" std::uint32_t vm_load_story(std::uint32_t handle,
                                        const std::uint8_t* bytes,
                                        std::uint32_t size) {
    auto* vm = as_vm(handle);
    if (!vm || !bytes) {
        return VM_BAD_HANDLE;
    }
#ifdef GLUPSK_NO_EXCEPTIONS
    auto owned = glupsk::Bytes(bytes, bytes + size);
    auto story = glupsk::Story::from_bytes(std::move(owned));
    vm->machine = std::make_unique<glupsk::Machine>(
        glupsk::Machine::from_story(story));
    vm->machine->glk = &vm->glk;
    vm->last_error.clear();
    return VM_OK;
#else
    try {
        auto owned = glupsk::Bytes(bytes, bytes + size);
        auto story = glupsk::Story::from_bytes(std::move(owned));
        vm->machine = std::make_unique<glupsk::Machine>(
            glupsk::Machine::from_story(story));
        vm->machine->glk = &vm->glk;
        vm->last_error.clear();
        return VM_OK;
    } catch (const std::exception& ex) {
        return fail(vm, ex.what());
    } catch (...) {
        return fail(vm, "unknown story load error");
    }
#endif
}

extern "C" std::uint32_t vm_step(std::uint32_t handle) {
    return with_vm(handle, [](WasmVm& vm) {
        glupsk::step(*vm.machine);
        return vm.status();
    });
}

extern "C" std::uint32_t vm_run_until_blocked(std::uint32_t handle,
                                               std::uint32_t max_steps) {
    return with_vm(handle, [max_steps](WasmVm& vm) {
        (void) glupsk::run_until_blocked(*vm.machine, max_steps);
        return vm.status();
    });
}

extern "C" std::uint32_t vm_resume(std::uint32_t handle) {
    return with_vm(handle, [](WasmVm& vm) {
        glupsk::resume(*vm.machine);
        return vm.status();
    });
}

extern "C" std::uint32_t vm_status(std::uint32_t handle) {
    auto* vm = as_vm(handle);
    if (!vm || !vm->machine) {
        return VM_BAD_HANDLE;
    }
    return vm->status();
}

extern "C" std::uint32_t vm_pc(std::uint32_t handle) {
    auto* vm = as_vm(handle);
    return vm && vm->machine ? vm->machine->regs.pc : 0;
}

extern "C" std::uint32_t vm_sp(std::uint32_t handle) {
    auto* vm = as_vm(handle);
    return vm && vm->machine ? vm->machine->stack.sp : 0;
}

extern "C" const char* vm_last_error(std::uint32_t handle) {
    auto* vm = as_vm(handle);
    if (!vm) {
        return global_last_error.c_str();
    }
    return vm->last_error.c_str();
}

extern "C" std::uint32_t vm_snapshot_size(std::uint32_t handle,
                                           std::uint32_t* size) {
    auto* vm = as_vm(handle);
    if (!vm || !vm->machine || !size) {
        return VM_BAD_HANDLE;
    }
    *size = 0;
    vm->last_error = "snapshots are not implemented yet";
    return VM_UNSUPPORTED;
}

extern "C" std::uint32_t vm_snapshot_write(std::uint32_t handle,
                                            std::uint8_t*,
                                            std::uint32_t) {
    auto* vm = as_vm(handle);
    if (!vm || !vm->machine) {
        return VM_BAD_HANDLE;
    }
    vm->last_error = "snapshots are not implemented yet";
    return VM_UNSUPPORTED;
}

extern "C" std::uint32_t vm_snapshot_read(std::uint32_t handle,
                                           const std::uint8_t*,
                                           std::uint32_t) {
    auto* vm = as_vm(handle);
    if (!vm || !vm->machine) {
        return VM_BAD_HANDLE;
    }
    vm->last_error = "snapshots are not implemented yet";
    return VM_UNSUPPORTED;
}
