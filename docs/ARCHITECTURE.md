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
// Named ReforgeOp, not Reforge, to avoid colliding with the `Reforge` namespace under `using`.
struct ReforgeOp { ItemStat from; ItemStat to; uint32 amount; };
std::optional<StatBlock> ApplyReforge(StatBlock const& base, ReforgeOp const&, ItemChassis const&, IReforgeConfig const&);
```

`IReforgeConfig` (injected): `ReforgeMaxFraction()`, `StatWeight(archetype,stat)`,
`StatLegal(archetype,stat)`, `AutoSocketCount(slot,archetype)`, `AutoSocketColor()`, `MaxSockets()`.

### Pure decision helpers (`core/reforge/Charge.*`)

The decidable arithmetic every adapter path shares — kept pure (POD in/out) so it is unit-tested
without a world, and so the NPC, command and addon paths can never diverge:

- `ReforgeCap(source, maxFraction)` — the §5 bounded-fraction cap, `floor(source * maxFraction)`.
  `ApplyReforge` and every adapter pre-check derive their cap here (single source of truth).
- `AmountOptions(cap)` — the reforge NPC's 25/50/75/100%-of-cap menu (zero/duplicate buckets dropped).
- `ReforgeAllowedHere(requireNpc, nearReforger)` — the `Reforge.RequireNpc` gate decision.
- `PlanCharge(accepted, chosenEntry, playerHas)` — resolve the chosen currency and decide affordability
  (`Ok` / `NotAccepted` / `Insufficient`); the adapter only reads `playerHas` and performs the deduction.

---

## 4. Gem sockets (automatic)

Re-itemisation grants sockets automatically, config-driven so "automatic" is a **tunable**, not a
mandate: `AutoSocketCount == 0` means none. The default color is **Prismatic** (accepts any gem), so a
re-itemised drop is never awkward to gem. Total is clamped to `MaxSockets()` (WoW convention ≈3) so a
misconfigured template can never over-socket. Gem power sits **on top** of the stat budget and is
bounded only by the socket cap — a deliberate, host-tunable power lever (mod-branding gates it behind
essence / heroic tier). *Future:* colored sockets + socket-set bonuses (v1 grants prismatic only).

---

## 5. Invariants (all tested — `tests/`, 51 cases: re-itemisation, currency, protocol, decision helpers)

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
│   │   ├── Currency.h / .cpp         # ParseCurrencyCosts, FindCost (§10)
│   │   ├── Charge.h / .cpp           # ReforgeCap, AmountOptions, ReforgeAllowedHere, PlanCharge (§3)
│   │   └── Protocol.h / .cpp         # the §11 wire codec (RFRG frames)
└── tests/
    ├── standalone/CMakeLists.txt     # FetchContent gtest 1.12.1; globs src/core + tests; builds reforge_core_tests
    ├── fakes/FakeReforgeConfig.h      # deterministic DI test double
    ├── reforge/ReitemizeTest.cpp      # ArchetypeTemplate/ResolveSockets/ApplyReforge matrix
    ├── reforge/CurrencyTest.cpp       # currency parse + resolve
    ├── reforge/ChargeTest.cpp         # cap / amount menu / RequireNpc gate / charge + insufficient funds
    └── addon/ProtocolTest.cpp         # RFRG frame encode/decode parity
```

Fast loop: `cmake -S tests/standalone -B build && cmake --build build && ctest --test-dir build`.

---

## 7. Build order (sequencing)

1. **Slice 1 — Re-itemisation engine** ✓: `ArchetypeTemplate`, `ResolveSockets`,
   `Reitemize`, `ApplyReforge` + 20 tests. Pure, host-agnostic.
2. **Slice 2 — Server adapter** ✓ (§8–§11): the stat-carrying **enchant-slot vehicle** (per-item stats
   without a client patch), the reforge NPC/command surface, per-item persistence, `IReforgeConfig`
   over `sConfigMgr`, a **client addon** (player-directed reforge paying **any currency**), and its
   pure wire codec.
3. **Slice 3 — Host integration seam:** a bridge interface so a policy module (mod-branding) supplies
   budget/archetype/cost/socket policy and consumes `ReitemizedItem` (future).

---

## 8. Server adapter (`src/`)

The adapter is the only place AzerothCore headers appear. It keeps the same discipline as the pure
core: no long-lived `Player*`/`Item*` (resolve from `ObjectGuid` at use), config snapshotted from
`sConfigMgr`, `PreparedStatement` for persistence, typed helpers, `{}`-format logging.

```
src/
├── mod_reforge_loader.{h,cpp}   # Addmod_reforgeScripts() → fans out to each feature's AddSC
├── ServerReforgeConfig.{h,cpp}  # production IReforgeConfig over sConfigMgr (snapshot on load/reload)
├── ReforgeStatMap.{h,cpp}       # ItemStat ⇄ ItemModType mapping (the ONE AC-enum bridge)
├── ItemChassisBuilder.{h,cpp}   # live Item → StatBlock + ItemChassis (native proto stats)
├── ReforgeMgr.{h,cpp}           # apply/clear a reforge, currency charge, persistence, cache
├── ReforgeVehicleScripts.cpp    # PlayerScript: the two stat-apply hooks (the enchant-slot vehicle)
├── ReforgeGossipScripts.cpp     # Reforge NPC gossip (works without the addon)
├── ReforgeCommandScripts.cpp    # `.reforge …` — also the client addon's inbound API
└── ReforgeAddonScripts.cpp      # WorldScript/PlayerScript: config load + LANG_ADDON state pushes
```

`ReforgeMgr` is a singleton (`sReforgeMgr`) holding the config snapshot and a per-item reforge cache
keyed by item `ObjectGuid`, backed by the `character_item_reforge` table. It never stores a live
pointer; every op takes a `Player*`/`Item*` from the caller and resolves state by GUID.

Reforge validation is **not re-implemented** in the adapter: `ReforgeMgr` builds the item's
`StatBlock` + `ItemChassis` (via `ItemChassisBuilder`) and calls the pure-core `ApplyReforge`, so the
budget-conserving / bounded-fraction / legality invariants (§5) hold identically for the NPC path, the
command path, and the addon path.

## 9. The enchant-slot stat vehicle (§8 detail, no client patch)

A reforge moves `amount` points from a native item stat (`from`) into another stat (`to`). Applying
that to a live character without patching the client uses **two** server-side apply hooks — one per
direction — because a WotLK `SpellItemEnchantment` STAT effect can only carry a **positive** amount
(the core reads it as `float(uint32)`, so a negative amount is impossible):

- **`from` (−amount)** — `PlayerScript::OnPlayerApplyItemModsBefore`: when the item's own `from` stat
  line is applied, subtract `amount` from `val`. `from` is always a stat the item natively has (you
  can only reforge *out of* an existing stat), and `amount ≤` that stat's value (§5 cap), so `val`
  stays ≥ 0.
- **`to` (+amount)** — a small **fixed pool** of custom enchantments (one per destination
  `ItemModType`), seeded once into `spellitemenchantment_dbc` (world DB). A reforge sets the item's
  `PRISMATIC_ENCHANTMENT_SLOT` to `REFORGE_ENCHANT_BASE + toModType`. The per-item **amount** is not
  baked into the shared enchant row (that would collide across items/players); it is injected at apply
  time by `PlayerScript::OnPlayerApplyEnchantmentItemModsBefore` (whose `enchant_amount` is
  `uint32&`), read from the `character_item_reforge` row for that item GUID.

The `character_item_reforge` row (`item_guid, stat_from, stat_to, amount`) is the single source of
truth; the enchant slot only records *which* destination stat. On login/equip the core re-applies both
hooks, so a reforge survives restarts with no runtime DBC generation. Clearing a reforge deletes the
row and clears the prismatic slot. The client never needs the enchant in its own DBC — stats apply
server-side; the addon renders the human-readable reforge.

## 10. Currency (pay a reforge with ANY currency)

Cost is expressed as **`<item-entry> × <count>`**, where item-entry `0` means **gold** (copper) and any
other value is an item entry used as a token (emblems, marks, a custom currency item, crafting mats…).
The server accepts a **list** of alternatives; the player chooses which to pay with — hence "any
currency". The list is a config string parsed by the pure `core/reforge/Currency` helpers (fully
unit-tested, no AC types):

```
Reforge.Cost.Currencies = "0:100000, 43228:5, 40752:2"
;  gold 10g            5× Emblem of Frost   2× Emblem of Triumph
```

`core/reforge/Currency`:
- `struct CurrencyCost { uint32 entry; uint32 count; }`
- `std::vector<CurrencyCost> ParseCurrencyCosts(std::string_view)` — tolerant CSV of `entry:count`;
  drops malformed/zero-count records; deterministic order preserved.
- `std::optional<CurrencyCost> FindCost(costs, uint32 entry)` — the chosen currency's required count,
  or `nullopt` if that entry isn't an accepted currency.

The adapter (`ReforgeMgr`) reads the player's balance (`GetMoney` for gold, `GetItemCount` for a token)
and hands it to the pure `PlanCharge` (`core/reforge/Charge`), which resolves the chosen currency and
decides `Ok` / `NotAccepted` / `Insufficient`; on `Ok` the adapter deducts via `ModifyMoney` (gold) or
`DestroyItemCount` (token). Charging is atomic with the reforge write (validate → charge → persist →
apply; abort leaves nothing changed).

## 11. Client addon (`client-addon/Reforge/`) + wire codec

A pure renderer + command driver, mirroring mod-branding's transport. Two channels, both over the
stock 3.3.5a `LANG_ADDON` path (no client patch, no custom opcode):

- **Inbound (client → server): the built-in AzerothCore addon command channel.** The addon issues the
  `.reforge …` chat commands (§8 `ReforgeCommandScripts`) through `AddonChannelCommandHandler`
  (`AzerothCore\t` framing) and reads back the command's `m`/`o`/`f` replies. So the command surface
  *is* the plugin's API and everything also works from plain chat / an NPC with no addon installed.
- **Outbound (server → client): `RFRG` state pushes** as `LANG_ADDON` whispers (never `CHAT_MSG_ADDON`
  — that crashes a receiving 3.3.5a client), encoded by the pure `core/reforge/Protocol` codec and
  decoded by `Comms.lua` with the identical grammar (tab fields, `;` records, `:` sub-fields; permille
  for fractions; ≤255-byte frames).

`core/reforge/Protocol` frames (`RFRG\t<KIND>\t…`, `ProtocolVersion` bumped in lock-step with the Lua
`ns.PROTOCOL`):

```
RFRG\tHELLO\t<version>\t<enabled>
RFRG\tCUR\t<entry:count;…>                 accepted currencies (§10)
RFRG\tCFG\t<maxFractionPermille>           the §5 reforge cap, for the UI slider
RFRG\tITEM\t<slot:from:to:amount;…>        the character's current reforges (equipped items)
```

The addon UI (`Panel.lua`) lets the player pick an equipped item, a `from` stat, a legal `to` stat, an
`amount` (bounded by the `CFG` fraction), and a **currency** from the `CUR` list with its shown count,
then submits `.reforge do <slot> <from> <to> <amount> <currencyEntry>`. `Comms.lua` owns the transport
+ decode + a small callback registry; `Panel.lua` subscribes and renders.
