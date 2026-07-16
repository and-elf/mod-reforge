#ifndef MOD_REFORGE_CORE_REFORGE_WEAPONSCALE_H
#define MOD_REFORGE_CORE_REFORGE_WEAPONSCALE_H

#include "ReforgeConfig.h"
#include <cstdint>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
//
// Weapon-damage scaling on re-itemization (ARCHITECTURE §12). A reforge re-itemizes a drop for a new
// level range; a weapon used at that range must have its min/max damage (and thus DPS) scaled for the
// source->target level transition. The scaling curve is injected via IReforgeConfig::WeaponDamageScale,
// so this layer is a deterministic applier -- it multiplies BOTH endpoints by the SAME factor, which
// preserves the min:max ratio exactly. POD in / POD out; unit-tested in tests/reforge/WeaponScaleTest.
namespace Reforge
{
    // A weapon's damage range. Mirrors ItemTemplate::_Damage (DamageMin/DamageMax), which the game
    // stores as float; kept as double here for exact ratio arithmetic. The adapter narrows to float at
    // the point of application (there is no in-core rounding -- see §12).
    struct WeaponDamage
    {
        double min = 0.0;
        double max = 0.0;
    };

    // The multiplicative factor the injected policy returns for a fromLevel->toLevel transition, clamped
    // to be non-negative (a bad/negative policy value can never produce negative damage). Pure.
    double WeaponScaleFactor(uint32_t fromLevel, uint32_t toLevel, IReforgeConfig const& cfg);

    // Scale `source` for the fromLevel->toLevel transition using the injected policy. Both min and max
    // are multiplied by the SAME (clamped, non-negative) factor, so:
    //   - the min:max RATIO is preserved exactly,
    //   - a zero source endpoint stays zero,
    //   - a well-behaved policy that returns 1.0 for equal levels yields an identity transform,
    //   - a non-positive factor collapses damage to zero (degenerate but defined, never negative).
    // Deterministic: identical inputs always yield identical output (pure arithmetic, no RNG, no
    // rounding in-core). See ARCHITECTURE §12.
    WeaponDamage ScaleWeaponDamage(WeaponDamage const& source, uint32_t fromLevel, uint32_t toLevel,
                                   IReforgeConfig const& cfg);
}

#endif // MOD_REFORGE_CORE_REFORGE_WEAPONSCALE_H
