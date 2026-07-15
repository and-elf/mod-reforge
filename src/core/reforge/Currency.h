#ifndef MOD_REFORGE_CORE_REFORGE_CURRENCY_H
#define MOD_REFORGE_CORE_REFORGE_CURRENCY_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Pure core. No AzerothCore includes are permitted anywhere under src/core/.
namespace Reforge
{
    // One accepted way to pay for a reforge: `count` of item `entry`. Entry 0 is GOLD (a copper
    // amount); any other value is an item entry used as a token (emblems, marks, a custom currency
    // item, crafting mats). The adapter charges gold vs items accordingly.
    struct CurrencyCost
    {
        uint32_t entry = 0;   // 0 => gold (copper); otherwise an item entry
        uint32_t count = 0;   // how many; always > 0 for a parsed record

        bool operator==(CurrencyCost const& other) const { return entry == other.entry && count == other.count; }
    };

    // Parse the "any currency" config string: a tolerant CSV of `entry:count` records
    //   e.g.  "0:100000, 43228:5, 40752:2"  =>  {0,100000},{43228,5},{40752,2}
    // Whitespace around tokens is ignored. A record is DROPPED (never throws) when it is malformed,
    // has a non-numeric field, or a zero count (a zero-count price is meaningless). Order is preserved
    // and deterministic so the client UI lists currencies in the configured order.
    std::vector<CurrencyCost> ParseCurrencyCosts(std::string_view spec);

    // The required cost for paying with currency `entry`, or nullopt if `entry` is not one of the
    // accepted currencies in `costs`. On duplicate entries the FIRST wins (matches parse order).
    std::optional<CurrencyCost> FindCost(std::vector<CurrencyCost> const& costs, uint32_t entry);
}

#endif // MOD_REFORGE_CORE_REFORGE_CURRENCY_H
