# Glupsk WebAssembly Host ABI

This target is intentionally `wasm32-unknown-unknown`: no WASI, no Emscripten,
and no ambient filesystem. The story is loaded from bytes supplied by the
embedding host.

Build with:

```sh
make wasm
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

The module imports only a narrow Glk-like host surface:

```c
uint32_t glupsk_host_glk_call(
    uint32_t selector,
    uint32_t argc,
    const uint32_t* args,
    uint32_t* value);

uint32_t glupsk_host_glk_put_char(uint32_t value);
```

Both imports return `0` for returned, `1` for blocked, and `2` for fatal. For
`glupsk_host_glk_call`, returned calls write the Glk return value through
`value`.
