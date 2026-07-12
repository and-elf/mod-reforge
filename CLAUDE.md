# CLAUDE.md — mod-reforge

Standalone AzerothCore (WotLK 3.3.5a) module: a generic item **re-itemization / reforging engine**.
Branding-agnostic; all itemization data is injected via `IReforgeConfig`. See `docs/ARCHITECTURE.md`
(the authoritative spec) and `README.md`.

## Workflow

`spec → tests → code` (TDD, red/green/refactor). No implementation lands before its spec section and
a failing test exist. Keep `src/core/` free of AzerothCore headers (the pure-core / adapter split).

## Layout

- `src/core/reforge/` — pure C++20 logic (namespace `Reforge`): `Stats.h`, `ReforgeConfig.h`,
  `Reitemize.{h,cpp}`. No AzerothCore includes here — ever.
- `src/` (root) — thin server adapters (future: enchant-slot stat vehicle, reforge NPC, persistence).
- `tests/standalone/` — self-contained GoogleTest build (`reforge_core_tests`); `tests/fakes/` DI doubles.

## Fast test loop

```bash
cmake -S tests/standalone -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Pre-commit hook (required — gitleaks + linters + guards)

Install once from the repo root:

```bash
ln -sf ../../tools/pre-commit .git/hooks/pre-commit
```

It runs gitleaks (secret scan), the core-purity guard, `codestyle-cpp.py` (when a parent AzerothCore
checkout is present), and the standalone core tests. Bypass only in emergencies: `git commit --no-verify`.

## Code style

Matches AzerothCore: 4-space indent (no tabs), Allman braces, `Type const&` / `Type const*`, `{}`
format specifiers, UTF-8/LF, max 120 cols, trailing newline. No braces around single-line statements.
