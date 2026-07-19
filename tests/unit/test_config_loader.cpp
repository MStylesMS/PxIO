// tests/unit/test_config_loader.cpp
// ConfigLoader unit tests.

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

#include "shared/config/config_loader.h"
#include "shared/config/config_schema.h"
#include "shared/log/logger.h"

namespace fs = std::filesystem;

// Writes a temp INI file and returns its path.
static std::string writeTempIni(const std::string &contents)
{
    auto path = fs::temp_directory_path() / "pio_test_config.ini";
    std::ofstream f(path);
    f << contents;
    return path.string();
}

// ─── Minimal valid config ────────────────────────────────────────────────────

static const char *kMinimalIni = R"(
[global]
instance_name = test_pxio

[mqtt]
host = localhost
client_id = pxio_test
base_topic = pxio/test
commands_topic = pxio/test/commands
state_topic = pxio/test/state
events_topic = pxio/test/events
warnings_topic = pxio/test/warnings
announce_topic = pxio/test/announce
heartbeat_topic = pxio/test/heartbeat
)";

TEST(ConfigLoader, LoadsMinimalConfig)
{
    auto path = writeTempIni(kMinimalIni);
    AppConfig cfg;
    ASSERT_TRUE(ConfigLoader::load(path, cfg));

    EXPECT_EQ(cfg.global.instance_name, "test_pxio");
    EXPECT_EQ(cfg.mqtt.host, "localhost");
    EXPECT_EQ(cfg.mqtt.client_id, "pxio_test");
    EXPECT_EQ(cfg.mqtt.base_topic, "pxio/test");
}

// ─── Unknown keys warn but do not fail ───────────────────────────────────────

static const char *kUnknownKeyIni = R"(
[global]
instance_name = pxio
furble_wumble = this_key_does_not_exist

[mqtt]
host = localhost
client_id = test
base_topic = pxio
commands_topic = pxio/commands
state_topic = pxio/state
events_topic = pxio/events
warnings_topic = pxio/warnings
announce_topic = pxio/announce
heartbeat_topic = pxio/heartbeat
)";

TEST(ConfigLoader, UnknownKeyDoesNotFailLoad)
{
    auto path = writeTempIni(kUnknownKeyIni);
    AppConfig cfg;
    // Should still succeed despite unknown key 'furble_wumble'
    EXPECT_TRUE(ConfigLoader::load(path, cfg));
}

// ─── Missing required mqtt.host fails validation ─────────────────────────────

static const char *kMissingHostIni = R"(
[global]
instance_name = pxio

[mqtt]
client_id = test
base_topic = pxio
commands_topic = pxio/commands
state_topic = pxio/state
events_topic = pxio/events
warnings_topic = pxio/warnings
announce_topic = pxio/announce
heartbeat_topic = pxio/heartbeat
)";

TEST(ConfigLoader, MissingMqttHostFails)
{
    auto path = writeTempIni(kMissingHostIni);
    AppConfig cfg;
    EXPECT_FALSE(ConfigLoader::load(path, cfg));
}

// ─── Missing required mqtt.client_id fails ───────────────────────────────────

static const char *kMissingClientIdIni = R"(
[global]
instance_name = pxio

[mqtt]
host = localhost
base_topic = pxio
commands_topic = pxio/commands
state_topic = pxio/state
events_topic = pxio/events
warnings_topic = pxio/warnings
announce_topic = pxio/announce
heartbeat_topic = pxio/heartbeat
)";

TEST(ConfigLoader, MissingClientIdFails)
{
    auto path = writeTempIni(kMissingClientIdIni);
    AppConfig cfg;
    EXPECT_FALSE(ConfigLoader::load(path, cfg));
}

// ─── GPIO endpoint parsed correctly ──────────────────────────────────────────

static const char *kWithGpioIni = R"(
[global]
instance_name = pxio

[mqtt]
host = localhost
client_id = pxio
base_topic = pxio
commands_topic = pxio/commands
state_topic = pxio/state
events_topic = pxio/events
warnings_topic = pxio/warnings
announce_topic = pxio/announce
heartbeat_topic = pxio/heartbeat

[gpio:led_red]
type = digital_out
pin = 17
io_topic = gpio/led_red
default = 0
retain = true
allow_mode_switch = false

[gpio:button]
type = digital_in
pin = 27
io_topic = gpio/button
debounce_cycles = 4
)";

TEST(ConfigLoader, GpioEndpointParsed)
{
    auto path = writeTempIni(kWithGpioIni);
    AppConfig cfg;
    ASSERT_TRUE(ConfigLoader::load(path, cfg));
    ASSERT_EQ(cfg.gpio_endpoints.size(), 2u);

    // Find by name (unordered_map iteration order is not guaranteed)
    const GpioEndpointConfig *out_ep = nullptr;
    const GpioEndpointConfig *in_ep = nullptr;
    for (const auto &ep : cfg.gpio_endpoints)
    {
        if (ep.name == "led_red")
            out_ep = &ep;
        if (ep.name == "button")
            in_ep = &ep;
    }
    ASSERT_NE(out_ep, nullptr) << "gpio:led_red not found";
    ASSERT_NE(in_ep, nullptr) << "gpio:button not found";

    EXPECT_EQ(out_ep->type, "digital_out");
    EXPECT_EQ(out_ep->pin, "17");
    EXPECT_EQ(out_ep->io_topic, "gpio/led_red");
    EXPECT_EQ(out_ep->default_value, "0");
    EXPECT_TRUE(out_ep->retain);
    EXPECT_TRUE(out_ep->retain_explicit);

    EXPECT_EQ(in_ep->name, "button");
    EXPECT_EQ(in_ep->type, "digital_in");
    EXPECT_EQ(in_ep->debounce_cycles, 4);
}

// ─── Default numeric values ───────────────────────────────────────────────────

TEST(ConfigLoader, DefaultValues)
{
    auto path = writeTempIni(kMinimalIni);
    AppConfig cfg;
    ASSERT_TRUE(ConfigLoader::load(path, cfg));

    EXPECT_EQ(cfg.global.poll_interval_ms, 20);
    EXPECT_EQ(cfg.mqtt.port, 1883);
    EXPECT_EQ(cfg.mqtt.qos, 1);
    EXPECT_FALSE(cfg.mqtt.global_retain);
    EXPECT_EQ(cfg.mqtt.heartbeat_interval_ms, 5000);
}

// ─── Nonexistent file fails ──────────────────────────────────────────────────

TEST(ConfigLoader, NonexistentFileFails)
{
    AppConfig cfg;
    EXPECT_FALSE(ConfigLoader::load("/tmp/pio_does_not_exist_xyz.ini", cfg));
}
