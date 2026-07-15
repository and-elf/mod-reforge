#include "Protocol.h"
#include <array>
#include <charconv>

namespace Reforge::Addon
{
    namespace
    {
        std::string Head(char const* kind)
        {
            std::string s = Prefix;
            s += Sep;
            s += kind;
            return s;
        }

        void AppendUint(std::string& s, uint64_t value)
        {
            std::array<char, 20> buf{};
            auto const [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
            if (ec == std::errc())
                s.append(buf.data(), ptr);
            else
                s += '0';
        }

        // Split `s` on `delim` into fields (a trailing/empty field is preserved).
        std::vector<std::string_view> Split(std::string_view s, char delim)
        {
            std::vector<std::string_view> out;
            std::size_t pos = 0;
            while (true)
            {
                std::size_t const next = s.find(delim, pos);
                if (next == std::string_view::npos)
                {
                    out.push_back(s.substr(pos));
                    break;
                }
                out.push_back(s.substr(pos, next - pos));
                pos = next + 1;
            }
            return out;
        }

        bool ToUint(std::string_view s, uint64_t& out)
        {
            if (s.empty())
                return false;
            uint64_t value = 0;
            auto const [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
            if (ec != std::errc() || ptr != s.data() + s.size())
                return false;
            out = value;
            return true;
        }

        // Verify the frame begins with "RFRG\t<KIND>\t" and return the body after that second tab.
        // Returns false (body untouched) on any mismatch.
        bool Body(std::string_view frame, char const* kind, std::string_view& body)
        {
            std::string const head = Head(kind) + Sep;
            if (frame.size() < head.size() || frame.substr(0, head.size()) != head)
                return false;
            body = frame.substr(head.size());
            return true;
        }
    }

    FrameKind ClassifyFrame(std::string_view frame)
    {
        std::string const pfx = std::string(Prefix) + Sep;
        if (frame.size() < pfx.size() || frame.substr(0, pfx.size()) != pfx)
            return FrameKind::Unknown;

        std::string_view const rest = frame.substr(pfx.size());
        std::size_t const end = rest.find(Sep);
        std::string_view const kind = rest.substr(0, end);

        if (kind == "HELLO") return FrameKind::Hello;
        if (kind == "CUR")   return FrameKind::Currencies;
        if (kind == "CFG")   return FrameKind::Config;
        if (kind == "ITEM")  return FrameKind::Items;
        return FrameKind::Unknown;
    }

    std::string EncodeHello(HelloFrame const& f)
    {
        std::string s = Head("HELLO");
        s += Sep;
        AppendUint(s, f.version);
        s += Sep;
        s += f.enabled ? '1' : '0';
        return s;
    }

    std::string EncodeConfig(ConfigFrame const& f)
    {
        std::string s = Head("CFG");
        s += Sep;
        AppendUint(s, f.maxFractionPermille);
        return s;
    }

    std::string EncodeCurrencies(std::vector<CurrencyCost> const& costs, bool& outTruncated)
    {
        outTruncated = false;
        std::string s = Head("CUR");
        s += Sep;

        bool first = true;
        for (CurrencyCost const& c : costs)
        {
            std::string rec;
            if (!first)
                rec += RecSep;
            AppendUint(rec, c.entry);
            rec += FieldSep;
            AppendUint(rec, c.count);

            if (s.size() + rec.size() > MaxFrame)
            {
                outTruncated = true;
                break;
            }
            s += rec;
            first = false;
        }
        return s;
    }

    std::string EncodeItems(std::vector<ItemReforge> const& items, bool& outTruncated)
    {
        outTruncated = false;
        std::string s = Head("ITEM");
        s += Sep;

        bool first = true;
        for (ItemReforge const& it : items)
        {
            std::string rec;
            if (!first)
                rec += RecSep;
            AppendUint(rec, it.slot);
            rec += FieldSep;
            AppendUint(rec, it.from);
            rec += FieldSep;
            AppendUint(rec, it.to);
            rec += FieldSep;
            AppendUint(rec, it.amount);

            if (s.size() + rec.size() > MaxFrame)
            {
                outTruncated = true;
                break;
            }
            s += rec;
            first = false;
        }
        return s;
    }

    bool DecodeHello(std::string_view frame, HelloFrame& out)
    {
        std::string_view body;
        if (!Body(frame, "HELLO", body))
            return false;

        std::vector<std::string_view> const f = Split(body, Sep);
        if (f.size() != 2)
            return false;

        uint64_t version = 0;
        if (!ToUint(f[0], version) || (f[1] != "0" && f[1] != "1"))
            return false;

        out.version = static_cast<uint8_t>(version);
        out.enabled = f[1] == "1";
        return true;
    }

    bool DecodeConfig(std::string_view frame, ConfigFrame& out)
    {
        std::string_view body;
        if (!Body(frame, "CFG", body))
            return false;

        uint64_t permille = 0;
        if (!ToUint(body, permille))
            return false;

        out.maxFractionPermille = static_cast<uint16_t>(permille);
        return true;
    }

    bool DecodeCurrencies(std::string_view frame, std::vector<CurrencyCost>& out, bool& outTruncated)
    {
        outTruncated = false;
        std::string_view body;
        if (!Body(frame, "CUR", body))
            return false;

        out.clear();
        if (body.empty())
            return true;

        for (std::string_view const rec : Split(body, RecSep))
        {
            std::vector<std::string_view> const sub = Split(rec, FieldSep);
            uint64_t entry = 0, count = 0;
            if (sub.size() != 2 || !ToUint(sub[0], entry) || !ToUint(sub[1], count))
                return false;
            out.push_back({ static_cast<uint32_t>(entry), static_cast<uint32_t>(count) });
        }
        return true;
    }

    bool DecodeItems(std::string_view frame, std::vector<ItemReforge>& out, bool& outTruncated)
    {
        outTruncated = false;
        std::string_view body;
        if (!Body(frame, "ITEM", body))
            return false;

        out.clear();
        if (body.empty())
            return true;

        for (std::string_view const rec : Split(body, RecSep))
        {
            std::vector<std::string_view> const sub = Split(rec, FieldSep);
            uint64_t slot = 0, from = 0, to = 0, amount = 0;
            if (sub.size() != 4 || !ToUint(sub[0], slot) || !ToUint(sub[1], from)
                || !ToUint(sub[2], to) || !ToUint(sub[3], amount))
                return false;
            out.push_back({ static_cast<uint8_t>(slot), static_cast<uint8_t>(from),
                            static_cast<uint8_t>(to), static_cast<uint32_t>(amount) });
        }
        return true;
    }
}
