#include "ServerReforgeConfig.h"
#include "ReforgeStatMap.h"
#include "Configuration/Config.h"
#include <sstream>

namespace Reforge
{
    namespace
    {
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
    }
}
