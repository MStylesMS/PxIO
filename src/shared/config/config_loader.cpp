#include "config_loader.h"
#include "../log/logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::string trim(const std::string &s)
{
    const auto ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos)
        return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static bool parseBool(const std::string &v)
{
    std::string l = v;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    return l == "true" || l == "1" || l == "yes";
}

static int parseInt(const std::string &v, int fallback = 0)
{
    try
    {
        std::size_t pos;
        int n = std::stoi(v, &pos, 0); // base 0: auto-detect hex/octal/decimal
        return n;
    }
    catch (...)
    {
        return fallback;
    }
}

// Split a comma-separated string into trimmed tokens.
static std::vector<std::string> splitCsv(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        auto t = trim(token);
        if (!t.empty())
            out.push_back(t);
    }
    return out;
}

// Parse comma-separated integers.
static std::vector<int> splitCsvInt(const std::string &s)
{
    auto tokens = splitCsv(s);
    std::vector<int> out;
    out.reserve(tokens.size());
    for (const auto &t : tokens)
        out.push_back(parseInt(t));
    return out;
}

// ─── INI parser ──────────────────────────────────────────────────────────────
// Produces a flat list of { section, key, value } triples.

struct IniEntry
{
    std::string section;
    std::string key;
    std::string value;
};

static std::vector<IniEntry> parseIni(const std::string &path, bool &ok)
{
    std::vector<IniEntry> entries;
    std::ifstream f(path);
    if (!f.is_open())
    {
        LOG_ERROR("ConfigLoader: cannot open config file: " + path);
        ok = false;
        return entries;
    }

    std::string current_section;
    std::string line;
    int lineno = 0;

    while (std::getline(f, line))
    {
        ++lineno;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            current_section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            LOG_WARN("ConfigLoader: ignoring unparseable line " + std::to_string(lineno) + ": " + line);
            continue;
        }

        IniEntry e;
        e.section = current_section;
        e.key = trim(line.substr(0, eq));
        e.value = trim(line.substr(eq + 1));
        entries.push_back(std::move(e));
    }

    ok = true;
    return entries;
}

// Split section "gpio:led_red" into { "gpio", "led_red" }.
// Returns {"", section} if no colon.
static std::pair<std::string, std::string> splitSection(const std::string &section)
{
    auto colon = section.find(':');
    if (colon == std::string::npos)
        return {"", section};
    return {section.substr(0, colon), section.substr(colon + 1)};
}

// ─── load ────────────────────────────────────────────────────────────────────

bool ConfigLoader::load(const std::string &path, AppConfig &cfg)
{
    bool ok = false;
    auto entries = parseIni(path, ok);
    if (!ok)
        return false;

    cfg.config_file_path = path;

    // Track which sections we have seen to detect unknown keys per section.
    // Known section prefixes
    const std::vector<std::string> known_prefixes{"global", "mqtt", "gpio", "compound", "bus"};

    // We build compound and bus configs into maps keyed by name, then move to vectors.
    std::unordered_map<std::string, GpioEndpointConfig> gpio_map;
    std::unordered_map<std::string, CompoundEndpointConfig> compound_map;
    std::unordered_map<std::string, BusConfig> bus_map;

    for (const auto &e : entries)
    {
        auto [prefix, sname] = splitSection(e.section);

        // ── [global] ──────────────────────────────────────────────────────
        if (prefix.empty() && sname == "global")
        {
            auto &g = cfg.global;
            if (e.key == "instance_name")
                g.instance_name = e.value;
            else if (e.key == "log_level")
                g.log_level = Logger::levelFromString(e.value);
            else if (e.key == "poll_interval_ms")
                g.poll_interval_ms = parseInt(e.value, 20);
            else if (e.key == "message_level")
                g.message_level = e.value;
            else if (e.key == "log_file")
                g.log_file = e.value;
            else
                LOG_WARN("ConfigLoader: unknown key [global]." + e.key);
            continue;
        }

        // ── [mqtt] ────────────────────────────────────────────────────────
        if (prefix.empty() && sname == "mqtt")
        {
            auto &m = cfg.mqtt;
            if (e.key == "host")
                m.host = e.value;
            else if (e.key == "port")
                m.port = parseInt(e.value, 1883);
            else if (e.key == "username")
                m.username = e.value;
            else if (e.key == "password")
                m.password = e.value;
            else if (e.key == "client_id")
                m.client_id = e.value;
            else if (e.key == "qos")
                m.qos = parseInt(e.value, 1);
            else if (e.key == "global_retain")
                m.global_retain = parseBool(e.value);
            else if (e.key == "base_topic")
                m.base_topic = e.value;
            else if (e.key == "commands_topic")
                m.commands_topic = e.value;
            else if (e.key == "state_topic")
                m.state_topic = e.value;
            else if (e.key == "events_topic")
                m.events_topic = e.value;
            else if (e.key == "warnings_topic")
                m.warnings_topic = e.value;
            else if (e.key == "announce_topic")
                m.announce_topic = e.value;
            else if (e.key == "heartbeat_topic")
                m.heartbeat_topic = e.value;
            else if (e.key == "heartbeat_interval_ms")
                m.heartbeat_interval_ms = parseInt(e.value, 5000);
            else
                LOG_WARN("ConfigLoader: unknown key [mqtt]." + e.key);
            continue;
        }

        // ── [gpio:<name>] ────────────────────────────────────────────────
        if (prefix == "gpio")
        {
            auto &g = gpio_map[sname];
            g.name = sname;
            if (e.key == "type")
                g.type = e.value;
            else if (e.key == "pin")
                g.pin = e.value;
            else if (e.key == "io_topic")
                g.io_topic = e.value;
            else if (e.key == "default")
                g.default_value = e.value;
            else if (e.key == "retain")
            {
                g.retain = parseBool(e.value);
                g.retain_explicit = true;
            }
            else if (e.key == "debounce_cycles")
                g.debounce_cycles = parseInt(e.value, 8);
            else if (e.key == "allow_mode_switch")
                g.allow_mode_switch = parseBool(e.value);
            else
                LOG_WARN("ConfigLoader: unknown key [gpio:" + sname + "]." + e.key);
            continue;
        }

        // ── [compound:<name>] ────────────────────────────────────────────
        if (prefix == "compound")
        {
            auto &c = compound_map[sname];
            c.name = sname;
            if (e.key == "type")
                c.type = e.value;
            else if (e.key == "io_topic")
                c.io_topic = e.value;
            else if (e.key == "retain")
            {
                c.retain = parseBool(e.value);
                c.retain_explicit = true;
            }
            else if (e.key == "rows")
                c.rows = splitCsvInt(e.value);
            else if (e.key == "cols")
                c.cols = splitCsvInt(e.value);
            else if (e.key == "keys")
                c.keys = splitCsv(e.value);
            else if (e.key == "debounce_cycles")
                c.debounce_cycles = parseInt(e.value, 16);
            else if (e.key == "pins")
                c.pins = splitCsv(e.value);
            else if (e.key == "scan_interval_ms")
                c.scan_interval_ms = parseInt(e.value, 100);
            else if (e.key == "allow_mode_switch")
                c.allow_mode_switch = parseBool(e.value);
            else
                LOG_WARN("ConfigLoader: unknown key [compound:" + sname + "]." + e.key);
            continue;
        }

        // ── [bus:<name>] ─────────────────────────────────────────────────
        if (prefix == "bus")
        {
            auto &b = bus_map[sname];
            b.name = sname;
            if (e.key == "type")
                b.type = e.value;
            else if (e.key == "device")
                b.device = e.value;
            else if (e.key == "parent")
                b.parent = e.value;
            else if (e.key == "address")
                b.address = parseInt(e.value, 0);
            else if (e.key == "pin_count")
                b.pin_count = parseInt(e.value, 16);
            else if (e.key == "baud")
                b.baud = parseInt(e.value, 115200);
            else
                LOG_WARN("ConfigLoader: unknown key [bus:" + sname + "]." + e.key);
            continue;
        }

        // ── unknown section ───────────────────────────────────────────────
        LOG_WARN("ConfigLoader: unknown section [" + e.section + "], key " + e.key + " ignored");
    }

    // Move maps → vectors (preserves stable iteration order via map key sort)
    for (auto &[name, g] : gpio_map)
        cfg.gpio_endpoints.push_back(std::move(g));
    for (auto &[name, c] : compound_map)
        cfg.compound_endpoints.push_back(std::move(c));
    for (auto &[name, b] : bus_map)
        cfg.buses.push_back(std::move(b));

    return validate(cfg);
}

// ─── validate ────────────────────────────────────────────────────────────────

bool ConfigLoader::validate(const AppConfig &cfg)
{
    bool ok = true;

    auto require = [&](const std::string &val, const std::string &field)
    {
        if (val.empty())
        {
            LOG_ERROR("ConfigLoader: required field missing: " + field);
            ok = false;
        }
    };

    // [mqtt] required fields
    require(cfg.mqtt.host, "[mqtt].host");
    require(cfg.mqtt.client_id, "[mqtt].client_id");
    require(cfg.mqtt.base_topic, "[mqtt].base_topic");
    require(cfg.mqtt.commands_topic, "[mqtt].commands_topic");
    require(cfg.mqtt.state_topic, "[mqtt].state_topic");
    require(cfg.mqtt.events_topic, "[mqtt].events_topic");
    require(cfg.mqtt.warnings_topic, "[mqtt].warnings_topic");
    require(cfg.mqtt.announce_topic, "[mqtt].announce_topic");
    require(cfg.mqtt.heartbeat_topic, "[mqtt].heartbeat_topic");

    // qos range
    if (cfg.mqtt.qos < 0 || cfg.mqtt.qos > 2)
    {
        LOG_ERROR("ConfigLoader: [mqtt].qos must be 0, 1, or 2");
        ok = false;
    }

    // heartbeat_interval_ms > 0
    if (cfg.mqtt.heartbeat_interval_ms <= 0)
    {
        LOG_ERROR("ConfigLoader: [mqtt].heartbeat_interval_ms must be > 0");
        ok = false;
    }

    // [gpio] endpoints
    for (const auto &g : cfg.gpio_endpoints)
    {
        std::string ctx = "[gpio:" + g.name + "]";
        if (g.type.empty())
        {
            LOG_ERROR("ConfigLoader: " + ctx + " missing type");
            ok = false;
        }
        if (g.pin.empty())
        {
            LOG_ERROR("ConfigLoader: " + ctx + " missing pin");
            ok = false;
        }
        if (g.io_topic.empty())
        {
            LOG_ERROR("ConfigLoader: " + ctx + " missing io_topic");
            ok = false;
        }
    }

    // [compound] endpoints
    for (const auto &c : cfg.compound_endpoints)
    {
        std::string ctx = "[compound:" + c.name + "]";
        if (c.type.empty())
        {
            LOG_ERROR("ConfigLoader: " + ctx + " missing type");
            ok = false;
        }
        if (c.io_topic.empty())
        {
            LOG_ERROR("ConfigLoader: " + ctx + " missing io_topic");
            ok = false;
        }
        if (c.type == "keypad_matrix")
        {
            if (c.rows.empty())
            {
                LOG_ERROR("ConfigLoader: " + ctx + " missing rows");
                ok = false;
            }
            if (c.cols.empty())
            {
                LOG_ERROR("ConfigLoader: " + ctx + " missing cols");
                ok = false;
            }
            if (c.keys.empty())
            {
                LOG_ERROR("ConfigLoader: " + ctx + " missing keys");
                ok = false;
            }
            if (!c.rows.empty() && !c.cols.empty() && !c.keys.empty())
            {
                if (c.keys.size() != c.rows.size() * c.cols.size())
                {
                    LOG_ERROR("ConfigLoader: " + ctx + " keys count (" + std::to_string(c.keys.size()) + ") != rows*cols (" + std::to_string(c.rows.size() * c.cols.size()) + ")");
                    ok = false;
                }
            }
        }
        if (c.type == "patch_map")
        {
            if (c.pins.size() < 2)
            {
                LOG_ERROR("ConfigLoader: " + ctx + " patch_map requires at least 2 pins");
                ok = false;
            }
        }
    }

    return ok;
}
