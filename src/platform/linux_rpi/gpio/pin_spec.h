#pragma once

#include <optional>
#include <stdexcept>
#include <string>

// Native BCM: "17". Expander: "expander_a:8" (returns the offset only).
// Never throws — a bad spec returns nullopt so the process stays up.
inline std::optional<unsigned int> parsePinOffset(const std::string &pin_str)
{
    try
    {
        auto colon = pin_str.find(':');
        const std::string num = (colon != std::string::npos)
                                    ? pin_str.substr(colon + 1)
                                    : pin_str;
        if (num.empty())
            return std::nullopt;
        std::size_t pos = 0;
        int n = std::stoi(num, &pos, 0);
        if (pos != num.size() || n < 0)
            return std::nullopt;
        return static_cast<unsigned int>(n);
    }
    catch (...)
    {
        return std::nullopt;
    }
}
