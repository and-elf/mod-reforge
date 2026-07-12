#include "reforge/Reitemize.h"
#include "fakes/FakeReforgeConfig.h"
#include <gtest/gtest.h>

using namespace Reforge;
using namespace Reforge::Test;

namespace
{
    ItemChassis CasterChassis(uint32_t budget)
    {
        ItemChassis c;
        c.budget = budget;
        c.archetype = ItemArchetype::CasterDps;
        c.armor = ArmorClass::Cloth;
        c.slot = EquipSlot::Chest;
        return c;
    }
}

// --- ArchetypeTemplate: budget conservation is the headline invariant ---

// The template spends the budget EXACTLY, across a range of budgets and both defined archetypes.
TEST(ArchetypeTemplate, SpendsBudgetExactly)
{
    FakeReforgeConfig cfg;
    for (uint32_t budget : { 1u, 2u, 7u, 100u, 1000u, 12345u, 999983u })
    {
        ItemChassis caster = CasterChassis(budget);
        EXPECT_EQ(ArchetypeTemplate(caster, cfg).Total(), budget) << "caster budget " << budget;

        ItemChassis tank = caster;
        tank.archetype = ItemArchetype::PlateTank;
        EXPECT_EQ(ArchetypeTemplate(tank, cfg).Total(), budget) << "tank budget " << budget;
    }
}

// Points land ONLY on template stats (weight > 0) — never on an off-archetype stat.
TEST(ArchetypeTemplate, OnlyAssignsToTemplateStats)
{
    FakeReforgeConfig cfg;
    StatBlock out = ArchetypeTemplate(CasterChassis(1000), cfg);
    for (std::size_t i = 0; i < static_cast<std::size_t>(ItemStat::COUNT); ++i)
    {
        ItemStat const stat = static_cast<ItemStat>(i);
        if (cfg.StatWeight(ItemArchetype::CasterDps, stat) <= 0.0)
        {
            EXPECT_EQ(out.Get(stat), 0u) << "off-template stat " << i << " got points";
        }
    }
    // Spot check specific off-template stats for a caster piece.
    EXPECT_EQ(out.Get(ItemStat::Strength), 0u);
    EXPECT_EQ(out.Get(ItemStat::AttackPower), 0u);
    EXPECT_EQ(out.Get(ItemStat::DefenseRating), 0u);
}

// Higher template weight => at least as many points. SpellPower(4) >= Intellect(3) >= each 1-weight.
TEST(ArchetypeTemplate, PointsTrackWeights)
{
    FakeReforgeConfig cfg;
    StatBlock out = ArchetypeTemplate(CasterChassis(10000), cfg);
    EXPECT_GE(out.Get(ItemStat::SpellPower), out.Get(ItemStat::Intellect));
    EXPECT_GE(out.Get(ItemStat::Intellect), out.Get(ItemStat::HitRating));
    EXPECT_GE(out.Get(ItemStat::HitRating), 1u);
}

// Deterministic: identical inputs => identical output.
TEST(ArchetypeTemplate, Deterministic)
{
    FakeReforgeConfig cfg;
    EXPECT_TRUE(ArchetypeTemplate(CasterChassis(4321), cfg) == ArchetypeTemplate(CasterChassis(4321), cfg));
}

// Zero budget => zeroed block.
TEST(ArchetypeTemplate, ZeroBudgetIsZeroed)
{
    FakeReforgeConfig cfg;
    EXPECT_EQ(ArchetypeTemplate(CasterChassis(0), cfg).Total(), 0u);
}

// An archetype with no template (all weights 0) => zeroed block, even with budget.
TEST(ArchetypeTemplate, EmptyTemplateIsZeroed)
{
    FakeReforgeConfig cfg;
    ItemChassis healer = CasterChassis(1000);
    healer.archetype = ItemArchetype::CasterHealer;   // undefined in the fake => empty template
    EXPECT_EQ(ArchetypeTemplate(healer, cfg).Total(), 0u);
}

// --- ApplyReforge: bounded, budget-conserving stat movement ---

// A legal in-cap reforge conserves the total (budget) and moves exactly `amount`.
TEST(ApplyReforge, ConservesTotalAndMovesExactAmount)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);

    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);
    base.Set(ItemStat::Intellect, 60);
    uint32_t const before = base.Total();

    // cap = floor(100 * 0.40) = 40; move 30 SpellPower -> HitRating (both legal for CasterDps).
    auto out = ApplyReforge(base, { ItemStat::SpellPower, ItemStat::HitRating, 30 }, chassis, cfg);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->Total(), before);                       // budget conserved
    EXPECT_EQ(out->Get(ItemStat::SpellPower), 70u);        // 100 - 30
    EXPECT_EQ(out->Get(ItemStat::HitRating), 30u);         // 0 + 30
    EXPECT_EQ(out->Get(ItemStat::Intellect), 60u);         // untouched
}

// Reforge exactly at the fraction cap succeeds; one past it is rejected.
TEST(ApplyReforge, FractionCapBoundary)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);   // cap = 40

    EXPECT_TRUE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::HitRating, 40 }, chassis, cfg).has_value());
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::HitRating, 41 }, chassis, cfg).has_value());
}

// Illegal destination for the archetype is rejected.
TEST(ApplyReforge, RejectsIllegalDestination)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);

    // Strength / DefenseRating are not legal caster destinations.
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::Strength, 10 }, chassis, cfg).has_value());
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::DefenseRating, 10 }, chassis, cfg).has_value());
}

// Reforging into a template-omitted but archetype-legal stat (Spirit for a caster) is allowed.
TEST(ApplyReforge, AllowsLegalNonTemplateDestination)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);

    auto out = ApplyReforge(base, { ItemStat::SpellPower, ItemStat::Spirit, 20 }, chassis, cfg);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->Get(ItemStat::Spirit), 20u);
    EXPECT_EQ(out->Total(), base.Total());
}

// from == to is rejected (a no-op reforge is not a valid move).
TEST(ApplyReforge, RejectsSameStat)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::SpellPower, 10 }, chassis, cfg).has_value());
}

// Zero amount is rejected.
TEST(ApplyReforge, RejectsZeroAmount)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::SpellPower, ItemStat::HitRating, 0 }, chassis, cfg).has_value());
}

// Moving more than the source holds is rejected (also independently blocked by the cap).
TEST(ApplyReforge, RejectsInsufficientSource)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::HitRating, 10);   // small source; cap = floor(10*0.4) = 4
    EXPECT_FALSE(ApplyReforge(base, { ItemStat::HitRating, ItemStat::Spirit, 11 }, chassis, cfg).has_value());
}

// A larger fraction cap (config knob) lets more move — the limit is genuinely config-driven.
TEST(ApplyReforge, FractionIsConfigDriven)
{
    FakeReforgeConfig cfg;
    cfg.reforgeMaxFraction = 1.0;   // allow moving the whole stat
    ItemChassis chassis = CasterChassis(0);
    StatBlock base;
    base.Set(ItemStat::SpellPower, 100);

    auto out = ApplyReforge(base, { ItemStat::SpellPower, ItemStat::HitRating, 100 }, chassis, cfg);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->Get(ItemStat::SpellPower), 0u);
    EXPECT_EQ(out->Get(ItemStat::HitRating), 100u);
    EXPECT_EQ(out->Total(), base.Total());
}

// --- Gem sockets: automatic on re-itemization ---

// Auto sockets are granted per the slot policy, as the configured (prismatic) color.
TEST(ResolveSockets, GrantsConfiguredCountAndColor)
{
    FakeReforgeConfig cfg;

    ItemChassis chest = CasterChassis(1000);   // Chest => 2
    SocketLayout chestSockets = ResolveSockets(chest, cfg);
    EXPECT_EQ(chestSockets.Total(), 2u);
    EXPECT_EQ(chestSockets.Get(SocketColor::Prismatic), 2u);   // prismatic = accepts any gem

    ItemChassis neck = chest;
    neck.slot = EquipSlot::Neck;               // other slot => 1
    EXPECT_EQ(ResolveSockets(neck, cfg).Total(), 1u);
}

// A slot the policy grants no sockets to gets none.
TEST(ResolveSockets, NoSocketsWhenPolicyIsZero)
{
    FakeReforgeConfig cfg;
    ItemChassis none = CasterChassis(1000);
    none.slot = EquipSlot::None;               // policy => 0
    EXPECT_EQ(ResolveSockets(none, cfg).Total(), 0u);
}

// AutoSocketCount is clamped to MaxSockets so a template can never over-socket.
TEST(ResolveSockets, ClampedToMaxSockets)
{
    FakeReforgeConfig cfg;
    cfg.maxSockets = 1;                         // below the Chest policy of 2
    EXPECT_EQ(ResolveSockets(CasterChassis(1000), cfg).Total(), 1u);

    cfg.maxSockets = 0;                         // sockets globally disabled
    EXPECT_EQ(ResolveSockets(CasterChassis(1000), cfg).Total(), 0u);
}

// Deterministic.
TEST(ResolveSockets, Deterministic)
{
    FakeReforgeConfig cfg;
    EXPECT_TRUE(ResolveSockets(CasterChassis(1000), cfg) == ResolveSockets(CasterChassis(1000), cfg));
}

// Reitemize bundles the rebuilt stat block (budget preserved) and the auto sockets in one call.
TEST(Reitemize, BundlesStatsAndSockets)
{
    FakeReforgeConfig cfg;
    ItemChassis chest = CasterChassis(5000);
    ReitemizedItem item = Reitemize(chest, cfg);

    EXPECT_EQ(item.stats.Total(), 5000u);                          // budget spent exactly
    EXPECT_EQ(item.sockets.Total(), 2u);                           // Chest policy
    EXPECT_TRUE(item.stats == ArchetypeTemplate(chest, cfg));      // same as the primitives
    EXPECT_TRUE(item.sockets == ResolveSockets(chest, cfg));
}

// End-to-end: reforge a freshly templated item and the budget still holds.
TEST(ApplyReforge, ComposesWithArchetypeTemplate)
{
    FakeReforgeConfig cfg;
    ItemChassis chassis = CasterChassis(5000);
    StatBlock templated = ArchetypeTemplate(chassis, cfg);
    ASSERT_EQ(templated.Total(), 5000u);

    uint32_t const sp = templated.Get(ItemStat::SpellPower);
    uint32_t const move = sp / 4;                  // safely within the 40% cap
    ASSERT_GT(move, 0u);

    auto out = ApplyReforge(templated, { ItemStat::SpellPower, ItemStat::HasteRating, move }, chassis, cfg);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->Total(), 5000u);                // budget preserved through template + reforge
}
