#include "core/execute.hpp"
#include "core/error.hpp"
#include "core/glk.hpp"
#include "core/machine.hpp"
#include "core/story.hpp"
#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifndef GLUPSK_NO_EXCEPTIONS
#include <exception>
#endif
#include <limits>
#include <memory>
#include <string>
#include <utility>

extern "C" std::uint32_t glupsk_host_glk_call(std::uint32_t selector,
                                               std::uint32_t argc,
                                               const std::uint32_t* args,
                                               std::uint32_t* value);
extern "C" std::uint32_t glupsk_host_glk_put_char(std::uint32_t value);

namespace {

enum WasmStatus : std::uint32_t {
    VM_OK = 0,
    VM_BLOCKED = 1,
    VM_HALTED = 2,
    VM_ERROR = 3,
    VM_BAD_HANDLE = 4,
    VM_UNSUPPORTED = 5,
};

enum HostResult : std::uint32_t {
    HOST_RETURNED = 0,
    HOST_BLOCKED = 1,
    HOST_FATAL = 2,
};

class WasmGlkRuntime final : public glupsk::GlkRuntime {
  public:
    glupsk::GlkCallResult call(glupsk::Machine&,
                               glupsk::u32 selector,
                               glupsk::span<const glupsk::u32> args) override {
        auto value = glupsk::u32{0};
        const auto kind = glupsk_host_glk_call(selector,
                                              static_cast<glupsk::u32>(args.size()),
                                              args.data(),
                                              &value);
        switch (kind) {
            case HOST_RETURNED:
                return glupsk::glk_returned(value);
            case HOST_BLOCKED:
                return glupsk::glk_blocked();
            case HOST_FATAL:
                return glupsk::glk_fatal("Glk host reported a fatal call error");
            default:
                return glupsk::glk_fatal("Glk host returned an invalid call status");
        }
    }

    void put_char(glupsk::Machine&, glupsk::u32 ch) override {
        const auto kind = glupsk_host_glk_put_char(ch);
        if (kind == HOST_RETURNED) {
            return;
        }
        if (kind == HOST_BLOCKED) {
            glupsk::fail("Glk host blocked while writing a character");
        }
        glupsk::fail("Glk host reported a fatal character output error");
    }
};

struct WasmVm {
    WasmGlkRuntime glk;
    std::unique_ptr<glupsk::Machine> machine;
    std::string last_error;

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
