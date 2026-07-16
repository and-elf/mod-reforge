#include "reforge/Blocklist.h"
#include <gtest/gtest.h>

using namespace Reforge;

// --- ParseIdList: tolerant CSV of item entry ids ---

// A well-formed list parses to the ids in order.
TEST(ParseIdList, ParsesOrderedIds)
{
    auto ids = ParseIdList("49623, 50735, 17182");
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 49623u);
    EXPECT_EQ(ids[1], 50735u);
    EXPECT_EQ(ids[2], 17182u);
}

// Whitespace is tolerated; empty / zero / non-numeric tokens are dropped (never throws).
TEST(ParseIdList, ToleratesJunkAndDropsBadTokens)
{
    auto ids = ParseIdList("  100 , , abc, 0, 200 ,");
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 100u);
    EXPECT_EQ(ids[1], 200u);
}

// Empty / whitespace-only spec yields no ids.
TEST(ParseIdList, EmptyIsEmpty)
{
    EXPECT_TRUE(ParseIdList("").empty());
    EXPECT_TRUE(ParseIdList("   ").empty());
    EXPECT_TRUE(ParseIdList(" , , ").empty());
}

// --- SlotFromName / ParseSlotList ---

// Canonical names round-trip through SlotName/SlotFromName for every real slot.
TEST(SlotFromName, CanonicalNamesRoundTrip)
{
    for (std::size_t i = 1; i < static_cast<std::size_t>(EquipSlot::COUNT); ++i)
    {
        EquipSlot const slot = static_cast<EquipSlot>(i);
        auto parsed = SlotFromName(SlotName(slot));
        ASSERT_TRUE(parsed.has_value()) << SlotName(slot);
        EXPECT_EQ(*parsed, slot);
    }
}

// Parsing is case-insensitive, trims whitespace, and accepts common aliases and raw ordinals.
TEST(SlotFromName, AliasesAndCaseAndOrdinal)
{
    EXPECT_EQ(SlotFromName("Trinket"), EquipSlot::Trinket);
    EXPECT_EQ(SlotFromName("  RANGED "), EquipSlot::Ranged);
    EXPECT_EQ(SlotFromName("ring"), EquipSlot::Finger);
    EXPECT_EQ(SlotFromName("back"), EquipSlot::Cloak);
    EXPECT_EQ(SlotFromName("gloves"), EquipSlot::Hands);
    EXPECT_EQ(SlotFromName("2h"), EquipSlot::TwoHand);
    EXPECT_EQ(SlotFromName(std::to_string(static_cast<int>(EquipSlot::Chest))), EquipSlot::Chest);
}

// An unknown name is nullopt.
TEST(SlotFromName, UnknownIsNullopt)
{
    EXPECT_FALSE(SlotFromName("banana").has_value());
    EXPECT_FALSE(SlotFromName("").has_value());
}

// ParseSlotList keeps the known slots in order and drops the unknown.
TEST(ParseSlotList, KeepsKnownDropsUnknown)
{
    auto slots = ParseSlotList("trinket, banana, ranged");
    ASSERT_EQ(slots.size(), 2u);
    EXPECT_EQ(slots[0], EquipSlot::Trinket);
    EXPECT_EQ(slots[1], EquipSlot::Ranged);
}

// --- ArmorClassFromName / ParseArmorClassList ---

TEST(ArmorClassFromName, NamesCaseAndOrdinal)
{
    EXPECT_EQ(ArmorClassFromName("cloth"), ArmorClass::Cloth);
    EXPECT_EQ(ArmorClassFromName("LEATHER"), ArmorClass::Leather);
    EXPECT_EQ(ArmorClassFromName(" mail "), ArmorClass::Mail);
    EXPECT_EQ(ArmorClassFromName("plate"), ArmorClass::Plate);
    EXPECT_EQ(ArmorClassFromName("none"), ArmorClass::None);
    EXPECT_FALSE(ArmorClassFromName("wood").has_value());
}

TEST(ParseArmorClassList, KeepsKnownDropsUnknown)
{
    auto classes = ParseArmorClassList("cloth, wood, plate");
    ASSERT_EQ(classes.size(), 2u);
    EXPECT_EQ(classes[0], ArmorClass::Cloth);
    EXPECT_EQ(classes[1], ArmorClass::Plate);
}

// --- IsBlocked: the OR decision across all axes ---

// An empty policy blocks nothing, whatever the key.
TEST(IsBlocked, EmptyPolicyBlocksNothing)
{
    BlockPolicy policy;
    BlockKey key{ 12345, EquipSlot::Trinket, ArmorClass::Cloth, 5 };
    EXPECT_FALSE(IsBlocked(key, policy));
}

// Axis 1: by item entry id.
TEST(IsBlocked, ByItemId)
{
    BlockPolicy policy;
    policy.itemIds = { 49623, 50735 };

    EXPECT_TRUE(IsBlocked(BlockKey{ 49623, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
    EXPECT_TRUE(IsBlocked(BlockKey{ 50735, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
}

// Axis 2: by equip slot.
TEST(IsBlocked, BySlot)
{
    BlockPolicy policy;
    policy.slots = { EquipSlot::Trinket, EquipSlot::Ranged };

    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Trinket, ArmorClass::None, 3 }, policy));
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Ranged, ArmorClass::None, 3 }, policy));
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::Chest, ArmorClass::None, 3 }, policy));
}

// Axis 3a: by quality ordinal.
TEST(IsBlocked, ByQuality)
{
    BlockPolicy policy;
    policy.qualities = { 5, 7 };   // legendary, heirloom

    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 5 }, policy));
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 7 }, policy));
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
}

// Axis 3b: by armour class.
TEST(IsBlocked, ByArmorClass)
{
    BlockPolicy policy;
    policy.armorClasses = { ArmorClass::Cloth };

    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Chest, ArmorClass::Cloth, 3 }, policy));
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::Chest, ArmorClass::Plate, 3 }, policy));
}

// The axes combine with OR: a key matching any single axis is blocked even when it misses the others.
TEST(IsBlocked, AxesCombineWithOr)
{
    BlockPolicy policy;
    policy.itemIds = { 49623 };
    policy.slots = { EquipSlot::Trinket };
    policy.armorClasses = { ArmorClass::Cloth };
    policy.qualities = { 5 };

    // Matches on id only.
    EXPECT_TRUE(IsBlocked(BlockKey{ 49623, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
    // Matches on slot only.
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Trinket, ArmorClass::Plate, 4 }, policy));
    // Matches on armour class only.
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Cloth, 4 }, policy));
    // Matches on quality only.
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 5 }, policy));
    // Matches nothing.
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::Plate, 4 }, policy));
}

// A None slot / None armour in the key does not match unless None is explicitly listed.
TEST(IsBlocked, NoneKeyOnlyMatchesWhenListed)
{
    BlockPolicy noneUnlisted;
    noneUnlisted.slots = { EquipSlot::Trinket };
    noneUnlisted.armorClasses = { ArmorClass::Cloth };
    EXPECT_FALSE(IsBlocked(BlockKey{ 1, EquipSlot::None, ArmorClass::None, 4 }, noneUnlisted));

    BlockPolicy noneListed;
    noneListed.armorClasses = { ArmorClass::None };
    EXPECT_TRUE(IsBlocked(BlockKey{ 1, EquipSlot::Head, ArmorClass::None, 4 }, noneListed));
}
