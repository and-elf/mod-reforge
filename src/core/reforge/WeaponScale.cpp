#include "WeaponScale.h"

namespace Reforge
{
    double WeaponScaleFactor(uint32_t fromLevel, uint32_t toLevel, IReforgeConfig const& cfg)
    {
        double const factor = cfg.WeaponDamageScale(fromLevel, toLevel);
        return factor > 0.0 ? factor : 0.0;
    }

    WeaponDamage ScaleWeaponDamage(WeaponDamage const& source, uint32_t fromLevel, uint32_t toLevel,
                                   IReforgeConfig const& cfg)
    {
        double const factor = WeaponScaleFactor(fromLevel, toLevel, cfg);

        WeaponDamage out;
        out.min = source.min * factor;
        out.max = source.max * factor;
        return out;
    }
}
