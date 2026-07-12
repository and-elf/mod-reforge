# mod-reforge — Architecture & Specification

Status: **DRAFT v0.1** · Target: AzerothCore (WotLK 3.3.5a) · Build: C++20, CMake, GoogleTest

Authoritative spec. Workflow: **spec → tests → code**. No implementation lands before its spec
section and a failing test exist.

---

## 1. Purpose & Scope

`mod-reforge` is a **generic item re-itemization / reforging engine**. Given an item reduced to a
*chassis* — a stat **budget** plus an **archetype** (and armour/slot) — it:

1. **rebuilds the stat composition** onto a modern stat template at that budget (`ArchetypeTemplate`),
2. grants **automatic gem sockets** per a config policy (`ResolveSockets`),
3. lets a player **reforge** — move a *bounded* fraction of one stat into another (`ApplyReforge`).

It knows **nothing** about any progression system. All itemisation data — stat weights, destination
legality, the reforge limit, socket policy — is **injected** via `IReforgeConfig`. So it runs on a
bare server, or under a host module that supplies policy.

### Motivating host

`mod-branding` drives it to solve issue and-elf/azerothcore-wotlk#76: heroic-scaled vanilla/TBC
loot is level-80 in *budget* but not *composition* (vanilla itemisation predates hit/haste/expertise
and unified spellpower). mod-branding feeds each old-content drop in as a chassis (budget from the
heroic tier), charges branding essence per reforge, and preserves signature effects — see that
module's ARCHITECTURE.md §2.6. None of that policy lives here.

---

## 2. Pure-core / adapter split

All decision logic is dependency-free C++20 under `src/core/reforge/`, operating on plain structs and
unit-tested with GoogleTest in sub-second runs (no worldserver). `src/` adapters (future) are thin:
read a live item → build an `ItemChassis` → call the core → apply the result (stat enchant-slot
vehicle, socket grant, persistence). Every external capability the core needs is an injected
interface — never a global. **Rule:** anything under `src/core/` that includes an AzerothCore header
is a spec violation.

```
        item drop / reforge UI  (adapter, integration-tested)
                 │  ItemChassis / Reforge          ▲  StatBlock / SocketLayout
                 ▼                                  │
        core/reforge  —  PURE LOGIC (POD I/O)  ——————  unit-tested (all branches)
```

---

## 3. Core API (`src/core/reforge/`)

```cpp
enum class ItemStat      : uint8_t { Stamina, Strength, Agility, Intellect, Spirit,
                                     AttackPower, SpellPower, HitRating, CritRating, HasteRating,
                                     ExpertiseRating, ArmorPenRating, Mp5,
                                     DefenseRating, DodgeRating, ParryRating, BlockRating, Resilience, COUNT };
enum class ItemArchetype : uint8_t { PlateTank, StrDps, AgiMeleeDps, AgiRangedDps, CasterDps, CasterHealer, COUNT };
enum class ArmorClass    : uint8_t { None, Cloth, Leather, Mail, Plate, COUNT };
enum class SocketColor   : uint8_t { Prismatic, Red, Yellow, Blue, Meta, COUNT };

struct StatBlock    { /* points[ItemStat::COUNT]; Get/Set/Add; uint32 Total() */ };
struct SocketLayout { /* counts[SocketColor::COUNT]; Get/Set; uint8 Total() */ };
struct ItemChassis  { uint32 budget; ItemArchetype archetype; ArmorClass armor; EquipSlot slot; };

// Rebuild composition: spend the budget EXACTLY over the archetype template (largest-remainder,
// deterministic); points land only on weight>0 stats; empty template / zero budget => zeroed.
StatBlock      ArchetypeTemplate(ItemChassis const&, IReforgeConfig const&);

// Automatic sockets: AutoSocketCount(slot,archetype) of AutoSocketColor(), clamped to MaxSockets().
SocketLayout   ResolveSockets(ItemChassis const&, IReforgeConfig const&);

// Convenience bundle for the adapter.
struct ReitemizedItem { StatBlock stats; SocketLayout sockets; };
ReitemizedItem Reitemize(ItemChassis const&, IReforgeConfig const&);

// Player-directed reforge: move `amount` from `from` to `to`. nullopt if illegal (see §5).
struct Reforge { ItemStat from; ItemStat to; uint32 amount; };
std::optional<StatBlock> ApplyReforge(StatBlock const& base, Reforge const&, ItemChassis const&, IReforgeConfig const&);
```

`IReforgeConfig` (injected): `ReforgeMaxFraction()`, `StatWeight(archetype,stat)`,
`StatLegal(archetype,stat)`, `AutoSocketCount(slot,archetype)`, `AutoSocketColor()`, `MaxSockets()`.

---

## 4. Gem sockets (automatic)

Re-itemisation grants sockets automatically, config-driven so "automatic" is a **tunable**, not a
mandate: `AutoSocketCount == 0` means none. The default color is **Prismatic** (accepts any gem), so a
re-itemised drop is never awkward to gem. Total is clamped to `MaxSockets()` (WoW convention ≈3) so a
misconfigured template can never over-socket. Gem power sits **on top** of the stat budget and is
bounded only by the socket cap — a deliberate, host-tunable power lever (mod-branding gates it behind
essence / heroic tier). *Future:* colored sockets + socket-set bonuses (v1 grants prismatic only).

---

## 5. Invariants (all tested — `tests/reforge/`, 20 cases)

**ArchetypeTemplate** — `Total() == budget` exactly (across many budgets, both defined archetypes);
points only on weight>0 stats; more weight ⇒ ≥ points; deterministic; zero budget / empty template ⇒
zeroed.

**ResolveSockets** — grants the configured count & color; clamped to `MaxSockets()` (incl. 0 ⇒ none);
per-slot policy honored; deterministic. **Reitemize** bundles the two primitives verbatim.

**ApplyReforge** — the reforge is **budget-conserving** (`Total` invariant; points moved, never
created) and **bounded**; returns `nullopt` when: `to == from`; `to` not `StatLegal`; `amount == 0` or
`> ` source points; or `amount > floor(source * ReforgeMaxFraction())`. `base` is never mutated. The
fraction is genuinely config-driven (raising it lets more move); reforge composes with a freshly
templated item and the budget still holds.

---

## 6. Module layout

```
mod-reforge/
├── docs/ARCHITECTURE.md              # this file
├── src/
│   ├── core/reforge/                 # PURE C++20 — no AzerothCore includes
│   │   ├── Stats.h                   # ItemStat/Archetype/ArmorClass/EquipSlot/SocketColor; StatBlock, SocketLayout
│   │   ├── ReforgeConfig.h           # IReforgeConfig (injected itemisation data + tunables)
│   │   ├── Reitemize.h / .cpp        # ArchetypeTemplate, ResolveSockets, Reitemize, ApplyReforge
│   │   └── (adapter .cpp/.h at src/ root — future: item vehicle, reforge UI, persistence)
└── tests/
    ├── standalone/CMakeLists.txt     # FetchContent gtest 1.12.1; globs src/core + tests; builds reforge_core_tests
    ├── fakes/FakeReforgeConfig.h      # deterministic DI test double
    └── reforge/ReitemizeTest.cpp      # the 20-case matrix
```

Fast loop: `cmake -S tests/standalone -B build && cmake --build build && ctest --test-dir build`.

---

## 7. Build order (sequencing)

1. **Slice 1 — Re-itemisation engine** ✓ (this doc): `ArchetypeTemplate`, `ResolveSockets`,
   `Reitemize`, `ApplyReforge` + 20 tests. Pure, host-agnostic.
2. **Slice 2 — Server adapter:** the stat-carrying **enchant-slot vehicle** (per-item stats without a
   client patch), the reforge NPC/command surface, socket grant, per-item persistence, `IReforgeConfig`
   over `sConfigMgr` + itemisation tables.
3. **Slice 3 — Host integration seam:** a bridge interface so a policy module (mod-branding) supplies
   budget/archetype/cost/socket policy and consumes `ReitemizedItem`.
