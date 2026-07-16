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

## 5. Invariants (all tested — `tests/`: re-itemisation, currency, protocol, decision helpers, weapon scaling, blocklist, budget scaling)

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
│   │   ├── WeaponScale.h / .cpp      # ScaleWeaponDamage, WeaponScaleFactor (§12)
│   │   ├── Blocklist.h / .cpp        # IsBlocked + id/slot/armorclass parsers (§13)
│   │   ├── BudgetScale.h / .cpp      # TargetBudget, ScaleFactorPermille, ScaleStatBlock (§14)
│   │   └── Protocol.h / .cpp         # the §11 wire codec (RFRG frames)
└── tests/
    ├── standalone/CMakeLists.txt     # FetchContent gtest 1.12.1; globs src/core + tests; builds reforge_core_tests
    ├── fakes/FakeReforgeConfig.h      # deterministic DI test double
    ├── reforge/ReitemizeTest.cpp      # ArchetypeTemplate/ResolveSockets/ApplyReforge matrix
    ├── reforge/CurrencyTest.cpp       # currency parse + resolve
    ├── reforge/ChargeTest.cpp         # cap / amount menu / RequireNpc gate / charge + insufficient funds
    ├── reforge/WeaponScaleTest.cpp    # weapon-damage scaling: up/down/equal/zero, ratio, determinism (§12)
    ├── reforge/BlocklistTest.cpp      # IsBlocked (all axes + OR) + id/slot/armorclass parsers (§13)
    ├── reforge/BudgetScaleTest.cpp    # target budget / quality mult / downscale guard / exact-total (§14)
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

---

## 12. Weapon-damage scaling on re-itemization (issue and-elf/mod-reforge#7)

A reforge re-itemizes a drop for a **new level range**. Stat *budget* scaling (issue #8) is one half; a
**weapon** actively used at that new range must also have its **min/max damage (and thus DPS)** scaled
for the source→target level transition, or a re-itemized low-level weapon hits far too soft (or a
down-scaled one too hard). This scaling is a **tunable**, configured under its own `Reforge.WeaponScale.*`
namespace (deliberately disjoint from #8's `Reforge.Scale.*`, so the two features develop and merge
independently).

### 12.1 Pure core (`src/core/reforge/WeaponScale.{h,cpp}`)

Dependency-free, POD in / POD out, fully unit-tested (`tests/reforge/WeaponScaleTest.cpp`).

```cpp
struct WeaponDamage { double min = 0.0; double max = 0.0; };   // mirrors ItemTemplate::_Damage (floats)

// Clamped (>= 0) multiplicative factor the injected policy returns for a fromLevel->toLevel transition.
double       WeaponScaleFactor(uint32_t fromLevel, uint32_t toLevel, IReforgeConfig const&);

// Scale a weapon's min AND max by that single factor -> the min:max RATIO is preserved exactly.
// Deterministic (pure arithmetic, no RNG, no rounding in-core). Degenerate: non-positive factor => 0
// damage; zero source stays 0; a well-behaved policy returns 1.0 for fromLevel == toLevel (identity).
WeaponDamage ScaleWeaponDamage(WeaponDamage const& source, uint32_t fromLevel, uint32_t toLevel,
                               IReforgeConfig const&);
```

The scaling **curve is injected**, not baked in: `IReforgeConfig` gains one virtual,

```cpp
// Multiplicative weapon-damage factor for a source->target level transition (>1 up, <1 down, 1 = none).
virtual double WeaponDamageScale(uint32_t fromLevel, uint32_t toLevel) const = 0;
```

so the core only applies the factor (multiply both endpoints), guaranteeing the ratio invariant and
determinism regardless of the policy. **In-core rounding is intentionally omitted** — rounding min and
max independently would perturb the exact ratio; the item template stores `float` and the adapter
narrows the doubles to the proto's precision at the point of application. "Deterministic" here means:
identical inputs ⇒ identical output (pure arithmetic).

### 12.2 Config policy (`ServerReforgeConfig`, `Reforge.WeaponScale.*`)

`ServerReforgeConfig::WeaponDamageScale` implements a **geometric per-level** curve — WoW weapon DPS
grows roughly multiplicatively with level:

```
factor = clamp( (1 + PerLevelPct/100) ^ (toLevel - fromLevel),  MinFactor,  MaxFactor )
```

returning `1.0` when disabled, when `fromLevel == 0`, or when `fromLevel == toLevel`. Keys (all read via
`sConfigMgr->GetOption<float>` — never `<double>`, per the module's dynamic-`.so` note):

| key | default | meaning |
|---|---|---|
| `Reforge.WeaponScale.Enable`      | `1`    | master switch for weapon-damage scaling |
| `Reforge.WeaponScale.PerLevelPct` | `3.0`  | percent DPS change per level of the source→target delta |
| `Reforge.WeaponScale.MinFactor`   | `0.1`  | lower clamp on the resulting factor (down-scaling floor) |
| `Reforge.WeaponScale.MaxFactor`   | `10.0` | upper clamp on the resulting factor (up-scaling ceiling) |

### 12.3 Adapter — live application with NO client patch

Applying scaled weapon damage to a **live** weapon uses an existing per-item core hook —
`PlayerScript::OnPlayerApplyWeaponDamage(player, slot, proto, float& minDamage, float& maxDamage,
damageIndex)`, called inside `Player::_ApplyWeaponDamage` for every damage line right before
`SetBaseWeaponDamage`. Because `minDamage`/`maxDamage` are **mutable references**, the adapter rewrites
them server-side per item-instance; no `SpellItemEnchantment` of `ITEM_ENCHANTMENT_TYPE_DAMAGE` and no
client DBC entry are needed. The hook fires on equip and on login, so a scaled weapon survives restarts
with no persisted weapon state and no runtime DBC generation — the analogue of §9's stat vehicle for
weapon damage.

`ReforgeVehicleScript::OnPlayerApplyWeaponDamage` (same class as the §9 stat hooks):

1. gate on `Reforge.Enable` + `Reforge.WeaponScale.Enable`;
2. resolve the equipped item at `slot`; **only re-itemized weapons scale** — the gate is the presence
   of a `character_item_reforge` row (`sReforgeMgr->GetReforge(item->GetGUID())`), so an untouched
   weapon is never altered;
3. `fromLevel = proto->RequiredLevel` (the weapon's own intended level; falls back to `proto->ItemLevel`
   when `RequiredLevel == 0`), `toLevel = player->GetLevel()` (the "current level" target, matching
   #8's target-level choice);
4. `auto const scaled = ScaleWeaponDamage({minDamage, maxDamage}, fromLevel, toLevel, cfg);` then
   `minDamage = float(scaled.min); maxDamage = float(scaled.max);`.

**No new persistence** is required: the factor is a pure function of the proto's source level, the
player's current level and config, re-derived every apply. The existing reforge row is reused purely as
the "this item was re-itemized" flag — no schema change, no new SQL.

**Reconciliation with #8 (documented seam).** #7 chooses source/target levels from the proto and the
player independently of #8. When the two branches merge, `fromLevel`/`toLevel` should be sourced from the
same re-itemization decision #8 produces (its target level + the item's source level) so weapon damage
and stat budget scale off one consistent transition; the pure `WeaponScale` core and its config are
unaffected by that reconciliation (only the two lines in step 3 change).

---

## 13. Item blocklist (and-elf/mod-reforge#5)

Some items must never be reforgeable (legendaries, heirlooms, a hand-picked list of specific drops, a
whole armour material, a slot). The blocklist is **config-driven** and evaluated on **three
independent axes** that combine with **OR** — an item is blocked if it matches **any** axis:

1. **By item entry id** — `Reforge.Blocklist.ItemIds` (CSV of `item_template.entry`), e.g. `"49623,50735"`.
2. **By equip slot** — `Reforge.Blocklist.Slots` (CSV of slot names, reusing the core `EquipSlot`
   vocabulary), e.g. `"trinket,ranged"`.
3. **By item type**, split into two sub-lists (the issue's "separate configuration for ids and type"):
   - `Reforge.Blocklist.Qualities` — CSV of item-quality names (`poor, common, uncommon, rare, epic,
     legendary, artifact, heirloom`; WoW `ItemQualities` ordinals 0–7), e.g. `"legendary,heirloom"`.
   - `Reforge.Blocklist.ArmorClasses` — CSV of armour-material names (`cloth, leather, mail, plate`),
     e.g. `"cloth"`.

**Why this "by type" set.** Quality and armour-class are the two type dimensions an operator actually
wants to protect in bulk: quality guards *legendaries/heirlooms* (the classic "don't let players
rebalance a legendary" case) and armour-class lets a realm exclude an entire material. Both map cleanly
onto proto fields (`Quality`, weapon/armour `SubClass`) and onto vocabularies that already exist (the
core `ArmorClass` enum; WoW's fixed quality ordinals). Inventory-type category is already covered by the
slot axis, so a separate inventory-type list would only duplicate it.

### Split (pure core owns the decision)

- **Pure core `core/reforge/Blocklist.{h,cpp}`** — the decision + the tolerant CSV parsers, POD in/out,
  unit-tested exhaustively with no worldserver:
  - `struct BlockKey { uint32 entry; EquipSlot slot; ArmorClass armor; uint8 quality; }` — the item
    reduced to the blockable facts.
  - `struct BlockPolicy { vector<uint32> itemIds; vector<EquipSlot> slots; vector<ArmorClass>
    armorClasses; vector<uint8> qualities; }` — the parsed config lists.
  - `bool IsBlocked(BlockKey const&, BlockPolicy const&)` — OR across the four lists. An empty policy
    blocks nothing; `EquipSlot::None` / `ArmorClass::None` in the key match only if explicitly listed.
  - Parsers (mirror `Currency.cpp`'s tolerant style — malformed tokens dropped, never throw):
    `ParseIdList` (CSV of `uint32`, drops `0`/malformed), `ParseSlotList` / `SlotFromName` (EquipSlot is
    a core enum), `ParseArmorClassList` / `ArmorClassFromName` (ArmorClass is a core enum).
  - Quality **names** are the one thing the core cannot own without hardcoding an AzerothCore enum, so
    quality name→ordinal parsing lives in the adapter (below); the core's quality axis is purely numeric.
- **Adapter `ServerReforgeConfig`** — snapshots the four `Reforge.Blocklist.*` options on load/reload
  into a `BlockPolicy` (item ids / slots / armour classes via the core parsers; qualities via a
  name→ordinal map that references AC's `ItemQualities` enum — the ADAPTER is the only place that enum
  appears). Exposes `bool IsItemBlocked(ItemTemplate const*)` which builds a `BlockKey` (entry =
  `ItemId`, slot from `InventoryType`, armour from `SubClass`, quality from `Quality`) and calls the
  pure `IsBlocked`. The `InventoryType → EquipSlot` bridge lives in `ReforgeStatMap` (the AC-enum
  bridge layer).
- **Enforcement.** `ReforgeMgr::ApplyReforge` gains an early guard (after the enabled/near-NPC checks)
  returning `"This item cannot be reforged."` when `IsItemBlocked` is true — so the NPC, command and
  addon paths are all covered by one check. `ReforgeGossipScripts::SendMain` additionally hides blocked
  items from the reforge NPC menu, so a blocked item never even appears as reforgeable.

Sensible defaults are empty on every axis, so nothing is blocked out of the box.
---

## 14. Level/rarity budget scaling (issues #8, #6)

A reforge also **re-itemises the item's stat BUDGET to the reforging player's CURRENT level** so an
early drop stays useful as the character grows. This is layered ON TOP of the §5 stat-move reforge: a
reforge first scales the whole budget, then moves a bounded fraction of one (scaled) stat into another.

### 14.1 Target budget — pure core (`core/reforge/BudgetScale.{h,cpp}`)

The target budget is a deterministic function of the player's level and the item's quality only —
**independent of the item's native budget** (that is the point: every item normalises to *your* level):

```
TargetBudget(level, quality, cfg) = round( cfg.LevelBudgetPoints(level) * cfg.QualityBudgetMultiplier(quality) )
```

Both the level curve and the per-quality multiplier are injected via `IReforgeConfig` (three new
virtuals), so the core stays generic and unit-tested:

```cpp
double LevelBudgetPoints(uint32_t level)   const;   // the level curve (host-tunable)
double QualityBudgetMultiplier(uint8_t q)  const;   // per WotLK quality 0..6 (poor..artifact)
bool   AllowDownscale()                    const;   // global twink-safety master switch
```

Scaling primitives (all POD-in / POD-out, deterministic, edge-cases handled):

- `uint32_t TargetBudget(level, quality, cfg)` — the formula above; clamps to ≥ 0.
- `uint32_t ScaleFactorPermille(sourceBudget, targetBudget)` — the per-stat multiplier the live stat
  vehicle applies, expressed in **permille** (1000 = identity). `sourceBudget == 0 ⇒ 1000`.
- `StatBlock ScaleStatBlock(base, targetBudget)` — scales `base` proportionally so `Total()` lands
  **exactly** on `targetBudget` (the same largest-remainder apportionment as `ArchetypeTemplate`);
  `base.Total() == 0` or `targetBudget == 0 ⇒ zeroed`. Used for the player-facing reforge math (cap,
  legality, displayed amounts).

### 14.2 Down-scale guard (twink protection)

Up-scaling is always allowed. **Down-scaling is blocked for trinkets and for items carrying an on-use /
on-equip effect** (no level-19 +500 AP raid trinket), and can be disabled globally. The decision is a
pure predicate:

```cpp
bool     DownscalePermitted(bool isDownscale, bool isTrinket, bool hasEffect, bool allowDownscale);
uint32_t ClampScalePermille(uint32_t rawPermille, bool isTrinket, bool hasEffect, bool allowDownscale);
```

`DownscalePermitted` is `true` for any up-scale (`!isDownscale`); for a down-scale it is
`allowDownscale && !isTrinket && !hasEffect`. When a down-scale is not permitted the effective target is
clamped back to the native budget (identity, permille 1000) — the item simply keeps its native stats.

### 14.3 Adapter (`ReforgeMgr` + the stat vehicle)

`ReforgeMgr::ApplyReforge` computes the scale as part of every reforge: build the native `StatBlock`,
read `player->GetLevel()`, the `ItemTemplate` quality, `InventoryType == INVTYPE_TRINKET`, and whether
any `Spells[]` entry has `ITEM_SPELLTRIGGER_ON_USE`/`ON_EQUIP`; derive the effective target (guarded),
persist the resulting **scale permille**, and run the §5 reforge on the *scaled* block. The
`character_item_reforge` row gains a `scale` column (permille, default 1000).

The **stat vehicle** (`OnPlayerApplyItemModsBefore`) multiplies every native (budget-contributing) stat
line by the stored permille before the existing `from` subtraction — so the whole item scales, and the
single-stat reforge composes on top of the scaled budget. Per-stat integer flooring means the live
apply is a close approximation of `ScaleStatBlock`'s exact total (an accepted, safe rounding tradeoff;
the `from` line is still clamped ≥ 0).

### 14.4 Re-reforge flag (#6)

`Reforge.ReReforge.Allowed` (**on by default**) lets a player re-reforge/re-scale an already-reforged
item — e.g. re-run it each level to bump the budget. When **off**, an item that already carries a
reforge is rejected until it is cleared first. Enforced in the adapter (`ReforgeMgr`), before charging.

### 14.5 Config (`Reforge.Scale.*` + `Reforge.ReReforge.Allowed`)

`Reforge.Scale.Enable` (master, default on), `Reforge.Scale.LevelBudget.Base` /
`Reforge.Scale.LevelBudget.PerLevel` (the linear level curve `Base + PerLevel*level`),
`Reforge.Scale.Quality.Multipliers` (CSV of 7 floats, quality 0..6), `Reforge.Scale.AllowDownscale`
(default on) and `Reforge.ReReforge.Allowed` (default on). The curve should be tuned to the realm's
itemisation.
