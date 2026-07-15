#include "ReforgeStatMap.h"
#include "ItemTemplate.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string>

namespace Reforge
{
    uint32_t ToItemModType(ItemStat stat)
    {
        switch (stat)
        {
            case ItemStat::Stamina:         return ITEM_MOD_STAMINA;
            case ItemStat::Strength:        return ITEM_MOD_STRENGTH;
            case ItemStat::Agility:         return ITEM_MOD_AGILITY;
            case ItemStat::Intellect:       return ITEM_MOD_INTELLECT;
            case ItemStat::Spirit:          return ITEM_MOD_SPIRIT;
            case ItemStat::AttackPower:     return ITEM_MOD_ATTACK_POWER;
            case ItemStat::SpellPower:      return ITEM_MOD_SPELL_POWER;
            case ItemStat::HitRating:       return ITEM_MOD_HIT_RATING;
            case ItemStat::CritRating:      return ITEM_MOD_CRIT_RATING;
            case ItemStat::HasteRating:     return ITEM_MOD_HASTE_RATING;
            case ItemStat::ExpertiseRating: return ITEM_MOD_EXPERTISE_RATING;
            case ItemStat::ArmorPenRating:  return ITEM_MOD_ARMOR_PENETRATION_RATING;
            case ItemStat::Mp5:             return ITEM_MOD_MANA_REGENERATION;
            case ItemStat::DefenseRating:   return ITEM_MOD_DEFENSE_SKILL_RATING;
            case ItemStat::DodgeRating:     return ITEM_MOD_DODGE_RATING;
            case ItemStat::ParryRating:     return ITEM_MOD_PARRY_RATING;
            case ItemStat::BlockRating:     return ITEM_MOD_BLOCK_RATING;
            case ItemStat::Resilience:      return ITEM_MOD_RESILIENCE_RATING;
            default:                        return 0;
        }
    }

    std::optional<ItemStat> FromItemModType(uint32_t mod)
    {
        switch (mod)
        {
            case ITEM_MOD_STAMINA:                  return ItemStat::Stamina;
            case ITEM_MOD_STRENGTH:                 return ItemStat::Strength;
            case ITEM_MOD_AGILITY:                  return ItemStat::Agility;
            case ITEM_MOD_INTELLECT:                return ItemStat::Intellect;
            case ITEM_MOD_SPIRIT:                   return ItemStat::Spirit;
            case ITEM_MOD_ATTACK_POWER:             return ItemStat::AttackPower;
            case ITEM_MOD_SPELL_POWER:              return ItemStat::SpellPower;
            case ITEM_MOD_HIT_RATING:               return ItemStat::HitRating;
            case ITEM_MOD_CRIT_RATING:              return ItemStat::CritRating;
            case ITEM_MOD_HASTE_RATING:             return ItemStat::HasteRating;
            case ITEM_MOD_EXPERTISE_RATING:         return ItemStat::ExpertiseRating;
            case ITEM_MOD_ARMOR_PENETRATION_RATING: return ItemStat::ArmorPenRating;
            case ITEM_MOD_MANA_REGENERATION:        return ItemStat::Mp5;
            case ITEM_MOD_DEFENSE_SKILL_RATING:     return ItemStat::DefenseRating;
            case ITEM_MOD_DODGE_RATING:             return ItemStat::DodgeRating;
            case ITEM_MOD_PARRY_RATING:             return ItemStat::ParryRating;
            case ITEM_MOD_BLOCK_RATING:             return ItemStat::BlockRating;
            case ITEM_MOD_RESILIENCE_RATING:        return ItemStat::Resilience;
            default:                                return std::nullopt;
        }
    }

    StatBlock BuildStatBlock(ItemTemplate const* proto)
    {
        StatBlock block;
        if (!proto)
            return block;

        uint32_t const count = std::min<uint32_t>(proto->StatsCount, MAX_ITEM_PROTO_STATS);
        for (uint32_t i = 0; i < count; ++i)
        {
            _ItemStat const& stat = proto->ItemStat[i];
            if (stat.ItemStatValue <= 0)
                continue;
            if (std::optional<ItemStat> const mapped = FromItemModType(stat.ItemStatType))
                block.Add(*mapped, static_cast<uint32_t>(stat.ItemStatValue));
        }
        return block;
    }

    ItemChassis BuildChassis(ItemTemplate const* proto)
    {
        ItemChassis chassis;
        if (!proto)
            return chassis;

        chassis.budget = BuildStatBlock(proto).Total();

        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_ARMOR_CLOTH:   chassis.armor = ArmorClass::Cloth;   break;
            case ITEM_SUBCLASS_ARMOR_LEATHER: chassis.armor = ArmorClass::Leather; break;
            case ITEM_SUBCLASS_ARMOR_MAIL:    chassis.armor = ArmorClass::Mail;    break;
            case ITEM_SUBCLASS_ARMOR_PLATE:   chassis.armor = ArmorClass::Plate;   break;
            default:                          chassis.armor = ArmorClass::None;    break;
        }

        // Archetype is not used for legality on a bare server (ServerReforgeConfig ignores it); leave
        // the default. A host module can re-derive it when it drives re-itemisation.
        chassis.archetype = ItemArchetype::CasterDps;
        return chassis;
    }

    char const* StatName(ItemStat stat)
    {
        switch (stat)
        {
            case ItemStat::Stamina:         return "stamina";
            case ItemStat::Strength:        return "strength";
            case ItemStat::Agility:         return "agility";
            case ItemStat::Intellect:       return "intellect";
            case ItemStat::Spirit:          return "spirit";
            case ItemStat::AttackPower:     return "attackpower";
            case ItemStat::SpellPower:      return "spellpower";
            case ItemStat::HitRating:       return "hit";
            case ItemStat::CritRating:      return "crit";
            case ItemStat::HasteRating:     return "haste";
            case ItemStat::ExpertiseRating: return "expertise";
            case ItemStat::ArmorPenRating:  return "armorpen";
            case ItemStat::Mp5:             return "mp5";
            case ItemStat::DefenseRating:   return "defense";
            case ItemStat::DodgeRating:     return "dodge";
            case ItemStat::ParryRating:     return "parry";
            case ItemStat::BlockRating:     return "block";
            case ItemStat::Resilience:      return "resilience";
            default:                        return "?";
        }
    }

    std::optional<ItemStat> StatFromName(std::string_view name)
    {
        // Trim surrounding whitespace (config CSVs and command args may pad tokens).
        std::size_t b = 0;
        while (b < name.size() && std::isspace(static_cast<unsigned char>(name[b])))
            ++b;
        std::size_t e = name.size();
        while (e > b && std::isspace(static_cast<unsigned char>(name[e - 1])))
            --e;
        name = name.substr(b, e - b);

        std::string key;
        key.reserve(name.size());
        for (char const c : name)
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        // Aliases fold onto the canonical StatName tokens.
        if (key == "sta")  key = "stamina";
        if (key == "str")  key = "strength";
        if (key == "agi")  key = "agility";
        if (key == "int")  key = "intellect";
        if (key == "spi")  key = "spirit";
        if (key == "ap")   key = "attackpower";
        if (key == "sp")   key = "spellpower";
        if (key == "exp")  key = "expertise";
        if (key == "arp")  key = "armorpen";
        if (key == "resil") key = "resilience";

        for (std::size_t i = 0; i < static_cast<std::size_t>(ItemStat::COUNT); ++i)
        {
            ItemStat const stat = static_cast<ItemStat>(i);
            if (key == StatName(stat))
                return stat;
        }

        // Also accept a raw ItemStat ordinal.
        uint32_t ordinal = 0;
        auto const [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), ordinal);
        if (ec == std::errc() && ptr == key.data() + key.size() && ordinal < static_cast<uint32_t>(ItemStat::COUNT))
            return static_cast<ItemStat>(ordinal);

        return std::nullopt;
    }
}
