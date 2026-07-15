# mod-reforge — End-to-End Manual QA Checklist

Status: **manual QA script** · Target: AzerothCore (WotLK 3.3.5a) · Refs issue and-elf/mod-reforge#1

This is the live-client verification the headless test suite **cannot** cover. The pure decision logic
(re-itemisation, reforge cap, currency parse + charge decision, RequireNpc gate, addon wire codec) is
unit-tested in `tests/` (run `cmake -S tests/standalone -B build && ctest --test-dir build`). Everything
below needs a running **worldserver + populated MySQL + a 3.3.5a client**, so it is executed by hand and
ticked off here.

Legend: `[ ]` = to do, `[x]` = pass, `[!]` = fail (file a bug). Fill in the sign-off table at the end.

---

## 0. What this verifies (issue #1 checkboxes)

- [ ] A — Reforge NPC gossip wizard: item → from → to → amount → currency → confirm
- [ ] B — `.reforge do / clear / list / sync` (chat and addon command channel)
- [ ] C — Currency charge: gold (entry 0) and item-token currency; insufficient-funds path
- [ ] D — Enchant-slot stat vehicle: char sheet shows −from/+to; unequip/relog reverses; persists across restart
- [ ] E — `Reforge.RequireNpc` gating (near vs away from the Reforger)
- [ ] F — Client addon `/reforge` panel renders CUR/CFG/ITEM and submit drives `.reforge do`
- [ ] G — Seeded `spellitemenchantment_dbc` pool applies the destination stat (amount injected at apply)

---

## 1. Reference tables (used throughout)

**`ItemStat` ordinals** (persisted in `character_item_reforge.stat_from/stat_to`; also the command/addon stat tokens):

| ord | token (command) | ord | token | ord | token |
|----:|-----------------|----:|-------|----:|-------|
| 0 | stamina | 6 | spellpower | 12 | mp5 |
| 1 | strength | 7 | hit | 13 | defense |
| 2 | agility | 8 | crit | 14 | dodge |
| 3 | intellect | 9 | haste | 15 | parry |
| 4 | spirit | 10 | expertise | 16 | block |
| 5 | attackpower | 11 | armorpen | 17 | resilience |

Default legal reforge stats (source **and** destination): `hit, crit, haste, expertise, armorpen, spirit, dodge, parry, block, resilience, defense, mp5`.

**Equipment slot indices** (the `<slot>` arg to `.reforge`, and the addon's serverSlot = clientInvSlot − 1):

| slot | idx | slot | idx | slot | idx |
|------|----:|------|----:|------|----:|
| Head | 0 | Waist | 5 | Finger1 | 10 |
| Neck | 1 | Legs | 6 | Finger2 | 11 |
| Shoulders | 2 | Feet | 7 | Trinket1 | 12 |
| Body/Shirt | 3 | Wrists | 8 | Trinket2 | 13 |
| Chest | 4 | Hands | 9 | Back | 14 |
| MainHand | 15 | OffHand | 16 | Ranged | 17 |
| Tabard | 18 | | | | |

**Reforge enchant IDs** = `Reforge.EnchantBase (900200)` + destination `ItemModType`. Common ones:

| destination | ItemModType | enchant ID |
|-------------|------------:|-----------:|
| spirit | 6 | 900206 |
| defense | 12 | 900212 |
| dodge | 13 | 900213 |
| hit | 31 | 900231 |
| crit | 32 | 900232 |
| resilience | 35 | 900235 |
| haste | 36 | 900236 |
| expertise | 37 | 900237 |

**Cap formula** (default `Reforge.MaxFraction = 0.40`): a reforge may move at most
`cap = floor(sourceStatValue × 0.40)` points. Pick amounts `1 … cap`.

---

## 2. One-time setup

- [ ] **2.1 Build with the module.** `mod-reforge` is present under `modules/` and the worldserver was
  built with it (static or dynamic). Confirm at worldserver startup the log shows the reforge scripts
  registering (no `Addmod_reforgeScripts` errors).
- [ ] **2.2 Apply the module SQL** to the three DBs (idempotent; safe to re-run):
  ```sql
  SOURCE data/sql/db-world/2026_07_14_00_reforge_world.sql;        -- into acore_world
  SOURCE data/sql/db-characters/2026_07_14_00_reforge_characters.sql; -- into acore_characters
  SOURCE data/sql/db-auth/2026_07_14_00_reforge_rbac.sql;          -- into acore_auth
  ```
- [ ] **2.3 Verify the enchant pool seeded** (backs checkbox G):
  ```sql
  SELECT ID, Effect_1, EffectArg_1, Name_Lang_enUS
  FROM acore_world.spellitemenchantment_dbc WHERE ID BETWEEN 900200 AND 900299 ORDER BY ID;
  ```
  Expect 18 rows (e.g. `900231 | 5 | 31 | Reforged: Hit`). `Effect_1 = 5` (STAT), `EffectArg_1` = the
  destination ItemModType.
- [ ] **2.4 Verify the RBAC permission** and its links:
  ```sql
  SELECT * FROM acore_auth.rbac_permissions WHERE id = 100000;              -- 'Command: reforge'
  SELECT * FROM acore_auth.rbac_linked_permissions WHERE linkedId = 100000; -- linked to 192-195
  ```
- [ ] **2.5 Config present.** `conf/mod_reforge.conf` exists (copied from `.dist`). Defaults:
  `Reforge.Enable=1`, `Reforge.RequireNpc=1`, `Reforge.NpcRange=10.0`, `Reforge.NpcEntry=900100`,
  `Reforge.MaxFraction=0.40`, `Reforge.Cost.Currencies="0:100000"`, `Reforge.Addon.Enable=1`.
- [ ] **2.6 Spawn the Reforger** near your test spot: target a location and run in-game `.npc add 900100`
  (or `.npc add temp 900100`). The NPC "Reforger" (display 10045) should appear, gossip-flagged.
- [ ] **2.7 Test character.** A level-80 with a couple of equipped items carrying reforgeable secondary
  stats (e.g. a chest with **Hit** and some **Haste**). If needed, `.additem <entry>` a known piece and
  equip it. Note the exact **Hit** and **Crit** values on the Character sheet (C) before starting.

> Tip — find your item GUID at any time from the source-of-truth table after a reforge:
> `SELECT * FROM acore_characters.character_item_reforge;`

---

## 3. Checkbox A — Reforge NPC gossip wizard

With `Reforge.RequireNpc=1`, stand next to the Reforger and open its gossip.

- [ ] **A.1** Gossip lists each equipped item that has a legal reforge **source** stat (by item name).
      An item with no legal source stat is **not** listed; if none qualify you see
      "You have no reforgeable equipped items."
- [ ] **A.2** Select an item → menu lists **"Reduce `<stat>` (`<value>`)"** for each legal source stat the
      item actually has (value > 0), plus a **"< Back"**.
- [ ] **A.3** Pick a source stat → menu lists **"... into `<stat>`"** for every legal destination
      **except** the chosen source. Pick a destination.
- [ ] **A.4** Amount step shows up to four options: **"Move N `<from>` → `<to>`"** for 25/50/75/100 % of the
      cap (zero/duplicate buckets omitted for small caps). Values match `floor(cap × f)`.
- [ ] **A.5** Currency step lists every accepted currency: **"Pay Xg Ys Zc"** for gold, **"Pay N× `<item>`"**
      for a token. Choosing one confirms.
- [ ] **A.6** On success you get **"Reforged N `<from>` into `<to>`."** and the gossip closes. Re-opening the
      NPC and selecting the item now offers **"Remove current reforge"** and the item shows "(reforged)".
- [ ] **A.7** DB reflects it: `SELECT * FROM acore_characters.character_item_reforge;` has the expected
      `stat_from`, `stat_to`, `amount` row.

---

## 4. Checkbox B — `.reforge` commands

Run these in the client chat (they also work verbatim from the addon command channel — see §8). Slot
numbers from §1.

- [ ] **B.1 do** — `.reforge do 4 hit crit 20 0` (chest, 20 Hit→Crit, pay gold). Replies
      "Reforged 20 hit into crit." (adjust the amount to ≤ your cap). Aliases work too
      (`.reforge do 4 hit crit 20` — currency defaults to the first accepted).
- [ ] **B.2 list** — `.reforge list` shows `slot 4: 20 hit -> crit`. Reforge a second item and confirm both
      list.
- [ ] **B.3 clear** — `.reforge clear 4` replies "Reforge removed." `.reforge list` no longer shows slot 4,
      and the `character_item_reforge` row for that item is gone.
- [ ] **B.4 sync** — `.reforge sync` replies "Reforge: state synced." (with the addon loaded this re-pushes
      CFG/CUR/ITEM — visible in §8).
- [ ] **B.5 validation** — bad inputs are rejected with a clear message, nothing charged:
  - [ ] unknown stat: `.reforge do 4 foo crit 10` → "unknown stat name…"
  - [ ] over the cap: `.reforge do 4 hit crit 99999` → "You may move at most `<cap>` hit…"
  - [ ] same stat: `.reforge do 4 hit hit 5` → "Pick two different stats."
  - [ ] illegal destination (default legal set excludes primaries): `.reforge do 4 hit strength 5` →
        rejected ("strength is not a legal reforge destination." / "not allowed").
  - [ ] source the item lacks: pick a slot/stat the item has 0 of → "This item has no `<stat>` to reforge."
  - [ ] empty slot: `.reforge do 3 hit crit 5` (shirt) → "nothing equipped in slot 3."

---

## 5. Checkbox C — Currency charge (gold, token, insufficient)

**Gold (entry 0):**
- [ ] **C.1** Note your gold. `.reforge do 4 hit crit <=cap> 0`. Money drops by exactly the configured
      copper (default 100000 = 10g). Reforge succeeds.
- [ ] **C.2 insufficient gold** — reduce yourself below the price (e.g. `.modify money` to set a low
      balance, or on a fresh char) and retry. Reply: "Reforging costs 10g 0s 0c." **No** money is taken,
      **no** reforge row is written, the item is unchanged.

**Item-token currency:**
- [ ] **C.3** Set `Reforge.Cost.Currencies = "0:100000, 43228:5"` in the conf, `.reload config`.
      (43228 = Emblem of Frost; substitute any item entry you can mint.)
- [ ] **C.4** `.additem 43228 5`. Reforge choosing the token: `.reforge do 4 hit crit <=cap> 43228`
      (or pick "Pay 5× Emblem of Frost" in the NPC wizard). Succeeds; your Emblem count drops by 5.
- [ ] **C.5 insufficient token** — with fewer than 5 emblems, retry → "Reforging costs 5x Emblem of
      Frost." Nothing consumed, nothing written.
- [ ] **C.6 unaccepted currency** — `.reforge do 4 hit crit 10 99999` → "That is not an accepted currency
      here." Nothing charged.

---

## 6. Checkbox D — Enchant-slot stat vehicle (the headline live behaviour)

Use a chest (slot 4) that natively has, say, **40 Hit**. Cap = floor(40 × 0.40) = **16**. Reforge
`.reforge do 4 hit crit 16 0`.

- [ ] **D.1 char sheet −from/+to** — open the Character sheet. **Hit Rating is 16 lower** and **Crit
      Rating is 16 higher** than the pre-reforge baseline from §2.7. The item tooltip still shows its
      native stats (the swing is applied server-side).
- [ ] **D.2 unequip reverses** — unequip the chest. Hit/Crit return to their un-reforged values (minus
      the item's own contribution). Re-equip → the −16/+16 swing reappears. No stat drift after several
      equip/unequip cycles.
- [ ] **D.3 relog reverses/reapplies** — log out and back in with the item equipped. The −16/+16 swing is
      still present (re-applied on login). Worldserver log at startup shows `>> Loaded N item reforges.`
- [ ] **D.4 DB source of truth** —
      `SELECT * FROM acore_characters.character_item_reforge;` → row `stat_from=7, stat_to=8, amount=16`.
- [ ] **D.5 prismatic enchant slot recorded** — force a save (`.save` or log out), then:
      ```sql
      SELECT guid, enchantments FROM acore_characters.item_instance WHERE guid = <item_guid>;
      ```
      `enchantments` is space-separated triples `(id, duration, charges)` per slot; the **7th** triple
      (slot index 6 = PRISMATIC) has id **900232** (crit). The per-item amount is **not** stored in the
      enchant — it is injected at apply time from the reforge row (§ARCHITECTURE 9).
- [ ] **D.6 persists across worldserver restart** — with the reforge active, restart the worldserver.
      Log in: char sheet still shows the swing, `.reforge list` still shows it, the DB row and enchant
      slot are intact. (Proves no runtime DBC generation is needed.)
- [ ] **D.7 clear reverts everything** — `.reforge clear 4`. Char sheet returns to baseline, the reforge
      row is deleted, and the item's prismatic slot (7th triple) resets to `0 0 0`.

---

## 7. Checkbox E — `Reforge.RequireNpc` gating

- [ ] **E.1 gate ON, away** — with `Reforge.RequireNpc=1`, stand **>10y** from any Reforger and run
      `.reforge do 4 hit crit 5 0` → "You must be near a Reforger to do that." Nothing changes.
- [ ] **E.2 gate ON, near** — walk within `Reforge.NpcRange` of the Reforger and retry → succeeds.
- [ ] **E.3 clear is gated too** — away from the NPC, `.reforge clear 4` → same "must be near" refusal.
- [ ] **E.4 gate OFF** — set `Reforge.RequireNpc=0`, `.reload config`. Now `.reforge do …` and the addon
      panel work from **anywhere** (no NPC needed). Restore to `1` afterwards.

---

## 8. Checkbox F — Client addon `/reforge` panel

Install `client-addon/Reforge/` into `Interface/AddOns/Reforge/` and enable it (`Reforge.Addon.Enable=1`).

- [ ] **F.1 login push** — log in with the addon enabled. No Lua errors. (On login the server pushes
      HELLO + CFG + CUR + ITEM.) A protocol-version mismatch would print a yellow warning — should not
      appear (addon `ns.PROTOCOL` == server `ProtocolVersion`).
- [ ] **F.2 open panel** — `/reforge` toggles the panel.
- [ ] **F.3 CUR rendered** — the **Currency** dropdown lists exactly the realm's accepted currencies in
      configured order ("Pay 10g 0s 0c", "Pay 5× Emblem of Frost", …). Matches §5's config.
- [ ] **F.4 CFG rendered** — the amount hint reads "max 40% of the stat" (or whatever
      `Reforge.MaxFraction × 100` is). Change `Reforge.MaxFraction`, `.reload config`, `/reforge` (or the
      panel's Sync on show) → the hint updates.
- [ ] **F.5 ITEM rendered** — pick an equipped item that is reforged; the status line shows
      "Current: N `<from>` → `<to>`" for it; an un-reforged item shows "Not reforged."
- [ ] **F.6 submit drives `.reforge do`** — pick item / from / to / amount / currency, click **Reforge**.
      The chat shows the "Reforged …" reply (blue "Reforge:" prefix), the char sheet swings (as §6), and
      the panel status refreshes to the new "Current:" — proving the server re-pushed ITEM after the op.
- [ ] **F.7 remove** — the **Remove reforge** button clears the selected slot (chat "Reforge removed.",
      status → "Not reforged.").
- [ ] **F.8 no-addon parity** — everything in F is optional sugar: with the addon **disabled**, the NPC
      (§3) and chat commands (§4) still perform every operation.

---

## 9. Checkbox G — Seeded enchant pool applies the destination stat

This is the mechanism behind D.1/D.5. Confirm the pool is the thing doing the work:

- [ ] **G.1** The 18 rows from §2.3 are loaded into the world (no startup error about
      `spellitemenchantment` for IDs 9002xx).
- [ ] **G.2** Reforge into **several different destinations** on different items (e.g. → crit 900232,
      → haste 900236, → spirit 900206) and confirm each shows the correct **destination** stat gain on the
      char sheet, and the matching enchant ID in the item's prismatic slot (§1 table).
- [ ] **G.3 amount is per-item, not baked into the shared row** — reforge two different items both into
      **crit** but with **different amounts** (e.g. 16 and 8). Both use enchant 900232, yet each item
      shows its own +crit on the sheet (amount injected via `OnPlayerApplyEnchantmentItemModsBefore` from
      the item's `character_item_reforge` row). This is the key correctness check for the shared pool.

---

## 10. Teardown (optional)

- [ ] Remove the test reforges (`.reforge clear <slot>` on each) or
      `DELETE FROM acore_characters.character_item_reforge WHERE item_guid IN (…);`
- [ ] Despawn the test Reforger (`.npc delete` while targeting it) if it was a temp spawn.
- [ ] Restore `Reforge.Cost.Currencies` and `Reforge.RequireNpc` to their intended production values.

---

## 11. Sign-off

| # | Area | Result | Tester | Build / commit | Notes |
|---|------|--------|--------|----------------|-------|
| A | NPC gossip wizard | | | | |
| B | `.reforge` commands | | | | |
| C | Currency (gold/token/insufficient) | | | | |
| D | Enchant-slot vehicle + persistence | | | | |
| E | RequireNpc gating | | | | |
| F | Addon panel | | | | |
| G | Enchant pool applies destination | | | | |

All rows must be **pass** before the issue #1 live-verification checkboxes are ticked.
