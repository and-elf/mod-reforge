#ifndef MOD_REFORGE_CORE_REFORGE_REFORGECONFIG_H
#define MOD_REFORGE_CORE_REFORGE_REFORGECONFIG_H

#include "Stats.h"

namespace Reforge
{
    // Injected tunables + itemization data for the reforge engine. The host (bare server or a policy
    // module like mod-branding) supplies the stat templates, destination legality and reforge limit,
    // so the engine itself stays generic and deterministic.
    class IReforgeConfig
    {
    public:
        virtual ~IReforgeConfig() = default;

        // Max fraction of a single SOURCE stat that one reforge may move (Cata-style; default 0.40).
        // Bounds player agency so specific drops still matter — you tune a drop, you don't rebuild it.
        virtual double ReforgeMaxFraction() const = 0;

        // Relative weight of `stat` in `archetype`'s default template. 0 => not in the template.
        // ArchetypeTemplate distributes the chassis budget proportionally to these weights.
        virtual double StatWeight(ItemArchetype archetype, ItemStat stat) const = 0;

        // Whether `stat` is a legal reforge DESTINATION for `archetype`. Typically a superset of the
        // template stats (you may reforge into an archetype-appropriate stat the template omits).
        virtual bool StatLegal(ItemArchetype archetype, ItemStat stat) const = 0;

        // --- Gem sockets (automatic on re-itemization) ---

        // How many sockets a re-itemized item of this slot/archetype receives automatically (0 = none).
        // This is the "automatic socketing" knob: the engine grants the sockets; the player fills them
        // with gems as normal. Clamped to MaxSockets().
        virtual uint8_t AutoSocketCount(EquipSlot slot, ItemArchetype archetype) const = 0;

        // Color of auto-granted sockets. Prismatic (recommended default) accepts any gem.
        virtual SocketColor AutoSocketColor() const = 0;

        // Hard cap on total sockets an item may carry (WoW convention ~3). Clamps AutoSocketCount so a
        // misconfigured template can never over-socket.
        virtual uint8_t MaxSockets() const = 0;

        // --- Weapon-damage scaling (issue #7) ---

        // Multiplicative factor applied to a re-itemized weapon's min AND max damage for a source->target
        // level transition (source item level -> target/current level). >1 scales up, <1 scales down,
        // 1.0 = no change. Injected so the whole curve is config-driven and the core stays deterministic;
        // a well-behaved policy returns exactly 1.0 when fromLevel == toLevel. See ARCHITECTURE §12.
        virtual double WeaponDamageScale(uint32_t fromLevel, uint32_t toLevel) const = 0;
    };
}

#endif // MOD_REFORGE_CORE_REFORGE_REFORGECONFIG_H
