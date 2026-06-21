# AGENTS.md

## Project

Glupsk is a small, modern C++ Glulx interpreter project. Its current design target is a minimal explicit-state runtime that can eventually run a basic compiled Inform 7 Glulx game, with clarity, correctness, serialization, and WebAssembly portability taking priority over optimization.

Read `glupsk-spec.txt` first for the project goals, architecture sketch, constraints, and non-goals. `glulx-spec.md` is the local copy of the Glulx reference specification.

Reference implementations live under `refimpl/` as git submodules. Reference games, test data, and archived external fixtures live under `refdata/`. Tooling or compiler-packaging helpers live under `tools/`.

## Build

Use the root GNU `Makefile` for the simple local loop. `make` builds the default C++ tool, `make test` runs the lightweight test binary, `make info` inspects the committed AA Glulx story, and `make aa` rebuilds `refdata/aa.ulx` through the Inform 7 Nix tooling.

Meson is also available for editor tooling and a conventional compile database. `make compile-commands` creates `build/meson/compile_commands.json` and links it at the repo root for clangd. `make meson-test` verifies the Meson build.

Use C++23. Prefer `std::format`, `std::print`, and `std::println` over iostream formatting for new code.

In VM execution code, avoid heap-allocating temporary containers. Prefer fixed-size local arrays, spans over existing storage, or in-place algorithms when an opcode can be implemented without dynamic allocation.

Do not stub missing VM or Glk functionality by returning a plausible zero-ish value unless the spec makes that result unambiguous, such as an unsupported `gestalt` feature or the end of an iterator. If story code attempts an unsupported opcode, selector, or invalid nonzero Glk handle, fail loudly with an exception/error so the missing behavior is visible in tests and traces.

Tests use the lightweight harness in `tests/test.hpp`: register static `suite`s and write cases with `"name"_test = [] { ... };` plus `expect(...)`.

## Commit Style

Keep the repository in clean checkpoints. Commits are cheap and should usually be made after reasonable self-contained steps, especially after adding dependencies, reference material, scaffolding, or other coherent chunks of work.

A commit does not need to imply an exhaustive validation pass. For small bookkeeping steps, it is fine to commit after basic sanity checks such as inspecting `git status` or the touched files.
