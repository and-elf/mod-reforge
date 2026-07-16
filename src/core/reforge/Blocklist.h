#ifndef MOD_REFORGE_CORE_REFORGE_BLOCKLIST_H
#define MOD_REFORGE_CORE_REFORGE_BLOCKLIST_H

#include "Stats.h"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
//
// The item blocklist (ARCHITECTURE §13): decide whether an item may be reforged at all, from
// config-driven lists on three independent axes that combine with OR. POD in / POD out so the NPC,
// command and addon paths share ONE tested decision and the adapter stays a thin wire-up. Quality
// name -> ordinal parsing is the ONLY piece kept in the adapter (it needs AzerothCore's ItemQualities
// enum); this core's quality axis is purely numeric.
namespace Reforge
{
    // An item reduced to the facts the blocklist can match on. The adapter builds this from an
    // ItemTemplate (entry = ItemId, slot from InventoryType, armor from SubClass, quality from Quality).
    struct BlockKey
    {
        uint32_t   entry = 0;                      // item_template.entry
        EquipSlot  slot = EquipSlot::None;         // resolved equip slot
        ArmorClass armor = ArmorClass::None;       // armour material (None for weapons/jewellery)
        uint8_t    quality = 0;                    // WoW ItemQualities ordinal (0..7)
    };

    // The parsed blocklist config. Each list is one axis; an item is blocked if it matches ANY axis
    // (OR). All lists empty => nothing is ever blocked.
    struct BlockPolicy
    {
        std::vector<uint32_t>   itemIds;       // Reforge.Blocklist.ItemIds
        std::vector<EquipSlot>  slots;         // Reforge.Blocklist.Slots
        std::vector<ArmorClass> armorClasses;  // Reforge.Blocklist.ArmorClasses
        std::vector<uint8_t>    qualities;     // Reforge.Blocklist.Qualities (numeric ordinals)
    };

    // Whether `key` is blocked from reforging under `policy`. OR across the four axes: a match on the
    // entry id, the equip slot, the armour class, or the quality blocks the item. An empty policy
    // blocks nothing. EquipSlot::None / ArmorClass::None in the key match only if explicitly listed.
    bool IsBlocked(BlockKey const& key, BlockPolicy const& policy);

    // Tolerant CSV of item-template entry ids (mirrors Currency.cpp's parsing). Whitespace around
    // tokens is ignored; a token is DROPPED (never throws) when it is empty, non-numeric, or zero
    // (entry 0 is not a real item). Order is preserved.
    std::vector<uint32_t> ParseIdList(std::string_view spec);

    // Short, stable, lower-case canonical name for an equip slot ("head", "trinket", …). "?" for
    // None / COUNT / out of range.
    char const* SlotName(EquipSlot slot);

    // Parse a slot name (case-insensitive; accepts the SlotName tokens plus common aliases such as
    // "ring", "back", "gloves", "2h") to an EquipSlot, or nullopt. Also accepts the raw EquipSlot
    // ordinal as a decimal string.
    std::optional<EquipSlot> SlotFromName(std::string_view name);

    // Tolerant CSV of slot names -> EquipSlot. Unknown/empty tokens are dropped; order preserved.
    std::vector<EquipSlot> ParseSlotList(std::string_view spec);

    // Short, stable, lower-case canonical name for an armour class ("cloth", …). "?" for None /
    // COUNT / out of range.
    char const* ArmorClassName(ArmorClass armor);

    // Parse an armour-class name (case-insensitive; SlotName tokens: cloth/leather/mail/plate, plus
    // "none") to an ArmorClass, or nullopt. Also accepts the raw ArmorClass ordinal.
    std::optional<ArmorClass> ArmorClassFromName(std::string_view name);

    // Tolerant CSV of armour-class names -> ArmorClass. Unknown/empty tokens dropped; order preserved.
    std::vector<ArmorClass> ParseArmorClassList(std::string_view spec);
}

#endif // MOD_REFORGE_CORE_REFORGE_BLOCKLIST_H
