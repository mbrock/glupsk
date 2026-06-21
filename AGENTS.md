# AGENTS.md

## Project

Glupsk is a small, modern C++ Glulx interpreter project. Its current design target is a minimal explicit-state runtime that can eventually run a basic compiled Inform 7 Glulx game, with clarity, correctness, serialization, and WebAssembly portability taking priority over optimization.

Read `glupsk-spec.txt` first for the project goals, architecture sketch, constraints, and non-goals. `glulx-spec.md` is the local copy of the Glulx reference specification.

Reference implementations live under `refimpl/` as git submodules. Reference games, test data, and archived external fixtures live under `refdata/`. Tooling or compiler-packaging helpers live under `tools/`.

## Build

Meson is the authoritative C++ build. Use the root GNU `Makefile` for the simple local loop, but keep it as a wrapper around Meson rather than a separate compiler invocation path. `make` runs Meson setup and compile, `make test` runs Meson tests, `make trace` builds the trace tool, `make info` inspects the committed AA Glulx story with the Meson-built info tool, and `make aa` rebuilds `refdata/aa.ulx` through the Inform 7 Nix tooling.

`make`, `make meson`, and `make compile-commands` ensure `build/meson/compile_commands.json` exists and link it at the repo root for clangd.

Use C++23. Prefer `std::format`, `std::print`, and `std::println` over iostream formatting for new code.

In VM execution code, avoid heap-allocating temporary containers. Prefer fixed-size local arrays, spans over existing storage, or in-place algorithms when an opcode can be implemented without dynamic allocation.

Do not stub missing VM or Glk functionality by returning a plausible zero-ish value unless the spec makes that result unambiguous, such as an unsupported `gestalt` feature or the end of an iterator. If story code attempts an unsupported opcode, selector, or invalid nonzero Glk handle, fail loudly with an exception/error so the missing behavior is visible in tests and traces.

If VM behavior becomes significantly confusing, consider adding an oracle trace comparison against a patched reference interpreter such as Glulxe. A compact per-instruction state fingerprint stream, with `pc`, opcode, stack pointer, and stack hash, would usually be better than manually reading giant traces; dump full stack/memory only at the first divergence.

Tests use the lightweight harness in `tests/test.hpp`: register static `suite`s and write cases with `"name"_test = [] { ... };` plus `expect(...)`.

## Commit Style

Commit basically whenever you're done with something, don't leave the repo uncommitted, there is no harm in committing.

A commit does not need to imply an exhaustive validation pass. For small bookkeeping steps, it is fine to commit after basic sanity checks such as inspecting `git status` or the touched files.
