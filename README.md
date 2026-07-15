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

## Server adapter

The `src/` adapters wire the engine to a live server (see `docs/ARCHITECTURE.md` §8–§11):

- **Reforge NPC** (`Reforger`, entry `900100`) — a gossip wizard: pick an equipped item → reduce a
  stat → into another → amount → currency. Needs no addon.
- **`.reforge` commands** — `do <slot> <from> <to> <amount> [currencyEntry]`, `clear <slot>`, `list`,
  `sync`. Player-usable (RBAC `100000`); also the client addon's inbound API.
- **Enchant-slot stat vehicle** — reforged stats apply server-side (source reduced via item-mod hook,
  destination added via a prismatic-slot enchant whose amount is injected per item). No client patch.
- **Any-currency payment** — cost is `<item-entry>×<count>` (entry `0` = gold); the realm accepts a
  list and the player chooses. Configure with `Reforge.Cost.Currencies`.

Reforging is gated to the Reforge NPC by default (`Reforge.RequireNpc`). Apply the module SQL under
`data/sql/` (world: enchant pool + NPC; characters: `character_item_reforge`; auth: the RBAC perm).

## Client addon

`client-addon/Reforge/` — a `/reforge` panel (item picker, from/to stat dropdowns, amount, currency
dropdown). Pure `LANG_ADDON` transport, no client patch: state is pushed as `RFRG` frames, requests go
out over AzerothCore's built-in addon command channel as the same `.reforge` command. See
`client-addon/README.md`.
