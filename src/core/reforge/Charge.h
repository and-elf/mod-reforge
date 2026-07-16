#ifndef MOD_REFORGE_CORE_REFORGE_CHARGE_H
#define MOD_REFORGE_CORE_REFORGE_CHARGE_H

#include "Currency.h"
#include <cstdint>
#include <vector>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
//
// The decidable helpers the server adapter composes: the bounded-fraction reforge cap, the reforge
// NPC's amount menu, the currency charge decision (including the insufficient-funds branch) and the
// NPC-gate decision. Kept here (POD in / POD out) so the NPC gossip, `.reforge` command and client
// addon paths all share ONE tested implementation and the adapter stays a thin wire-up around it.
namespace Reforge
{
    // The most of a single SOURCE stat one reforge may move: floor(sourceValue * maxFraction).
    // Matches ARCHITECTURE §5 exactly; ApplyReforge and every adapter pre-check derive their cap
    // here so the bound can never drift between the NPC, command and addon paths. A non-positive
    // fraction (or a zero source) yields 0.
    uint32_t ReforgeCap(uint32_t sourceValue, double maxFraction);

    // The reforge NPC's amount menu for a source whose cap is `cap`: 25/50/75/100% of `cap`, floored,
    // dropping zero and duplicate buckets (a small cap collapses the low ones). Deterministic; order
    // preserved low -> high. A zero cap yields no options.
    std::vector<uint32_t> AmountOptions(uint32_t cap);

    // Whether a reforge is allowed at the player's current location. RequireNpc off => reforge from
    // anywhere; on => only when a Reforge NPC is near. The adapter supplies `nearReforger` (its
    // FindNearestCreature result); isolating the decision here makes it testable without a world.
    bool ReforgeAllowedHere(bool requireNpc, bool nearReforger);

    // Outcome of resolving a chosen currency against the accepted list and checking affordability.
    enum class ChargeStatus : uint8_t
    {
        Ok,             // accepted and affordable: deduct `amount` of `entry`
        NotAccepted,    // `chosenEntry` is not one of the realm's accepted currencies
        Insufficient    // accepted, but the player holds less than the required count
    };

    struct ChargePlan
    {
        ChargeStatus status = ChargeStatus::NotAccepted;
        uint32_t     entry = 0;    // the chosen currency (0 = gold copper); echoed for the caller
        uint32_t     amount = 0;   // required count to deduct (meaningful on Ok and Insufficient)

        bool operator==(ChargePlan const& o) const
        {
            return status == o.status && entry == o.entry && amount == o.amount;
        }
    };

    // Resolve `chosenEntry` against `accepted` and decide whether `playerHas` covers the price.
    // `playerHas` is copper for gold (entry 0) or the owned item count otherwise. Pure: the adapter
    // reads `playerHas` (GetMoney / GetItemCount) and, only on Ok, performs the matching deduction.
    ChargePlan PlanCharge(std::vector<CurrencyCost> const& accepted, uint32_t chosenEntry, uint64_t playerHas);
}

#endif // MOD_REFORGE_CORE_REFORGE_CHARGE_H
