# Reforge — client addon

A small WotLK 3.3.5a addon: a `/reforge` panel to rebalance an equipped item's stats, paying any
currency the realm accepts. Optional — the server also exposes a **Reforge NPC** (gossip) and the
`.reforge` chat commands, which need no addon.

## Install

Copy the `Reforge/` folder into `World of Warcraft/Interface/AddOns/`. Enable it at the character
select screen. Open with `/reforge`.

## How it works

Two channels, both over the stock `LANG_ADDON` path — no client patch:

- **State (server → client):** the server pushes `RFRG` frames as `LANG_ADDON` whispers (never
  `CHAT_MSG_ADDON`, which crashes a receiving 3.3.5a client). `Comms.lua` decodes them into
  `ns.state` — the accepted currencies (`CUR`), the reforge cap (`CFG`), and your equipped items'
  current reforges (`ITEM`) — mirroring the server's pure codec
  (`modules/mod-reforge/src/core/reforge/Protocol.cpp`).
- **Requests (client → server):** the built-in **AzerothCore addon command channel** (prefix
  `AzerothCore`, opcode `i`). The panel issues the same `.reforge do …` command the NPC/chat use, and
  reads back its `m`/`o`/`f` replies. So the command surface *is* the addon's API.

If `Reforge.RequireNpc` is on (default), reforging only works while near a Reforge NPC — stand next to
one, then use the panel.

## Wire format (server → client)

```
RFRG\tHELLO\t<version>\t<enabled>
RFRG\tCFG\t<maxFractionPermille>              cap as x1000 (e.g. 400 = 40%)
RFRG\tCUR\t<entry:count;…>                    accepted currencies (entry 0 = gold copper)
RFRG\tITEM\t<slot:from:to:amount;…>           slot = server EQUIPMENT_SLOT (client INVSLOT - 1)
```

`from`/`to` are `Reforge::ItemStat` ordinals (see `ns.StatName` in `Comms.lua`). Keep `ns.PROTOCOL`
and the server's `ProtocolVersion` in lock-step when extending the grammar.

## Manual verification (needs a live client + server)

1. `Reforge.Enable = 1`, `Reforge.Addon.Enable = 1`; reload config. Log in → no protocol-mismatch warning.
2. `/reforge` → the panel lists your equipped items and the accepted currencies.
3. Stand by a Reforge NPC, pick an item, reduce a stat into another, choose a currency, click
   **Reforge** → chat shows the result; your character sheet reflects the moved stat; the panel shows
   the current reforge.
4. **Remove reforge** → the stat returns; the currency is not refunded.
