#include "reforge/Protocol.h"
#include <gtest/gtest.h>

using namespace Reforge;
using namespace Reforge::Addon;

// HELLO round-trips version + enabled and carries the RFRG prefix + KIND.
TEST(Protocol, HelloRoundTrip)
{
    HelloFrame in{ ProtocolVersion, true };
    std::string const frame = EncodeHello(in);
    EXPECT_EQ(frame.rfind("RFRG\tHELLO\t", 0), 0u);
    EXPECT_EQ(ClassifyFrame(frame), FrameKind::Hello);

    HelloFrame out;
    ASSERT_TRUE(DecodeHello(frame, out));
    EXPECT_EQ(out, in);
}

// CFG carries the reforge cap as permille, exactly.
TEST(Protocol, ConfigRoundTrip)
{
    ConfigFrame in{ 400 };   // 0.40
    std::string const frame = EncodeConfig(in);
    EXPECT_EQ(ClassifyFrame(frame), FrameKind::Config);

    ConfigFrame out;
    ASSERT_TRUE(DecodeConfig(frame, out));
    EXPECT_EQ(out.maxFractionPermille, 400u);
}

// CUR round-trips the accepted-currency list, order preserved.
TEST(Protocol, CurrenciesRoundTrip)
{
    std::vector<CurrencyCost> in{ { 0, 100000 }, { 43228, 5 }, { 40752, 2 } };
    bool trunc = false;
    std::string const frame = EncodeCurrencies(in, trunc);
    EXPECT_FALSE(trunc);
    EXPECT_EQ(ClassifyFrame(frame), FrameKind::Currencies);

    std::vector<CurrencyCost> out;
    bool outTrunc = true;
    ASSERT_TRUE(DecodeCurrencies(frame, out, outTrunc));
    EXPECT_FALSE(outTrunc);
    EXPECT_EQ(out, in);
}

// An empty currency list encodes and decodes to an empty vector (no records).
TEST(Protocol, CurrenciesEmpty)
{
    bool trunc = false;
    std::string const frame = EncodeCurrencies({}, trunc);
    std::vector<CurrencyCost> out{ { 1, 1 } };   // pre-seed to prove it is cleared
    bool outTrunc = false;
    ASSERT_TRUE(DecodeCurrencies(frame, out, outTrunc));
    EXPECT_TRUE(out.empty());
}

// ITEM round-trips the per-item reforge list.
TEST(Protocol, ItemsRoundTrip)
{
    std::vector<ItemReforge> in{ { 4, 4, 31, 25 }, { 8, 6, 45, 40 } };
    bool trunc = false;
    std::string const frame = EncodeItems(in, trunc);
    EXPECT_FALSE(trunc);
    EXPECT_EQ(ClassifyFrame(frame), FrameKind::Items);

    std::vector<ItemReforge> out;
    bool outTrunc = true;
    ASSERT_TRUE(DecodeItems(frame, out, outTrunc));
    EXPECT_FALSE(outTrunc);
    EXPECT_EQ(out, in);
}

// Frames never exceed the 255-byte CHAT_MSG_ADDON cap; overflow sets outTruncated (never a silent split).
TEST(Protocol, ItemsTruncateAtFrameCap)
{
    std::vector<ItemReforge> many;
    for (uint8_t i = 0; i < 100; ++i)
        many.push_back({ i, 4, 31, 123456 });

    bool trunc = false;
    std::string const frame = EncodeItems(many, trunc);
    EXPECT_LE(frame.size(), MaxFrame);
    EXPECT_TRUE(trunc);

    // What DID fit still decodes cleanly.
    std::vector<ItemReforge> out;
    bool outTrunc = false;
    ASSERT_TRUE(DecodeItems(frame, out, outTrunc));
    EXPECT_FALSE(out.empty());
    EXPECT_LT(out.size(), many.size());
}

// A decoder rejects a frame of the wrong kind (never mis-parses across kinds).
TEST(Protocol, DecoderRejectsWrongKind)
{
    ConfigFrame cfg{ 250 };
    std::string const cfgFrame = EncodeConfig(cfg);

    HelloFrame hello;
    EXPECT_FALSE(DecodeHello(cfgFrame, hello));

    ConfigFrame out;
    EXPECT_TRUE(DecodeConfig(cfgFrame, out));   // right kind still works
}

// Malformed bodies are rejected without throwing.
TEST(Protocol, RejectsMalformed)
{
    HelloFrame hello;
    EXPECT_FALSE(DecodeHello("RFRG\tHELLO\tx\t1", hello));      // non-numeric version
    EXPECT_FALSE(DecodeHello("RFRG\tHELLO\t1\t2", hello));      // enabled not 0/1
    EXPECT_FALSE(DecodeHello("RFRG\tHELLO\t1", hello));         // too few fields

    std::vector<ItemReforge> items;
    bool trunc = false;
    EXPECT_FALSE(DecodeItems("RFRG\tITEM\t4:4:31", items, trunc));   // 3 sub-fields, need 4

    EXPECT_EQ(ClassifyFrame("NOPE\tHELLO\t1\t1"), FrameKind::Unknown);
}
