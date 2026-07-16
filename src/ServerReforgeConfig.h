#ifndef MOD_REFORGE_SRC_SERVERREFORGECONFIG_H
#define MOD_REFORGE_SRC_SERVERREFORGECONFIG_H

#include "reforge/Currency.h"
#include "reforge/ReforgeConfig.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Reforge
{
    // Production IReforgeConfig: snapshots every Reforge.* option from sConfigMgr at load and on
    // `.reload config`. The pure core reads no globals; this is the only place sConfigMgr is touched
    // for reforge tunables. Adapter-only getters (enabled, NPC entry, currencies, addon, enchant base)
    // live here too, alongside the IReforgeConfig contract the core depends on.
    //
    // On a bare server the reforge engine only rebalances an item's OWN stats, so StatWeight (used by
    // re-itemisation) returns 0 and StatLegal ignores the archetype — legality is a fixed, config-
    // tunable set of reforge-eligible stats (retail's secondary ratings). A host module can subclass /
    // replace this with archetype-aware policy.
    class ServerReforgeConfig : public IReforgeConfig
    {
    public:
        void Load();

        // --- IReforgeConfig (consumed by the pure core) ---
        double ReforgeMaxFraction() const override { return _maxFraction; }
        double StatWeight(ItemArchetype /*archetype*/, ItemStat /*stat*/) const override { return 0.0; }
        bool StatLegal(ItemArchetype /*archetype*/, ItemStat stat) const override { return IsLegalStat(stat); }
        uint8_t AutoSocketCount(EquipSlot /*slot*/, ItemArchetype /*archetype*/) const override { return 0; }
        SocketColor AutoSocketColor() const override { return SocketColor::Prismatic; }
        uint8_t MaxSockets() const override { return 3; }

        // Weapon-damage scaling (issue #7): geometric per-level curve, clamped. 1.0 when disabled or
        // fromLevel == toLevel (or fromLevel == 0). Reads Reforge.WeaponScale.* snapshotted in Load().
        double WeaponDamageScale(uint32_t fromLevel, uint32_t toLevel) const override;

        // --- Adapter-only accessors ---
        bool Enabled() const { return _enabled; }
        bool WeaponScaleEnabled() const { return _weaponScaleEnabled; }
        bool AddonEnabled() const { return _addonEnabled; }
        bool RequireNpc() const { return _requireNpc; }
        float NpcRange() const { return _npcRange; }
        uint32_t NpcEntry() const { return _npcEntry; }
        uint32_t EnchantBase() const { return _enchantBase; }

        // Whether `stat` may be a reforge source/destination (the fixed, config-tunable eligible set).
        bool IsLegalStat(ItemStat stat) const;

        // Accepted ways to pay a reforge, in configured order (entry 0 = gold). See §10.
        std::vector<CurrencyCost> const& AcceptedCurrencies() const { return _currencies; }

    private:
        bool _enabled = false;
        bool _addonEnabled = false;
        bool _requireNpc = true;
        float _npcRange = 10.0f;
        double _maxFraction = 0.40;
        uint32_t _npcEntry = 900100;
        uint32_t _enchantBase = 900200;   // REFORGE_ENCHANT_BASE; +ItemModType per destination stat
        bool _weaponScaleEnabled = true;  // Reforge.WeaponScale.* (issue #7)
        double _weaponScalePerLevel = 0.03;
        double _weaponScaleMinFactor = 0.1;
        double _weaponScaleMaxFactor = 10.0;
        std::array<bool, static_cast<std::size_t>(ItemStat::COUNT)> _legal{};
        std::vector<CurrencyCost> _currencies;
    };
}

#endif // MOD_REFORGE_SRC_SERVERREFORGECONFIG_H
