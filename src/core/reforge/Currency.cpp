#include "Currency.h"
#include <cctype>

namespace Reforge
{
    namespace
    {
        // Trim ASCII whitespace from both ends of a view.
        std::string_view Trim(std::string_view s)
        {
            std::size_t b = 0;
            while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
                ++b;
            std::size_t e = s.size();
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
                --e;
            return s.substr(b, e - b);
        }

        // Parse a non-negative decimal from `s` into `out`. Returns false on empty input or any
        // non-digit character (so a malformed field drops the whole record rather than half-parsing).
        bool ParseUint(std::string_view s, uint32_t& out)
        {
            s = Trim(s);
            if (s.empty())
                return false;

            uint64_t value = 0;
            for (char const c : s)
            {
                if (c < '0' || c > '9')
                    return false;
                value = value * 10 + static_cast<uint64_t>(c - '0');
                if (value > 0xFFFFFFFFull)   // clamp overflow to the u32 ceiling; still a valid record
                    value = 0xFFFFFFFFull;
            }
            out = static_cast<uint32_t>(value);
            return true;
        }
    }

    std::vector<CurrencyCost> ParseCurrencyCosts(std::string_view spec)
    {
        std::vector<CurrencyCost> out;

        std::size_t pos = 0;
        while (pos <= spec.size())
        {
            std::size_t const comma = spec.find(',', pos);
            std::string_view const record = spec.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);

            std::string_view const trimmed = Trim(record);
            if (!trimmed.empty())
            {
                std::size_t const colon = trimmed.find(':');
                if (colon != std::string_view::npos)
                {
                    CurrencyCost cost;
                    if (ParseUint(trimmed.substr(0, colon), cost.entry)
                        && ParseUint(trimmed.substr(colon + 1), cost.count)
                        && cost.count > 0)
                    {
                        out.push_back(cost);
                    }
                }
            }

            if (comma == std::string_view::npos)
                break;
            pos = comma + 1;
        }

        return out;
    }

    std::optional<CurrencyCost> FindCost(std::vector<CurrencyCost> const& costs, uint32_t entry)
    {
        for (CurrencyCost const& cost : costs)
            if (cost.entry == entry)
                return cost;
        return std::nullopt;
    }
}
