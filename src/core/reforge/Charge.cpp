#include "Charge.h"
#include <cmath>

namespace Reforge
{
    uint32_t ReforgeCap(uint32_t sourceValue, double maxFraction)
    {
        if (maxFraction <= 0.0 || sourceValue == 0)
            return 0;

        double const cap = std::floor(static_cast<double>(sourceValue) * maxFraction);
        return cap > 0.0 ? static_cast<uint32_t>(cap) : 0;
    }

    std::vector<uint32_t> AmountOptions(uint32_t cap)
    {
        std::vector<uint32_t> out;
        for (double const f : { 0.25, 0.50, 0.75, 1.00 })
        {
            uint32_t const v = static_cast<uint32_t>(std::floor(static_cast<double>(cap) * f));
            if (v > 0 && (out.empty() || out.back() != v))
                out.push_back(v);
        }
        return out;
    }

    bool ReforgeAllowedHere(bool requireNpc, bool nearReforger)
    {
        return !requireNpc || nearReforger;
    }

    ChargePlan PlanCharge(std::vector<CurrencyCost> const& accepted, uint32_t chosenEntry, uint64_t playerHas)
    {
        ChargePlan plan;
        plan.entry = chosenEntry;

        std::optional<CurrencyCost> const cost = FindCost(accepted, chosenEntry);
        if (!cost)
        {
            plan.status = ChargeStatus::NotAccepted;
            return plan;
        }

        plan.amount = cost->count;
        plan.status = playerHas >= cost->count ? ChargeStatus::Ok : ChargeStatus::Insufficient;
        return plan;
    }
}
