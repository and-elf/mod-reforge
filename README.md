# mod-reforge

A standalone AzerothCore (WotLK 3.3.5a) module: a **generic item re-itemization / reforging
engine**. It rebuilds an item's stat *composition* onto a modern stat template at a given stat
*budget*, and lets players reforge — move a bounded fraction of one stat into another — all as
dependency-free pure C++20 logic plus a thin server adapter.

It knows nothing about any progression system. Stat templates, legality, reforge limits and cost
are **injected** (`IReforgeConfig`), so the engine runs on a bare server or under a host module
(e.g. mod-branding drives it to make heroic-scaled old-content loot viable at max level — see
issue and-elf/azerothcore-wotlk#76).

## Design

Pure-core / adapter split (see `docs/ARCHITECTURE.md`): all decision logic lives in
`src/core/reforge/` over plain structs and is unit-tested with GoogleTest in seconds via the
standalone build; `src/` adapters wire it to live items.

## Fast test loop

```bash
cmake -S tests/standalone -B build && cmake --build build
ctest --test-dir build --output-on-failure
```
