#include "reforge/WeaponScale.h"
#include "fakes/FakeReforgeConfig.h"
#include <gtest/gtest.h>

using namespace Reforge;
using Reforge::Test::FakeReforgeConfig;

// Weapon-damage scaling on re-itemization (ARCHITECTURE §12). All branches: equal / up / down / zero
// source, ratio preservation, determinism, configurable factor, disabled policy, degenerate factors.

namespace
{
    // Cross-multiplied ratio equality avoids division and is exact for the doubles we produce:
    // a.min/a.max == b.min/b.max  <=>  a.min*b.max == a.max*b.min.
    void ExpectSameRatio(WeaponDamage const& a, WeaponDamage const& b)
    {
        EXPECT_DOUBLE_EQ(a.min * b.max, a.max * b.min);
    }
}

// fromLevel == toLevel: a well-behaved policy returns 1.0, so damage is unchanged.
TEST(WeaponScale, EqualLevelsIsIdentity)
{
    FakeReforgeConfig cfg;
    WeaponDamage const src{100.0, 150.0};

    WeaponDamage const out = ScaleWeaponDamage(src, 40, 40, cfg);

    EXPECT_DOUBLE_EQ(out.min, 100.0);
    EXPECT_DOUBLE_EQ(out.max, 150.0);
    EXPECT_DOUBLE_EQ(WeaponScaleFactor(40, 40, cfg), 1.0);
}

// toLevel > fromLevel: both endpoints grow, factor > 1, ratio preserved.
TEST(WeaponScale, ScalesUp)
{
    FakeReforgeConfig cfg;
    cfg.weaponScalePerLevel = 0.03;
    WeaponDamage const src{100.0, 150.0};

    double const factor = WeaponScaleFactor(20, 40, cfg);
    EXPECT_GT(factor, 1.0);

    WeaponDamage const out = ScaleWeaponDamage(src, 20, 40, cfg);
    EXPECT_GT(out.min, src.min);
    EXPECT_GT(out.max, src.max);
    EXPECT_DOUBLE_EQ(out.min, src.min * factor);
    EXPECT_DOUBLE_EQ(out.max, src.max * factor);
    ExpectSameRatio(out, src);
}

// toLevel < fromLevel: both endpoints shrink, factor < 1, ratio preserved.
TEST(WeaponScale, ScalesDown)
{
    FakeReforgeConfig cfg;
    cfg.weaponScalePerLevel = 0.03;
    WeaponDamage const src{100.0, 150.0};

    double const factor = WeaponScaleFactor(60, 40, cfg);
    EXPECT_LT(factor, 1.0);
    EXPECT_GT(factor, 0.0);

    WeaponDamage const out = ScaleWeaponDamage(src, 60, 40, cfg);
    EXPECT_LT(out.min, src.min);
    EXPECT_LT(out.max, src.max);
    ExpectSameRatio(out, src);
}

// A zero-damage source stays zero regardless of factor (no NaN, no negatives).
TEST(WeaponScale, ZeroSourceStaysZero)
{
    FakeReforgeConfig cfg;
    WeaponDamage const src{0.0, 0.0};

    WeaponDamage const up = ScaleWeaponDamage(src, 1, 80, cfg);
    EXPECT_DOUBLE_EQ(up.min, 0.0);
    EXPECT_DOUBLE_EQ(up.max, 0.0);
}

// A one-sided source (min 0, max > 0) keeps min at 0 and scales max by the factor.
TEST(WeaponScale, HandlesZeroMinDegenerateSource)
{
    FakeReforgeConfig cfg;
    cfg.weaponScalePerLevel = 0.03;
    WeaponDamage const src{0.0, 120.0};

    double const factor = WeaponScaleFactor(10, 30, cfg);
    WeaponDamage const out = ScaleWeaponDamage(src, 10, 30, cfg);
    EXPECT_DOUBLE_EQ(out.min, 0.0);
    EXPECT_DOUBLE_EQ(out.max, 120.0 * factor);
}

// Deterministic: identical inputs yield byte-identical output.
TEST(WeaponScale, Deterministic)
{
    FakeReforgeConfig cfg;
    WeaponDamage const src{37.0, 71.0};

    WeaponDamage const a = ScaleWeaponDamage(src, 15, 55, cfg);
    WeaponDamage const b = ScaleWeaponDamage(src, 15, 55, cfg);
    EXPECT_DOUBLE_EQ(a.min, b.min);
    EXPECT_DOUBLE_EQ(a.max, b.max);
}

// The factor genuinely comes from config: a steeper per-level curve scales harder for the same levels.
TEST(WeaponScale, ConfigurableFactorChangesOutput)
{
    WeaponDamage const src{100.0, 150.0};

    FakeReforgeConfig gentle;
    gentle.weaponScalePerLevel = 0.03;
    FakeReforgeConfig steep;
    steep.weaponScalePerLevel = 0.06;

    WeaponDamage const a = ScaleWeaponDamage(src, 20, 40, gentle);
    WeaponDamage const b = ScaleWeaponDamage(src, 20, 40, steep);

    EXPECT_GT(b.min, a.min);
    EXPECT_GT(b.max, a.max);
}

// Disabled policy => factor 1.0 => identity even across a wide level gap.
TEST(WeaponScale, DisabledPolicyIsIdentity)
{
    FakeReforgeConfig cfg;
    cfg.weaponScaleEnabled = false;
    WeaponDamage const src{100.0, 150.0};

    WeaponDamage const out = ScaleWeaponDamage(src, 1, 80, cfg);
    EXPECT_DOUBLE_EQ(out.min, 100.0);
    EXPECT_DOUBLE_EQ(out.max, 150.0);
}

// A non-positive factor from the policy is clamped to 0 => zero damage (never negative).
TEST(WeaponScale, NegativeFactorClampedToZero)
{
    FakeReforgeConfig cfg;
    cfg.forcedWeaponScale = -2.5;
    WeaponDamage const src{100.0, 150.0};

    EXPECT_DOUBLE_EQ(WeaponScaleFactor(1, 80, cfg), 0.0);

    WeaponDamage const out = ScaleWeaponDamage(src, 1, 80, cfg);
    EXPECT_DOUBLE_EQ(out.min, 0.0);
    EXPECT_DOUBLE_EQ(out.max, 0.0);
}

// A zero factor yields zero damage.
TEST(WeaponScale, ZeroFactorZeroDamage)
{
    FakeReforgeConfig cfg;
    cfg.forcedWeaponScale = 0.0;
    WeaponDamage const src{100.0, 150.0};

    WeaponDamage const out = ScaleWeaponDamage(src, 1, 80, cfg);
    EXPECT_DOUBLE_EQ(out.min, 0.0);
    EXPECT_DOUBLE_EQ(out.max, 0.0);
}

// WeaponScaleFactor echoes a well-behaved positive policy value unchanged.
TEST(WeaponScale, FactorEchoesPositivePolicy)
{
    FakeReforgeConfig cfg;
    cfg.forcedWeaponScale = 2.75;
    EXPECT_DOUBLE_EQ(WeaponScaleFactor(5, 50, cfg), 2.75);
}
