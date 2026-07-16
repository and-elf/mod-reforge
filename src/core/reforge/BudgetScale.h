#ifndef MOD_REFORGE_CORE_REFORGE_BUDGETSCALE_H
#define MOD_REFORGE_CORE_REFORGE_BUDGETSCALE_H

#include "ReforgeConfig.h"
#include "Stats.h"
#include <cstdint>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
//
// Level/rarity budget scaling (ARCHITECTURE §14, issues #8/#6). A reforge re-itemises an item's stat
// BUDGET to the reforging player's CURRENT level: the target budget is a function of level + quality
// ONLY (independent of the item's native budget), so every item normalises to the player's level. The
// stat-move reforge (§5) then composes on top of the scaled budget.
//
// Everything here is POD in / POD out and deterministic so the NPC, command and addon paths — and the
// live stat vehicle — all share ONE tested implementation.
namespace Reforge
{
    // The target stat budget for a player of `level` holding an item of WotLK `quality` (0..6):
    // round(cfg.LevelBudgetPoints(level) * cfg.QualityBudgetMultiplier(quality)), clamped to >= 0.
    // Independent of the item's native budget — the whole point is to normalise to the player's level.
    uint32_t TargetBudget(uint32_t level, uint8_t quality, IReforgeConfig const& cfg);

    // The per-stat multiplier the live stat vehicle applies, expressed in PERMILLE (1000 == identity):
    // round(targetBudget * 1000 / sourceBudget). A zero source budget yields 1000 (nothing to scale).
    uint32_t ScaleFactorPermille(uint32_t sourceBudget, uint32_t targetBudget);

    // Whether a scale may be applied given the twink-protection rules. An UP-scale (!isDownscale) is
    // always permitted. A DOWN-scale is permitted only when downscaling is globally allowed AND the
    // item is neither a trinket nor carries an on-use/on-equip effect. POD in / bool out.
    bool DownscalePermitted(bool isDownscale, bool isTrinket, bool hasEffect, bool allowDownscale);

    // Clamp a raw scale (permille) through the down-scale guard: if `rawPermille` is a down-scale
    // (< 1000) that DownscalePermitted rejects, return 1000 (identity — keep the native budget);
    // otherwise return `rawPermille` unchanged. Up-scales pass through untouched.
    uint32_t ClampScalePermille(uint32_t rawPermille, bool isTrinket, bool hasEffect, bool allowDownscale);

    // Scale `base` proportionally so its Total() lands EXACTLY on `targetBudget`, using the same
    // largest-remainder apportionment as ArchetypeTemplate (deterministic; points land only on stats
    // that already carry points). `base.Total() == 0` or `targetBudget == 0` yields a zeroed block.
    // Used for the player-facing reforge math (cap, legality, displayed amounts).
    StatBlock ScaleStatBlock(StatBlock const& base, uint32_t targetBudget);
}

#endif // MOD_REFORGE_CORE_REFORGE_BUDGETSCALE_H
