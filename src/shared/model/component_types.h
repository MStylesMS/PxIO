#pragma once

#include <string>

// Endpoint component types
enum class EndpointType
{
    DIGITAL_IN,
    DIGITAL_OUT,
    KEYPAD_MATRIX,
    PATCH_MAP,
    UNKNOWN
};

// Runtime pin direction / endpoint mode
enum class Mode
{
    INPUT,
    OUTPUT
};

// ─── string conversions ──────────────────────────────────────────────────────

inline EndpointType endpointTypeFromString(const std::string &s)
{
    if (s == "digital_in")
        return EndpointType::DIGITAL_IN;
    if (s == "digital_out")
        return EndpointType::DIGITAL_OUT;
    if (s == "keypad_matrix")
        return EndpointType::KEYPAD_MATRIX;
    if (s == "patch_map")
        return EndpointType::PATCH_MAP;
    return EndpointType::UNKNOWN;
}

inline std::string endpointTypeToString(EndpointType t)
{
    switch (t)
    {
    case EndpointType::DIGITAL_IN:
        return "digital_in";
    case EndpointType::DIGITAL_OUT:
        return "digital_out";
    case EndpointType::KEYPAD_MATRIX:
        return "keypad_matrix";
    case EndpointType::PATCH_MAP:
        return "patch_map";
    default:
        return "unknown";
    }
}

inline Mode modeFromString(const std::string &s)
{
    return (s == "output") ? Mode::OUTPUT : Mode::INPUT;
}

inline std::string modeToString(Mode m)
{
    return (m == Mode::OUTPUT) ? "output" : "input";
}
