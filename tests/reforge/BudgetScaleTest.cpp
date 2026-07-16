#include "reforge/BudgetScale.h"
#include "fakes/FakeReforgeConfig.h"
#include <gtest/gtest.h>

using namespace Reforge;
using namespace Reforge::Test;

// --- TargetBudget: level curve * quality multiplier, independent of the item's native budget ---

// The default fake curve is 10 points/level, quality multiplier 1.0.
TEST(TargetBudget, LevelCurveTimesQuality)
{
    FakeReforgeConfig cfg;
    EXPECT_EQ(TargetBudget(1, 4, cfg), 10u);
    EXPECT_EQ(TargetBudget(20, 4, cfg), 200u);
    EXPECT_EQ(TargetBudget(80, 4, cfg), 800u);
}

// The per-quality multiplier scales the budget (epic here worth 1.5x a baseline quality).
TEST(TargetBudget, QualityMultiplierApplies)
{
    FakeReforgeConfig cfg;
    cfg.qualityMult[4] = 1.5;   // epic
    cfg.qualityMult[2] = 0.5;   // uncommon
    EXPECT_EQ(TargetBudget(80, 4, cfg), 1200u);   // 800 * 1.5
    EXPECT_EQ(TargetBudget(80, 2, cfg), 400u);    // 800 * 0.5
}

// Rounding is to the nearest integer, not truncation.
TEST(TargetBudget, RoundsToNearest)
{
    FakeReforgeConfig cfg;
    cfg.qualityMult[4] = 1.0;
    cfg.levelBudgetPerLevel = 10.0;
    cfg.levelBudgetBase = 0.5;                    // 10*level + 0.5 -> .5 rounds up
    EXPECT_EQ(TargetBudget(10, 4, cfg), 101u);    // round(100.5) = 101
}

// Level 0 with a zero base is a zero budget (degenerate but well-defined).
TEST(TargetBudget, ZeroLevelZeroBaseIsZero)
{
    FakeReforgeConfig cfg;
    EXPECT_EQ(TargetBudget(0, 4, cfg), 0u);
}

// An out-of-range quality index falls back to the 1.0 multiplier (no crash / overflow).
TEST(TargetBudget, OutOfRangeQualityIsSafe)
{
    FakeReforgeConfig cfg;
    EXPECT_EQ(TargetBudget(10, 200, cfg), 100u);
}

// --- ScaleFactorPermille: the per-stat multiplier the live vehicle applies (1000 = identity) ---

TEST(ScaleFactorPermille, IdentityUpscaleDownscale)
{
    EXPECT_EQ(ScaleFactorPermille(200, 200), 1000u);   // identity
    EXPECT_EQ(ScaleFactorPermille(200, 400), 2000u);   // 2x up
    EXPECT_EQ(ScaleFactorPermille(200, 100), 500u);    // 0.5x down
    EXPECT_EQ(ScaleFactorPermille(200, 0), 0u);        // scaled to nothing
}

// A zero source budget cannot be scaled proportionally: identity (1000), never a divide-by-zero.
TEST(ScaleFactorPermille, ZeroSourceIsIdentity)
{
    EXPECT_EQ(ScaleFactorPermille(0, 500), 1000u);
    EXPECT_EQ(ScaleFactorPermille(0, 0), 1000u);
}

// Permille rounds to nearest.
TEST(ScaleFactorPermille, RoundsToNearest)
{
    // 100 -> 133 : 1330/1000 = 1.33 exactly => 1330
    EXPECT_EQ(ScaleFactorPermille(100, 133), 1330u);
    // 3 -> 1 : 1000/3 = 333.33 -> 333
    EXPECT_EQ(ScaleFactorPermille(3, 1), 333u);
    // 3 -> 2 : 2000/3 = 666.67 -> 667
    EXPECT_EQ(ScaleFactorPermille(3, 2), 667u);
}

// --- DownscalePermitted: twink protection truth table ---

TEST(DownscalePermitted, UpscaleAlwaysAllowed)
{
    // isDownscale == false: every combination is allowed (up-scaling a trinket/effect item is fine).
    EXPECT_TRUE(DownscalePermitted(false, true, true, false));
    EXPECT_TRUE(DownscalePermitted(false, false, false, true));
}

TEST(DownscalePermitted, DownscaleBlockedForTrinketOrEffect)
{
    EXPECT_TRUE(DownscalePermitted(true, false, false, true));    // plain item, allowed globally
    EXPECT_FALSE(DownscalePermitted(true, true, false, true));    // trinket -> blocked
    EXPECT_FALSE(DownscalePermitted(true, false, true, true));    // on-use/on-equip effect -> blocked
    EXPECT_FALSE(DownscalePermitted(true, true, true, true));     // both -> blocked
}

TEST(DownscalePermitted, GlobalSwitchBlocksAllDownscale)
{
    EXPECT_FALSE(DownscalePermitted(true, false, false, false));  // downscale globally disabled
}

// --- ClampScalePermille: apply the guard, clamping a blocked down-scale back to identity ---

TEST(ClampScalePermille, UpscalePassesThrough)
{
    EXPECT_EQ(ClampScalePermille(2000, true, true, false), 2000u);   // up-scale, always kept
}

TEST(ClampScalePermille, PermittedDownscalePassesThrough)
{
    EXPECT_EQ(ClampScalePermille(500, false, false, true), 500u);    // plain item, downscale allowed
}

TEST(ClampScalePermille, BlockedDownscaleClampsToIdentity)
{
    EXPECT_EQ(ClampScalePermille(500, true, false, true), 1000u);    // trinket
    EXPECT_EQ(ClampScalePermille(500, false, true, true), 1000u);    // effect item
    EXPECT_EQ(ClampScalePermille(500, false, false, false), 1000u);  // global switch off
}

TEST(ClampScalePermille, IdentityStaysIdentity)
{
    EXPECT_EQ(ClampScalePermille(1000, true, true, false), 1000u);   // 1000 is not a down-scale
}

// --- ScaleStatBlock: proportional, exact-total, deterministic ---

namespace
{
    StatBlock CasterBlock()
    {
        StatBlock b;
        b.Set(ItemStat::SpellPower, 100);
        b.Set(ItemStat::Intellect, 60);
        b.Set(ItemStat::HitRating, 40);   // total 200
        return b;
    }
}

// Up-scaling lands the total EXACTLY on the target and keeps the proportions ordered.
TEST(ScaleStatBlock, UpscaleHitsExactTotal)
{
    StatBlock const scaled = ScaleStatBlock(CasterBlock(), 500);   // 2.5x
    EXPECT_EQ(scaled.Total(), 500u);
    EXPECT_GE(scaled.Get(ItemStat::SpellPower), scaled.Get(ItemStat::Intellect));
    EXPECT_GE(scaled.Get(ItemStat::Intellect), scaled.Get(ItemStat::HitRating));
    EXPECT_EQ(scaled.Get(ItemStat::SpellPower), 250u);   // 100/200 * 500
}

// Down-scaling also lands exactly on the target.
TEST(ScaleStatBlock, DownscaleHitsExactTotal)
{
    StatBlock const scaled = ScaleStatBlock(CasterBlock(), 100);   // 0.5x
    EXPECT_EQ(scaled.Total(), 100u);
    EXPECT_EQ(scaled.Get(ItemStat::SpellPower), 50u);
}

// A target that is not a clean multiple still lands exactly on target (largest-remainder mops up).
TEST(ScaleStatBlock, AwkwardTargetStillExact)
{
    for (uint32_t target : { 1u, 7u, 13u, 199u, 333u, 4001u })
        EXPECT_EQ(ScaleStatBlock(CasterBlock(), target).Total(), target) << "target " << target;
}

// Points only ever land on stats that already carried points (zero stays zero).
TEST(ScaleStatBlock, OnlyScalesNonZeroStats)
{
    StatBlock const scaled = ScaleStatBlock(CasterBlock(), 500);
    EXPECT_EQ(scaled.Get(ItemStat::Stamina), 0u);
    EXPECT_EQ(scaled.Get(ItemStat::Strength), 0u);
    EXPECT_EQ(scaled.Get(ItemStat::Spirit), 0u);
}

// A zero source block cannot be distributed proportionally: zeroed.
TEST(ScaleStatBlock, ZeroSourceIsZeroed)
{
    StatBlock empty;
    EXPECT_EQ(ScaleStatBlock(empty, 500).Total(), 0u);
}

// A zero target zeroes the block.
TEST(ScaleStatBlock, ZeroTargetIsZeroed)
{
    EXPECT_EQ(ScaleStatBlock(CasterBlock(), 0).Total(), 0u);
}

// Deterministic: identical inputs => identical output.
TEST(ScaleStatBlock, Deterministic)
{
    EXPECT_TRUE(ScaleStatBlock(CasterBlock(), 4321) == ScaleStatBlock(CasterBlock(), 4321));
}
