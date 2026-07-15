#ifndef MOD_REFORGE_SRC_REFORGESTATMAP_H
#define MOD_REFORGE_SRC_REFORGESTATMAP_H

#include "reforge/Reitemize.h"   // Stats.h (StatBlock/ItemStat) + ItemChassis
#include <cstdint>
#include <optional>
#include <string_view>

class ItemTemplate;

// Adapter bridge between the pure core's Reforge::ItemStat and AzerothCore's ItemModType, plus the
// live-item -> chassis builder. This is the ONE place the reforge stat enum meets AC's item stats.
namespace Reforge
{
    // The canonical WotLK ItemModType (see ItemTemplate.h) for a core ItemStat. Uses the COMBINED
    // rating types (ITEM_MOD_HIT_RATING, …) that modern 3.3.5a gear carries.
    uint32_t ToItemModType(ItemStat stat);

    // The core ItemStat for an ItemModType, or nullopt if it is not one we reforge. Only the canonical
    // (combined) rating types map; the legacy split ratings (melee/ranged/spell hit/crit/haste) are
    // intentionally unmapped so a reforge source lands on exactly one proto stat line.
    std::optional<ItemStat> FromItemModType(uint32_t mod);

    // Build the item's native stat composition as a StatBlock (only stats we understand, value > 0).
    StatBlock BuildStatBlock(ItemTemplate const* proto);

    // Build the identity-free chassis (budget + archetype/armor/slot selectors) for an item. On a bare
    // server the archetype does not gate legality (ServerReforgeConfig ignores it); a host module may.
    ItemChassis BuildChassis(ItemTemplate const* proto);

    // Short, stable, lower-case name for a stat ("hit", "spellpower", …) — used in config, chat
    // commands, gossip labels and the addon. "?" for COUNT / out of range.
    char const* StatName(ItemStat stat);

    // Parse a stat name (case-insensitive; accepts the StatName tokens and a few aliases) to an
    // ItemStat, or nullopt. Also accepts the raw ItemStat ordinal as a decimal string.
    std::optional<ItemStat> StatFromName(std::string_view name);
}

#endif // MOD_REFORGE_SRC_REFORGESTATMAP_H
