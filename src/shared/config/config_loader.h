#pragma once

#include <string>
#include "config_schema.h"

// Loads and validates an INI config file into AppConfig.
// Unknown keys emit LOG_WARN and are skipped (never fatal).
// Missing required fields or malformed required sections are fatal (return false).

class ConfigLoader
{
public:
    // Load from path. Returns true on success; config_out is populated.
    // On failure, logs the reason and returns false.
    static bool load(const std::string &path, AppConfig &config_out);

private:
    // Validate populated config; returns false if a required field is absent.
    static bool validate(const AppConfig &cfg);
};
