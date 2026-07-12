#ifndef MOD_REFORGE_CORE_REFORGE_STATS_H
#define MOD_REFORGE_CORE_REFORGE_STATS_H

#include <array>
#include <cstddef>
#include <cstdint>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
namespace Reforge
{
    // Modern WotLK (3.3.5a) item stats. Order is stable (used as a persisted index and as the
    // StatBlock array index). Append new stats BEFORE COUNT to preserve stored data.
    enum class ItemStat : uint8_t
    {
        // Primary
        Stamina = 0,
        Strength,
        Agility,
        Intellect,
        Spirit,
        // Offense — power + ratings
        AttackPower,
        SpellPower,
        HitRating,
        CritRating,
        HasteRating,
        ExpertiseRating,
        ArmorPenRating,
        Mp5,
        // Defense
        DefenseRating,
        DodgeRating,
        ParryRating,
        BlockRating,
        Resilience,
        COUNT
    };

    // Itemization archetype: which modern stat template a chassis is rebuilt onto. The host adapter
    // derives it from role + armor class + slot; the engine treats it as an opaque selector into the
    // injected IReforgeConfig weight/legality tables. Append BEFORE COUNT.
    enum class ItemArchetype : uint8_t
    {
        PlateTank = 0,
        StrDps,        // plate/arms/2h strength melee
        AgiMeleeDps,   // leather/mail agility melee
        AgiRangedDps,  // hunter
        CasterDps,
        CasterHealer,
        COUNT
    };

    // Armor class of the item. Carried for the host's template/legality tables; the engine does not
    // interpret it directly (it flows through the injected config).
    enum class ArmorClass : uint8_t { None = 0, Cloth, Leather, Mail, Plate, COUNT };

    // Equip slot bucket. Carried for the host (weapon vs armor templates, budget scaling); the engine
    // does not interpret it directly. Minimal set — extend as needed.
    enum class EquipSlot : uint8_t
    {
        None = 0,
        Head, Neck, Shoulder, Chest, Wrist, Hands, Waist, Legs, Feet,
        Finger, Trinket, Cloak, Shield,
        OneHand, TwoHand, Ranged,
        COUNT
    };

    // A resolved stat composition: point value per ItemStat. "Points" are abstract budget units the
    // adapter maps to concrete item stats (§ adapter). Total() is the item's stat budget.
    struct StatBlock
    {
        std::array<uint32_t, static_cast<std::size_t>(ItemStat::COUNT)> points{};

        uint32_t Get(ItemStat stat) const { return points[static_cast<std::size_t>(stat)]; }
        void     Set(ItemStat stat, uint32_t value) { points[static_cast<std::size_t>(stat)] = value; }
        void     Add(ItemStat stat, uint32_t value) { points[static_cast<std::size_t>(stat)] += value; }

        uint32_t Total() const
        {
            uint32_t total = 0;
            for (uint32_t const p : points)
                total += p;
            return total;
        }

        bool operator==(StatBlock const& other) const { return points == other.points; }
    };

    // Gem socket colors (WotLK). Prismatic accepts a gem of any color — the least restrictive, and the
    // default for auto-granted sockets so a re-itemized item is never awkward to gem. Order is stable.
    enum class SocketColor : uint8_t { Prismatic = 0, Red, Yellow, Blue, Meta, COUNT };

    // Gem sockets granted to a re-itemized item: a count per color. Independent of the StatBlock —
    // gem power sits ON TOP of the stat budget and is bounded by IReforgeConfig::MaxSockets().
    struct SocketLayout
    {
        std::array<uint8_t, static_cast<std::size_t>(SocketColor::COUNT)> counts{};

        uint8_t Get(SocketColor color) const { return counts[static_cast<std::size_t>(color)]; }
        void    Set(SocketColor color, uint8_t value) { counts[static_cast<std::size_t>(color)] = value; }

        uint8_t Total() const
        {
            uint8_t total = 0;
            for (uint8_t const c : counts)
                total = static_cast<uint8_t>(total + c);
            return total;
        }

        bool operator==(SocketLayout const& other) const { return counts == other.counts; }
    };
}

#endif // MOD_REFORGE_CORE_REFORGE_STATS_H
