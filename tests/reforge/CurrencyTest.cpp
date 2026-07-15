#include "reforge/Currency.h"
#include <gtest/gtest.h>

using namespace Reforge;

// A well-formed multi-currency spec parses to the records in order, with entry 0 meaning gold.
TEST(ParseCurrencyCosts, ParsesOrderedRecords)
{
    auto costs = ParseCurrencyCosts("0:100000, 43228:5, 40752:2");
    ASSERT_EQ(costs.size(), 3u);
    EXPECT_EQ(costs[0], (CurrencyCost{ 0, 100000 }));       // gold
    EXPECT_EQ(costs[1], (CurrencyCost{ 43228, 5 }));
    EXPECT_EQ(costs[2], (CurrencyCost{ 40752, 2 }));
}

// Whitespace around tokens and records is tolerated.
TEST(ParseCurrencyCosts, ToleratesWhitespace)
{
    auto costs = ParseCurrencyCosts("  0 : 50  ,   77 : 3  ");
    ASSERT_EQ(costs.size(), 2u);
    EXPECT_EQ(costs[0], (CurrencyCost{ 0, 50 }));
    EXPECT_EQ(costs[1], (CurrencyCost{ 77, 3 }));
}

// Malformed records are dropped (never throw): non-numeric, missing colon, missing field.
TEST(ParseCurrencyCosts, DropsMalformedRecords)
{
    auto costs = ParseCurrencyCosts("0:100, abc:5, 12, 34:, :7, 99:8");
    ASSERT_EQ(costs.size(), 2u);
    EXPECT_EQ(costs[0], (CurrencyCost{ 0, 100 }));
    EXPECT_EQ(costs[1], (CurrencyCost{ 99, 8 }));
}

// A zero count is meaningless as a price and is dropped.
TEST(ParseCurrencyCosts, DropsZeroCount)
{
    auto costs = ParseCurrencyCosts("55:0, 56:1");
    ASSERT_EQ(costs.size(), 1u);
    EXPECT_EQ(costs[0], (CurrencyCost{ 56, 1 }));
}

// Empty / whitespace-only spec yields no currencies.
TEST(ParseCurrencyCosts, EmptyIsEmpty)
{
    EXPECT_TRUE(ParseCurrencyCosts("").empty());
    EXPECT_TRUE(ParseCurrencyCosts("   ").empty());
    EXPECT_TRUE(ParseCurrencyCosts(" , , ").empty());
}

// FindCost returns the chosen currency's price, or nullopt if that entry is not accepted.
TEST(FindCost, ResolvesAcceptedCurrency)
{
    auto costs = ParseCurrencyCosts("0:100000, 43228:5");

    auto gold = FindCost(costs, 0);
    ASSERT_TRUE(gold.has_value());
    EXPECT_EQ(gold->count, 100000u);

    auto token = FindCost(costs, 43228);
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(token->count, 5u);

    EXPECT_FALSE(FindCost(costs, 99999).has_value());   // not an accepted currency
}

// On a duplicate entry the FIRST record wins (matches parse order).
TEST(FindCost, FirstDuplicateWins)
{
    auto costs = ParseCurrencyCosts("50:1, 50:9");
    auto found = FindCost(costs, 50);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->count, 1u);
}
