# AGENTS.md

## Project

Glupsk is a small, modern C++ Glulx interpreter project. It works but hasn't been really exhaustively tested for correctness and doesn't implement every possible feature yet.

The `aa.glulx` game (by my brother) was the initial milestone, but it would be cool to make sure Glupsk can run a variety of known and unknown Inform 7 games including ones that use features like graphics, etc.

## References

Read `glupsk-spec.txt` first for the project goals, architecture sketch, constraints, and non-goals. `glulx-spec.md` is the local copy of the Glulx reference specification. `Glk-Spec.md` likewise for the GLK API which is kind of like the platform backend abstract layer for the Glulx VM (windows, streams, styling, sound, events, etc).

Reference implementations live under `refimpl/` as git submodules. Reference games, test data, and archived external fixtures live under `refdata/`. Tooling or compiler-packaging helpers live under `tools/`.

The `tools/inform7-nix` flake is a submodule containing a repo that I made some time ago which bootstraps the Inform 7 compiler etc and provides some tools for command line builds of games to VM files etc.

## Build

The Makefile mostly calls Meson but we don't use Meson for the WASM build because it basically can only deal with Emscripten and messes with linking normal standalone/WASI Clang WASM builds.

Use C++23. Use `std::format`, `std::print`, and `std::println` for tracing tools and stuff but don't use `std::format` in the core VM stuff because it bloats the WASM binary significantly.

In VM execution code, avoid heap-allocating temporary containers.

## Commit Style

Commit basically whenever you're done with something, don't leave the repo uncommitted, there is no harm in committing.

A commit does not need to imply an exhaustive validation pass. For small bookkeeping steps, it is fine to commit after basic sanity checks such as inspecting `git status` or the touched files.
