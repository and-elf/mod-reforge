#ifndef MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H
#define MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H

#include "reforge/ReforgeConfig.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Reforge::Test
{
    // Deterministic test double. Defines concrete templates for two archetypes (CasterDps, PlateTank);
    // every other archetype has an empty template (all weights 0) so the "empty => zeroed" path is
    // testable. Legal reforge destinations = the template stats plus a couple archetype-appropriate
    // extras, so "reforge into a template-omitted but legal stat" is exercised.
    class FakeReforgeConfig : public IReforgeConfig
    {
    public:
        double reforgeMaxFraction = 0.40;

        double ReforgeMaxFraction() const override { return reforgeMaxFraction; }

        double StatWeight(ItemArchetype archetype, ItemStat stat) const override
        {
            switch (archetype)
            {
            case ItemArchetype::CasterDps:
                switch (stat)
                {
                case ItemStat::SpellPower:  return 4.0;
                case ItemStat::Intellect:   return 3.0;
                case ItemStat::HitRating:   return 1.0;
                case ItemStat::CritRating:  return 1.0;
                case ItemStat::HasteRating: return 1.0;
                default:                    return 0.0;
                }
            case ItemArchetype::PlateTank:
                switch (stat)
                {
                case ItemStat::Stamina:       return 4.0;
                case ItemStat::DefenseRating: return 2.0;
                case ItemStat::Strength:      return 1.0;
                case ItemStat::DodgeRating:   return 1.0;
                case ItemStat::ParryRating:   return 1.0;
                default:                      return 0.0;
                }
            default:
                return 0.0;
            }
        }

        bool StatLegal(ItemArchetype archetype, ItemStat stat) const override
        {
            if (StatWeight(archetype, stat) > 0.0)
                return true;

            switch (archetype)
            {
            case ItemArchetype::CasterDps:
                return stat == ItemStat::Spirit || stat == ItemStat::Mp5;
            case ItemArchetype::PlateTank:
                return stat == ItemStat::BlockRating || stat == ItemStat::Resilience;
            default:
                return false;
            }
        }

        // Socket policy: Chest gets 2 auto sockets, other real slots 1, the None slot 0; capped at 3.
        uint8_t maxSockets = 3;

        uint8_t AutoSocketCount(EquipSlot slot, ItemArchetype /*archetype*/) const override
        {
            switch (slot)
            {
            case EquipSlot::None:  return 0;
            case EquipSlot::Chest: return 2;
            default:               return 1;
            }
        }

        SocketColor AutoSocketColor() const override { return SocketColor::Prismatic; }
        uint8_t MaxSockets() const override { return maxSockets; }

        // Weapon-damage scaling policy (issue #7). Either a forced constant factor (`forcedWeaponScale`,
        // used to drive the core's degenerate branches -- zero/negative factor), or a geometric per-level
        // curve that returns exactly 1.0 for equal levels or when disabled. Tunable so a test can prove
        // the factor genuinely drives the output.
        bool weaponScaleEnabled = true;
        double weaponScalePerLevel = 0.03;          // 3% per level of the from->to delta
        std::optional<double> forcedWeaponScale;    // when set, overrides the curve verbatim

        double WeaponDamageScale(uint32_t fromLevel, uint32_t toLevel) const override
        {
            if (forcedWeaponScale)
                return *forcedWeaponScale;
            if (!weaponScaleEnabled || fromLevel == toLevel)
                return 1.0;
            int const delta = static_cast<int>(toLevel) - static_cast<int>(fromLevel);
            return std::pow(1.0 + weaponScalePerLevel, delta);
        }

        // --- Level/rarity budget scaling (§13) ---
        // A simple, predictable linear level curve and a per-quality multiplier table (all 1.0 by
        // default so a test can isolate one quality), plus the global down-scale switch.
        double levelBudgetBase = 0.0;
        double levelBudgetPerLevel = 10.0;
        std::array<double, 7> qualityMult{ 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
        bool allowDownscale = true;

        double LevelBudgetPoints(uint32_t level) const override
        {
            return levelBudgetBase + levelBudgetPerLevel * static_cast<double>(level);
        }

        double QualityBudgetMultiplier(uint8_t quality) const override
        {
            return quality < qualityMult.size() ? qualityMult[quality] : 1.0;
        }

        bool AllowDownscale() const override { return allowDownscale; }
    };
}

#endif // MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H
