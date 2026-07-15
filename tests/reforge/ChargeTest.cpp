#include "reforge/Charge.h"
#include <gtest/gtest.h>

using namespace Reforge;

// --- ReforgeCap: the bounded-fraction cap (ARCHITECTURE §5) ---

// The cap is floor(source * fraction) across a range of sources at the default 40%.
TEST(ReforgeCap, FloorsSourceTimesFraction)
{
    EXPECT_EQ(ReforgeCap(100, 0.40), 40u);
    EXPECT_EQ(ReforgeCap(99, 0.40), 39u);    // floor(39.6)
    EXPECT_EQ(ReforgeCap(101, 0.40), 40u);   // floor(40.4)
    EXPECT_EQ(ReforgeCap(1, 0.40), 0u);      // floor(0.4)
    EXPECT_EQ(ReforgeCap(3, 0.40), 1u);      // floor(1.2)
}

// The cap is genuinely fraction-driven: a bigger fraction lets more move; 1.0 == the whole stat.
TEST(ReforgeCap, TracksFraction)
{
    EXPECT_EQ(ReforgeCap(100, 1.00), 100u);
    EXPECT_EQ(ReforgeCap(100, 0.50), 50u);
    EXPECT_EQ(ReforgeCap(100, 0.10), 10u);
}

// Degenerate inputs never overflow or move anything.
TEST(ReforgeCap, ZeroInputsYieldZero)
{
    EXPECT_EQ(ReforgeCap(0, 0.40), 0u);      // no source
    EXPECT_EQ(ReforgeCap(100, 0.0), 0u);     // fraction disabled
    EXPECT_EQ(ReforgeCap(100, -0.5), 0u);    // guarded against a bad config value
}

// --- AmountOptions: the reforge NPC's 25/50/75/100%-of-cap menu ---

// A comfortable cap yields four ascending buckets.
TEST(AmountOptions, FourBucketsForALargeCap)
{
    auto opts = AmountOptions(40);
    ASSERT_EQ(opts.size(), 4u);
    EXPECT_EQ(opts[0], 10u);   // 25%
    EXPECT_EQ(opts[1], 20u);   // 50%
    EXPECT_EQ(opts[2], 30u);   // 75%
    EXPECT_EQ(opts[3], 40u);   // 100%
}

// A small cap collapses duplicate / zero buckets rather than offering a useless "move 0".
TEST(AmountOptions, CollapsesDuplicateAndZeroBuckets)
{
    // cap 1: 0.25->0, 0.5->0, 0.75->0, 1.0->1  => just {1}
    EXPECT_EQ(AmountOptions(1), (std::vector<uint32_t>{ 1 }));
    // cap 2: 0->0, 1->1, 1->(dup), 2->2         => {1,2}
    EXPECT_EQ(AmountOptions(2), (std::vector<uint32_t>{ 1, 2 }));
    // cap 3: 0->0, 1->1, 2->2, 3->3             => {1,2,3}
    EXPECT_EQ(AmountOptions(3), (std::vector<uint32_t>{ 1, 2, 3 }));
}

// A zero cap offers nothing to move.
TEST(AmountOptions, ZeroCapIsEmpty)
{
    EXPECT_TRUE(AmountOptions(0).empty());
}

// Options are always > 0, strictly ascending and within the cap.
TEST(AmountOptions, OptionsAreBoundedAscendingNonZero)
{
    for (uint32_t cap : { 1u, 2u, 5u, 7u, 40u, 100u, 999u })
    {
        auto opts = AmountOptions(cap);
        uint32_t prev = 0;
        for (uint32_t v : opts)
        {
            EXPECT_GT(v, prev) << "cap " << cap;   // strictly ascending, so >0 and de-duped
            EXPECT_LE(v, cap) << "cap " << cap;     // never exceeds the cap (the 100% bucket)
            prev = v;
        }
    }
}

// --- ReforgeAllowedHere: the Reforge.RequireNpc gate ---

// RequireNpc off => reforge from anywhere; on => only when a Reforger is near. Full truth table.
TEST(ReforgeAllowedHere, GateTruthTable)
{
    EXPECT_TRUE(ReforgeAllowedHere(false, false));   // gate off, away  -> allowed
    EXPECT_TRUE(ReforgeAllowedHere(false, true));    // gate off, near  -> allowed
    EXPECT_FALSE(ReforgeAllowedHere(true, false));   // gate on,  away  -> blocked
    EXPECT_TRUE(ReforgeAllowedHere(true, true));     // gate on,  near  -> allowed
}

// --- PlanCharge: resolve the chosen currency + the insufficient-funds branch ---

namespace
{
    std::vector<CurrencyCost> const kAccepted{ { 0, 100000 }, { 43228, 5 }, { 40752, 2 } };
}

// Paying gold with enough copper succeeds and reports the exact copper to deduct.
TEST(PlanCharge, GoldAffordable)
{
    ChargePlan const plan = PlanCharge(kAccepted, 0, 250000);
    EXPECT_EQ(plan, (ChargePlan{ ChargeStatus::Ok, 0, 100000 }));
}

// Paying gold at exactly the price is affordable (boundary).
TEST(PlanCharge, GoldExactBalanceIsAffordable)
{
    ChargePlan const plan = PlanCharge(kAccepted, 0, 100000);
    EXPECT_EQ(plan.status, ChargeStatus::Ok);
    EXPECT_EQ(plan.amount, 100000u);
}

// One copper short of the price is insufficient (still reports the required amount for the message).
TEST(PlanCharge, GoldOneShortIsInsufficient)
{
    ChargePlan const plan = PlanCharge(kAccepted, 0, 99999);
    EXPECT_EQ(plan.status, ChargeStatus::Insufficient);
    EXPECT_EQ(plan.entry, 0u);
    EXPECT_EQ(plan.amount, 100000u);
}

// Paying an item token with enough owned succeeds; the deduction is the token count, not copper.
TEST(PlanCharge, TokenAffordable)
{
    ChargePlan const plan = PlanCharge(kAccepted, 43228, 5);
    EXPECT_EQ(plan, (ChargePlan{ ChargeStatus::Ok, 43228, 5 }));
}

// Owning fewer tokens than required is insufficient.
TEST(PlanCharge, TokenInsufficient)
{
    ChargePlan const plan = PlanCharge(kAccepted, 40752, 1);   // need 2, have 1
    EXPECT_EQ(plan.status, ChargeStatus::Insufficient);
    EXPECT_EQ(plan.amount, 2u);
}

// A currency the realm does not accept is rejected before any funds check.
TEST(PlanCharge, UnacceptedCurrencyRejected)
{
    ChargePlan const plan = PlanCharge(kAccepted, 99999, 1000000);
    EXPECT_EQ(plan.status, ChargeStatus::NotAccepted);
    EXPECT_EQ(plan.entry, 99999u);
}

// With no accepted currencies at all, nothing is chargeable (even gold).
TEST(PlanCharge, EmptyAcceptedListRejectsEverything)
{
    EXPECT_EQ(PlanCharge({}, 0, 1000000).status, ChargeStatus::NotAccepted);
}

// --- Cross-checking the vehicle's -from safety property headlessly ---

// The enchant-slot vehicle subtracts `amount` from the item's native `from` stat. Because a legal
// reforge is capped at ReforgeCap(source, fraction) <= source, the subtraction can never underflow:
// for every source and any fraction in (0,1], cap <= source, so (source - cap) >= 0.
TEST(ReforgeVehicle, FromSubtractionNeverUnderflows)
{
    for (uint32_t source : { 1u, 2u, 3u, 10u, 37u, 100u, 250u, 999u })
        for (double frac : { 0.10, 0.25, 0.40, 0.50, 0.75, 1.00 })
        {
            uint32_t const cap = ReforgeCap(source, frac);
            EXPECT_LE(cap, source) << "source " << source << " frac " << frac;
            // The +to side is exactly the moved amount; the -from side is source - amount, and since
            // amount <= cap <= source it stays >= 0.
            EXPECT_GE(source - cap, 0u);
        }
}
