#include "Reitemize.h"
#include <array>
#include <cstddef>

namespace Reforge
{
    StatBlock ArchetypeTemplate(ItemChassis const& chassis, IReforgeConfig const& cfg)
    {
        StatBlock out;

        constexpr std::size_t StatCount = static_cast<std::size_t>(ItemStat::COUNT);

        // Sum the template weights for this archetype.
        double totalWeight = 0.0;
        for (std::size_t i = 0; i < StatCount; ++i)
            totalWeight += cfg.StatWeight(chassis.archetype, static_cast<ItemStat>(i));

        if (totalWeight <= 0.0 || chassis.budget == 0)
            return out;   // zeroed: no template or no budget

        // Largest-remainder apportionment: floor each share, then hand the leftover points to the
        // largest fractional remainders. This spends the budget EXACTLY and DETERMINISTICALLY, and
        // never places a point on a zero-weight stat.
        std::array<double, StatCount> remainder{};
        std::array<double, StatCount> weight{};
        uint32_t assigned = 0;
        for (std::size_t i = 0; i < StatCount; ++i)
        {
            weight[i] = cfg.StatWeight(chassis.archetype, static_cast<ItemStat>(i));
            double const exact = static_cast<double>(chassis.budget) * weight[i] / totalWeight;
            uint32_t const floorPts = static_cast<uint32_t>(exact);   // exact >= 0, so truncation == floor
            out.points[i] = floorPts;
            remainder[i] = exact - static_cast<double>(floorPts);
            assigned += floorPts;
        }

        // At least one weight is > 0 (totalWeight > 0), so a recipient always exists.
        for (uint32_t leftover = chassis.budget - assigned; leftover > 0; --leftover)
        {
            std::size_t best = StatCount;
            for (std::size_t i = 0; i < StatCount; ++i)
            {
                if (weight[i] <= 0.0)
                    continue;
                if (best == StatCount || remainder[i] > remainder[best])
                    best = i;
            }

            out.points[best] += 1;
            remainder[best] -= 1.0;   // drop below the field so the next leftover picks another stat
        }

        return out;
    }

    SocketLayout ResolveSockets(ItemChassis const& chassis, IReforgeConfig const& cfg)
    {
        SocketLayout layout;

        uint8_t const requested = cfg.AutoSocketCount(chassis.slot, chassis.archetype);
        uint8_t const granted = requested < cfg.MaxSockets() ? requested : cfg.MaxSockets();
        if (granted > 0)
            layout.Set(cfg.AutoSocketColor(), granted);

        return layout;
    }

    ReitemizedItem Reitemize(ItemChassis const& chassis, IReforgeConfig const& cfg)
    {
        return { ArchetypeTemplate(chassis, cfg), ResolveSockets(chassis, cfg) };
    }

    std::optional<StatBlock> ApplyReforge(StatBlock const& base, ReforgeOp const& reforge,
                                          ItemChassis const& chassis, IReforgeConfig const& cfg)
    {
        if (reforge.to == reforge.from)
            return std::nullopt;

        if (!cfg.StatLegal(chassis.archetype, reforge.to))
            return std::nullopt;

        uint32_t const available = base.Get(reforge.from);
        if (reforge.amount == 0 || reforge.amount > available)
            return std::nullopt;

        // Bounded fraction: at most floor(available * MaxFraction) may move in one reforge.
        uint32_t const cap = static_cast<uint32_t>(static_cast<double>(available) * cfg.ReforgeMaxFraction());
        if (reforge.amount > cap)
            return std::nullopt;

        // Move the points — total is conserved, so the item's budget is unchanged.
        StatBlock out = base;
        out.Set(reforge.from, available - reforge.amount);
        out.Add(reforge.to, reforge.amount);
        return out;
    }
}
