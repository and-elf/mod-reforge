#ifndef MOD_REFORGE_CORE_REFORGE_REITEMIZE_H
#define MOD_REFORGE_CORE_REFORGE_REITEMIZE_H

#include "ReforgeConfig.h"
#include "Stats.h"
#include <cstdint>
#include <optional>

namespace Reforge
{
    // The dropped item as the engine sees it: an identity-free chassis carrying a stat BUDGET and the
    // archetype/armor/slot that select its template. Identity (entry/name/model/effect) lives in the
    // host and is never touched here.
    struct ItemChassis
    {
        uint32_t      budget = 0;                        // total stat points at the item's ilvl band
        ItemArchetype archetype = ItemArchetype::CasterDps;
        ArmorClass    armor = ArmorClass::None;          // carried for the host's config tables
        EquipSlot     slot = EquipSlot::None;            // carried for the host's config tables
    };

    // Rebuild composition: distribute the chassis budget across the archetype's template stats,
    // spending the budget EXACTLY (largest-remainder apportionment) and DETERMINISTICALLY. A stat
    // receives points only if its template weight is > 0. An empty template (all weights 0) or a zero
    // budget yields a zeroed block — the host is expected to supply weights.
    StatBlock ArchetypeTemplate(ItemChassis const& chassis, IReforgeConfig const& cfg);

    // A single player-directed reforge: move `amount` points from `from` into `to`.
    struct Reforge
    {
        ItemStat from = ItemStat::Spirit;
        ItemStat to   = ItemStat::HitRating;
        uint32_t amount = 0;
    };

    // Resolve the gem sockets automatically granted to this chassis on re-itemization. Returns a layout
    // of AutoSocketCount(slot, archetype) sockets of AutoSocketColor(), clamped to MaxSockets(). Gem
    // power sits ON TOP of the stat budget (bounded by MaxSockets); the player fills the sockets as
    // normal. Deterministic.
    SocketLayout ResolveSockets(ItemChassis const& chassis, IReforgeConfig const& cfg);

    // The full re-itemization of a dropped chassis: a rebuilt stat block plus its auto-granted sockets.
    struct ReitemizedItem
    {
        StatBlock    stats;
        SocketLayout sockets;
    };

    // Convenience: ArchetypeTemplate + ResolveSockets in one call for the adapter.
    ReitemizedItem Reitemize(ItemChassis const& chassis, IReforgeConfig const& cfg);

    // Apply a reforge to `base`, returning the new block, or std::nullopt if the reforge is ILLEGAL:
    //   - to == from,
    //   - `to` is not an archetype-legal destination (IReforgeConfig::StatLegal),
    //   - `amount` is 0 or exceeds the points available in `from`,
    //   - `amount` exceeds floor(base.Get(from) * ReforgeMaxFraction())  — the bounded-fraction rule.
    // On success the block TOTAL is invariant: points are moved, never created, so the item's budget
    // (and thus its power level) is conserved. `base` is never mutated.
    std::optional<StatBlock> ApplyReforge(StatBlock const& base, Reforge const& reforge,
                                          ItemChassis const& chassis, IReforgeConfig const& cfg);
}

#endif // MOD_REFORGE_CORE_REFORGE_REITEMIZE_H
