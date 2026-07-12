#ifndef MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H
#define MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H

#include "reforge/ReforgeConfig.h"

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
    };
}

#endif // MOD_REFORGE_TESTS_FAKES_FAKEREFORGECONFIG_H
