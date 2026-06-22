# Glupsk WebAssembly Host ABI

This target is a small direct clang build, not an Emscripten build. It uses the
Ubuntu/LLVM WASI libc++ packages for C and C++ runtime support, but it does not
expect an Emscripten-generated JavaScript runtime or filesystem packaging layer.
The story is loaded from bytes supplied by the embedding host.

Build with:

```sh
make wasm
```

On Ubuntu, the expected packages are:

```sh
sudo apt-get install wasi-libc libc++-22-dev-wasm32 libc++abi-22-dev-wasm32 libclang-rt-22-dev-wasm32 lld-22
```

The resulting module exports:

- `vm_alloc(size)` / `vm_free(ptr, size)` for moving story bytes into module
  memory.
- `vm_create()` / `vm_destroy(vm)` for VM lifetime.
- `vm_load_story(vm, ptr, size)` to initialize a VM from Glulx bytes.
- `vm_step(vm)`, `vm_run_until_blocked(vm, max_steps)`, and `vm_resume(vm)`.
- `vm_status(vm)`, `vm_pc(vm)`, and `vm_sp(vm)` for simple inspection.
- `vm_last_error(vm)` for a nul-terminated diagnostic owned by the VM.
- `vm_snapshot_size(vm, size_ptr)`, `vm_snapshot_write(vm, ptr, size)`, and
  `vm_snapshot_read(vm, ptr, size)`, which currently return `VM_UNSUPPORTED`
  until the save layer exists.

The terminal host surface is intentionally small and semantic:

```c
void glupsk_host_write_latin1(const uint8_t* bytes, uint32_t length);
void glupsk_host_write_unicode(const uint32_t* codepoints, uint32_t length);

uint32_t glupsk_host_read_line_latin1(
    uint32_t window,
    uint8_t* bytes,
    uint32_t max_length);

uint32_t glupsk_host_read_line_unicode(
    uint32_t window,
    uint32_t* codepoints,
    uint32_t max_length);
```

The C++ wasm wrapper owns the lightweight Glk session, window/stream registry,
memory streams, event structs, and text decoding. The embedding host only writes
terminal output and fills line-input buffers. See `tools/glupsk-play-deno.ts`
for a minimal Deno runner.

The wasm build uses `-fno-exceptions`. Core failure paths that throw in native
builds trap in wasm instead. Tiny internal `__cxa_*` trap shims catch any
remaining libc++ throw path, so exported functions do not require the host to
provide a C++ exception runtime. The module still imports a small set of WASI
libc functions from `wasi_snapshot_preview1`.
