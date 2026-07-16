#include "BudgetScale.h"
#include <array>
#include <cmath>
#include <cstddef>

namespace Reforge
{
    uint32_t TargetBudget(uint32_t level, uint8_t quality, IReforgeConfig const& cfg)
    {
        double const points = cfg.LevelBudgetPoints(level) * cfg.QualityBudgetMultiplier(quality);
        if (points <= 0.0)
            return 0;
        return static_cast<uint32_t>(std::llround(points));
    }

    uint32_t ScaleFactorPermille(uint32_t sourceBudget, uint32_t targetBudget)
    {
        if (sourceBudget == 0)
            return 1000;   // nothing to scale proportionally — identity

        // round(targetBudget * 1000 / sourceBudget), in 64-bit to avoid overflow.
        uint64_t const numerator = static_cast<uint64_t>(targetBudget) * 1000u + sourceBudget / 2u;
        return static_cast<uint32_t>(numerator / sourceBudget);
    }

    bool DownscalePermitted(bool isDownscale, bool isTrinket, bool hasEffect, bool allowDownscale)
    {
        if (!isDownscale)
            return true;   // up-scaling is always allowed, even for trinkets / effect items
        return allowDownscale && !isTrinket && !hasEffect;
    }

    uint32_t ClampScalePermille(uint32_t rawPermille, bool isTrinket, bool hasEffect, bool allowDownscale)
    {
        bool const isDownscale = rawPermille < 1000;
        if (!DownscalePermitted(isDownscale, isTrinket, hasEffect, allowDownscale))
            return 1000;   // blocked down-scale -> keep native budget (identity)
        return rawPermille;
    }

    StatBlock ScaleStatBlock(StatBlock const& base, uint32_t targetBudget)
    {
        StatBlock out;

        constexpr std::size_t StatCount = static_cast<std::size_t>(ItemStat::COUNT);

        uint32_t const total = base.Total();
        if (total == 0 || targetBudget == 0)
            return out;   // zeroed: nothing to distribute, or nothing to distribute onto

        // Largest-remainder apportionment (identical to ArchetypeTemplate): floor each proportional
        // share, then hand the leftover points to the largest fractional remainders. Spends the target
        // budget EXACTLY and DETERMINISTICALLY, and only ever places points on stats that already had
        // points (a zero source stat has zero share, so it stays zero).
        std::array<double, StatCount> remainder{};
        uint32_t assigned = 0;
        for (std::size_t i = 0; i < StatCount; ++i)
        {
            double const exact = static_cast<double>(targetBudget) * static_cast<double>(base.points[i])
                / static_cast<double>(total);
            uint32_t const floorPts = static_cast<uint32_t>(exact);   // exact >= 0, so truncation == floor
            out.points[i] = floorPts;
            remainder[i] = exact - static_cast<double>(floorPts);
            assigned += floorPts;
        }

        // At least one source stat is > 0 (total > 0), so a recipient always exists.
        for (uint32_t leftover = targetBudget - assigned; leftover > 0; --leftover)
        {
            std::size_t best = StatCount;
            for (std::size_t i = 0; i < StatCount; ++i)
            {
                if (base.points[i] == 0)
                    continue;
                if (best == StatCount || remainder[i] > remainder[best])
                    best = i;
            }

            out.points[best] += 1;
            remainder[best] -= 1.0;   // drop below the field so the next leftover picks another stat
        }

        return out;
    }
}
