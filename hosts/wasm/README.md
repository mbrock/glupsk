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

The host surface is window-aware but still semantic. C++ owns the Glk registry,
memory streams, event structs, and text decoding; the embedding host owns window
presentation and line input:

```c
void glupsk_host_window_open(
    uint32_t window,
    uint32_t stream,
    uint32_t split,
    uint32_t method,
    uint32_t size,
    uint32_t type,
    uint32_t rock);

uint32_t glupsk_host_window_width(uint32_t window);
uint32_t glupsk_host_window_height(uint32_t window);
void glupsk_host_window_clear(uint32_t window);
void glupsk_host_window_move_cursor(uint32_t window, uint32_t x, uint32_t y);

void glupsk_host_write_latin1(
    uint32_t window,
    uint32_t stream,
    const uint8_t* bytes,
    uint32_t length);

void glupsk_host_write_unicode(
    uint32_t window,
    uint32_t stream,
    const uint32_t* codepoints,
    uint32_t length);

uint32_t glupsk_host_read_line_latin1(
    uint32_t window,
    uint8_t* bytes,
    uint32_t max_length);

uint32_t glupsk_host_read_line_unicode(
    uint32_t window,
    uint32_t* codepoints,
    uint32_t max_length);
```

Line-input imports return the number of code units written, or `0xffffffff`
when input is not ready yet. That lets a browser host return to the event loop
and resume the VM after a form submit.

`tools/glupsk-play-deno.ts` is a terminal simulator for the window ABI. It prints
the root text-buffer window normally and labels split/status-window output.
`make web-assets` copies the built wasm module and AA story into `www/`, plus
`.zst` and `.gz` sidecars for Caddy's precompressed file server. The static
files use DOM elements for Glk windows and run the VM in a Web Worker so Glulx
execution does not block main-thread rendering or input.

The wasm build uses `-fno-exceptions`. Core failure paths that throw in native
builds trap in wasm instead. Tiny internal `__cxa_*` trap shims catch any
remaining libc++ throw path, so exported functions do not require the host to
provide a C++ exception runtime. The module still imports a small set of WASI
libc functions from `wasi_snapshot_preview1`.
