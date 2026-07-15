#ifndef MOD_REFORGE_CORE_REFORGE_PROTOCOL_H
#define MOD_REFORGE_CORE_REFORGE_PROTOCOL_H

#include "Currency.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Pure wire codec for the client-addon transport (§11). No AzerothCore includes: the adapter
// (`src/ReforgeAddonScripts`) fills these POD snapshots from live state and performs the send; this
// file only turns snapshots into `RFRG\t…` frames and parses them back. The Lua addon (Comms.lua)
// mirrors this grammar: tab between fields, ';' between list records, ':' between a record's
// sub-fields; fractions cross the wire as permille integers (x1000) so the round-trip is exact.
namespace Reforge::Addon
{
    inline constexpr char const* Prefix = "RFRG";
    inline constexpr char Sep = '\t';
    inline constexpr char RecSep = ';';
    inline constexpr char FieldSep = ':';
    inline constexpr std::size_t MaxFrame = 255;   // CHAT_MSG_ADDON bodies are capped 255 bytes.
    inline constexpr uint8_t ProtocolVersion = 1;

    // Frame kinds pushed server -> client. (Client -> server uses the built-in AzerothCore addon
    // command channel — the `.reforge` command — not a frame kind here.)
    enum class FrameKind : uint8_t { Hello, Currencies, Config, Items, Unknown };

    // ---- Snapshot POD inputs (adapter fills these; no AC types) ----

    struct HelloFrame
    {
        uint8_t version = ProtocolVersion;
        bool    enabled = false;

        bool operator==(HelloFrame const& o) const { return version == o.version && enabled == o.enabled; }
    };

    // The §5 reforge cap, as permille (0.40 -> 400) so the UI can bound its amount slider exactly.
    struct ConfigFrame
    {
        uint16_t maxFractionPermille = 0;

        bool operator==(ConfigFrame const& o) const { return maxFractionPermille == o.maxFractionPermille; }
    };

    // One equipped item's active reforge, as the client renders it. `slot` is the character's equipment
    // slot index; `from`/`to` are ItemStat ordinals; `amount` is the moved point count.
    struct ItemReforge
    {
        uint8_t  slot = 0;
        uint8_t  from = 0;
        uint8_t  to = 0;
        uint32_t amount = 0;

        bool operator==(ItemReforge const& o) const
        {
            return slot == o.slot && from == o.from && to == o.to && amount == o.amount;
        }
    };

    // ---- Frame-kind dispatch ----

    // Classify a raw incoming frame (leading "RFRG\t" required). Returns Unknown for anything else;
    // never throws.
    FrameKind ClassifyFrame(std::string_view frame);

    // ---- Encoders (server -> client): each returns a full "RFRG\t<KIND>\t…" frame ----
    std::string EncodeHello(HelloFrame const&);
    std::string EncodeConfig(ConfigFrame const&);
    // Pack as many records as fit MaxFrame; set `outTruncated` if any were dropped (never a silent
    // split — same contract both encoders share).
    std::string EncodeCurrencies(std::vector<CurrencyCost> const&, bool& outTruncated);
    std::string EncodeItems(std::vector<ItemReforge> const&, bool& outTruncated);

    // ---- Decoders (test + Lua parity). Each returns false on a wrong kind or malformed body; never
    // throws. `frame` is the full "RFRG\t…" frame. ----
    bool DecodeHello(std::string_view frame, HelloFrame& out);
    bool DecodeConfig(std::string_view frame, ConfigFrame& out);
    bool DecodeCurrencies(std::string_view frame, std::vector<CurrencyCost>& out, bool& outTruncated);
    bool DecodeItems(std::string_view frame, std::vector<ItemReforge>& out, bool& outTruncated);
}

#endif // MOD_REFORGE_CORE_REFORGE_PROTOCOL_H
