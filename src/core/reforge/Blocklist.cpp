#include "Blocklist.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>

namespace Reforge
{
    namespace
    {
        // Trim ASCII whitespace from both ends of a view.
        std::string_view Trim(std::string_view s)
        {
            std::size_t b = 0;
            while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
                ++b;
            std::size_t e = s.size();
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
                --e;
            return s.substr(b, e - b);
        }

        // Lower-case copy of a (trimmed) token, for case-insensitive name lookups.
        std::string LowerKey(std::string_view name)
        {
            name = Trim(name);
            std::string key;
            key.reserve(name.size());
            for (char const c : name)
                key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return key;
        }

        // Parse a non-negative decimal from `s` into `out`. Returns false on empty input or any
        // non-digit character (so a malformed field drops the whole token rather than half-parsing).
        bool ParseUint(std::string_view s, uint32_t& out)
        {
            s = Trim(s);
            if (s.empty())
                return false;

            uint64_t value = 0;
            for (char const c : s)
            {
                if (c < '0' || c > '9')
                    return false;
                value = value * 10 + static_cast<uint64_t>(c - '0');
                if (value > 0xFFFFFFFFull)
                    value = 0xFFFFFFFFull;
            }
            out = static_cast<uint32_t>(value);
            return true;
        }

        // Split a CSV spec and hand each trimmed, non-empty token to `sink`.
        template <typename Fn>
        void ForEachToken(std::string_view spec, Fn sink)
        {
            std::size_t pos = 0;
            while (pos <= spec.size())
            {
                std::size_t const comma = spec.find(',', pos);
                std::string_view const token = spec.substr(
                    pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);

                std::string_view const trimmed = Trim(token);
                if (!trimmed.empty())
                    sink(trimmed);

                if (comma == std::string_view::npos)
                    break;
                pos = comma + 1;
            }
        }
    }

    bool IsBlocked(BlockKey const& key, BlockPolicy const& policy)
    {
        // Axis 1: item entry id.
        if (std::find(policy.itemIds.begin(), policy.itemIds.end(), key.entry) != policy.itemIds.end())
            return true;

        // Axis 2: equip slot.
        if (std::find(policy.slots.begin(), policy.slots.end(), key.slot) != policy.slots.end())
            return true;

        // Axis 3a: armour class.
        if (std::find(policy.armorClasses.begin(), policy.armorClasses.end(), key.armor) != policy.armorClasses.end())
            return true;

        // Axis 3b: item quality.
        if (std::find(policy.qualities.begin(), policy.qualities.end(), key.quality) != policy.qualities.end())
            return true;

        return false;
    }

    std::vector<uint32_t> ParseIdList(std::string_view spec)
    {
        std::vector<uint32_t> out;
        ForEachToken(spec, [&out](std::string_view token)
        {
            uint32_t id = 0;
            if (ParseUint(token, id) && id != 0)   // entry 0 is not a real item
                out.push_back(id);
        });
        return out;
    }

    char const* SlotName(EquipSlot slot)
    {
        switch (slot)
        {
            case EquipSlot::Head:     return "head";
            case EquipSlot::Neck:     return "neck";
            case EquipSlot::Shoulder: return "shoulder";
            case EquipSlot::Chest:    return "chest";
            case EquipSlot::Wrist:    return "wrist";
            case EquipSlot::Hands:    return "hands";
            case EquipSlot::Waist:    return "waist";
            case EquipSlot::Legs:     return "legs";
            case EquipSlot::Feet:     return "feet";
            case EquipSlot::Finger:   return "finger";
            case EquipSlot::Trinket:  return "trinket";
            case EquipSlot::Cloak:    return "cloak";
            case EquipSlot::Shield:   return "shield";
            case EquipSlot::OneHand:  return "onehand";
            case EquipSlot::TwoHand:  return "twohand";
            case EquipSlot::Ranged:   return "ranged";
            default:                  return "?";
        }
    }

    std::optional<EquipSlot> SlotFromName(std::string_view name)
    {
        std::string key = LowerKey(name);
        if (key.empty())
            return std::nullopt;

        // Aliases fold onto the canonical SlotName tokens.
        if (key == "shoulders")            key = "shoulder";
        if (key == "wrists" || key == "bracers") key = "wrist";
        if (key == "gloves")               key = "hands";
        if (key == "belt")                 key = "waist";
        if (key == "boots")                key = "feet";
        if (key == "ring")                 key = "finger";
        if (key == "back" || key == "cape") key = "cloak";
        if (key == "1h" || key == "weapon" || key == "mainhand" || key == "offhand" || key == "holdable")
            key = "onehand";
        if (key == "2h")                   key = "twohand";
        if (key == "thrown")               key = "ranged";

        for (std::size_t i = 0; i < static_cast<std::size_t>(EquipSlot::COUNT); ++i)
        {
            EquipSlot const slot = static_cast<EquipSlot>(i);
            if (key == SlotName(slot))
                return slot;
        }

        // Also accept a raw EquipSlot ordinal.
        uint32_t ordinal = 0;
        auto const [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), ordinal);
        if (ec == std::errc() && ptr == key.data() + key.size() && ordinal < static_cast<uint32_t>(EquipSlot::COUNT))
            return static_cast<EquipSlot>(ordinal);

        return std::nullopt;
    }

    std::vector<EquipSlot> ParseSlotList(std::string_view spec)
    {
        std::vector<EquipSlot> out;
        ForEachToken(spec, [&out](std::string_view token)
        {
            if (std::optional<EquipSlot> const slot = SlotFromName(token))
                out.push_back(*slot);
        });
        return out;
    }

    char const* ArmorClassName(ArmorClass armor)
    {
        switch (armor)
        {
            case ArmorClass::Cloth:   return "cloth";
            case ArmorClass::Leather: return "leather";
            case ArmorClass::Mail:    return "mail";
            case ArmorClass::Plate:   return "plate";
            case ArmorClass::None:    return "none";
            default:                  return "?";
        }
    }

    std::optional<ArmorClass> ArmorClassFromName(std::string_view name)
    {
        std::string const key = LowerKey(name);
        if (key.empty())
            return std::nullopt;

        for (std::size_t i = 0; i < static_cast<std::size_t>(ArmorClass::COUNT); ++i)
        {
            ArmorClass const armor = static_cast<ArmorClass>(i);
            if (key == ArmorClassName(armor))
                return armor;
        }

        // Also accept a raw ArmorClass ordinal.
        uint32_t ordinal = 0;
        auto const [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), ordinal);
        if (ec == std::errc() && ptr == key.data() + key.size() && ordinal < static_cast<uint32_t>(ArmorClass::COUNT))
            return static_cast<ArmorClass>(ordinal);

        return std::nullopt;
    }

    std::vector<ArmorClass> ParseArmorClassList(std::string_view spec)
    {
        std::vector<ArmorClass> out;
        ForEachToken(spec, [&out](std::string_view token)
        {
            if (std::optional<ArmorClass> const armor = ArmorClassFromName(token))
                out.push_back(*armor);
        });
        return out;
    }
}
