#include "ServerReforgeConfig.h"
#include "ReforgeStatMap.h"
#include "Configuration/Config.h"
#include "ItemTemplate.h"
#include "SharedDefines.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace Reforge
{
    namespace
    {
        // Quality name -> WoW ItemQualities ordinal. This is the ONE place the AzerothCore ItemQualities
        // enum is used for the blocklist (the pure core's quality axis is purely numeric — §12). Returns
        // nullopt for an unknown name; also accepts a raw numeric ordinal.
        std::optional<uint8_t> QualityFromName(std::string_view raw)
        {
            std::string key;
            key.reserve(raw.size());
            for (char const c : raw)
                if (!std::isspace(static_cast<unsigned char>(c)))
                    key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (key.empty())
                return std::nullopt;

            if (key == "poor" || key == "grey" || key == "gray")   return ITEM_QUALITY_POOR;
            if (key == "common" || key == "normal" || key == "white") return ITEM_QUALITY_NORMAL;
            if (key == "uncommon" || key == "green")               return ITEM_QUALITY_UNCOMMON;
            if (key == "rare" || key == "blue")                    return ITEM_QUALITY_RARE;
            if (key == "epic" || key == "purple")                  return ITEM_QUALITY_EPIC;
            if (key == "legendary" || key == "orange")             return ITEM_QUALITY_LEGENDARY;
            if (key == "artifact")                                 return ITEM_QUALITY_ARTIFACT;
            if (key == "heirloom")                                 return ITEM_QUALITY_HEIRLOOM;

            // Also accept a raw numeric ordinal (0..7).
            if (key.find_first_not_of("0123456789") == std::string::npos)
            {
                unsigned long const value = std::stoul(key);
                if (value <= ITEM_QUALITY_HEIRLOOM)
                    return static_cast<uint8_t>(value);
            }
            return std::nullopt;
        }

        // The default reforge-eligible set: retail's secondary combat ratings (source AND destination).
        // Primaries (str/agi/sta/int) and spellpower/attackpower are excluded so a reforge only ever
        // rebalances "spare" secondaries, never core power.
        constexpr char const* DefaultLegalStats =
            "hit,crit,haste,expertise,armorpen,spirit,dodge,parry,block,resilience,defense,mp5";

        constexpr char const* DefaultCurrencies = "0:100000";   // 10 gold
    }

    bool ServerReforgeConfig::IsLegalStat(ItemStat stat) const
    {
        std::size_t const idx = static_cast<std::size_t>(stat);
        return idx < _legal.size() && _legal[idx];
    }

    double ServerReforgeConfig::WeaponDamageScale(uint32_t fromLevel, uint32_t toLevel) const
    {
        // Identity when disabled, when the source level is unknown, or when there is no transition.
        if (!_weaponScaleEnabled || fromLevel == 0 || fromLevel == toLevel)
            return 1.0;

        // Geometric per-level curve: weapon DPS grows roughly multiplicatively with level (§12).
        int32_t const delta = static_cast<int32_t>(toLevel) - static_cast<int32_t>(fromLevel);
        double const factor = std::pow(1.0 + _weaponScalePerLevel, static_cast<double>(delta));
        return std::clamp(factor, _weaponScaleMinFactor, _weaponScaleMaxFactor);
    }

    bool ServerReforgeConfig::IsItemBlocked(ItemTemplate const* proto) const
    {
        if (!proto)
            return false;

        BlockKey key;
        key.entry = proto->ItemId;
        key.slot = SlotFromInventoryType(proto->InventoryType);
        key.armor = BuildChassis(proto).armor;
        key.quality = static_cast<uint8_t>(proto->Quality);
        return IsBlocked(key, _blockPolicy);
    }

    void ServerReforgeConfig::Load()
    {
        _enabled = sConfigMgr->GetOption<bool>("Reforge.Enable", true);
        _addonEnabled = sConfigMgr->GetOption<bool>("Reforge.Addon.Enable", true);
        _requireNpc = sConfigMgr->GetOption<bool>("Reforge.RequireNpc", true);
        _npcRange = sConfigMgr->GetOption<float>("Reforge.NpcRange", 10.0f);
        _npcEntry = sConfigMgr->GetOption<uint32_t>("Reforge.NpcEntry", 900100);
        _enchantBase = sConfigMgr->GetOption<uint32_t>("Reforge.EnchantBase", 900200);

        // GetOption is explicitly instantiated in core only for float (not double); using double here
        // links fine statically but leaves an undefined symbol in a dynamic module .so.
        double const fraction = sConfigMgr->GetOption<float>("Reforge.MaxFraction", 0.40f);
        _maxFraction = (fraction > 0.0 && fraction <= 1.0) ? fraction : 0.40;

        // Weapon-damage scaling (issue #7). GetOption is instantiated in core only for float (not
        // double) -- using double leaves an undefined symbol in the dynamic module .so (see MaxFraction).
        _weaponScaleEnabled = sConfigMgr->GetOption<bool>("Reforge.WeaponScale.Enable", true);
        float const perLevelPct = sConfigMgr->GetOption<float>("Reforge.WeaponScale.PerLevelPct", 3.0f);
        _weaponScalePerLevel = perLevelPct / 100.0;
        float const minFactor = sConfigMgr->GetOption<float>("Reforge.WeaponScale.MinFactor", 0.1f);
        float const maxFactor = sConfigMgr->GetOption<float>("Reforge.WeaponScale.MaxFactor", 10.0f);
        _weaponScaleMinFactor = (minFactor > 0.0f) ? minFactor : 0.1;
        _weaponScaleMaxFactor = (maxFactor >= _weaponScaleMinFactor) ? maxFactor : _weaponScaleMinFactor;

        // Legal reforge stats (source + destination), CSV of stat names.
        _legal.fill(false);
        std::string const legalRaw = sConfigMgr->GetOption<std::string>("Reforge.LegalStats", DefaultLegalStats);
        std::stringstream ss(legalRaw);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            if (std::optional<ItemStat> const stat = StatFromName(token))
                _legal[static_cast<std::size_t>(*stat)] = true;
        }

        // Accepted currencies (§10): "entry:count, entry:count, …" (entry 0 = gold copper).
        std::string const currencyRaw = sConfigMgr->GetOption<std::string>("Reforge.Cost.Currencies", DefaultCurrencies);
        _currencies = ParseCurrencyCosts(currencyRaw);
        if (_currencies.empty())
            _currencies = ParseCurrencyCosts(DefaultCurrencies);   // never leave the player unable to pay

        // Blocklist (§13): three OR-combined axes, all empty by default so nothing is blocked. Item
        // ids, slots and armour classes parse in the pure core; quality names resolve here (ItemQualities).
        _blockPolicy = BlockPolicy{};
        _blockPolicy.itemIds = ParseIdList(sConfigMgr->GetOption<std::string>("Reforge.Blocklist.ItemIds", ""));
        _blockPolicy.slots = ParseSlotList(sConfigMgr->GetOption<std::string>("Reforge.Blocklist.Slots", ""));
        std::string const armorRaw = sConfigMgr->GetOption<std::string>("Reforge.Blocklist.ArmorClasses", "");
        _blockPolicy.armorClasses = ParseArmorClassList(armorRaw);

        std::string const qualityRaw = sConfigMgr->GetOption<std::string>("Reforge.Blocklist.Qualities", "");
        std::stringstream qss(qualityRaw);
        std::string qtoken;
        while (std::getline(qss, qtoken, ','))
            if (std::optional<uint8_t> const quality = QualityFromName(qtoken))
                _blockPolicy.qualities.push_back(*quality);
    }
}
