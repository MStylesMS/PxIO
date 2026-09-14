#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>

#include "shared/version.h"
#include "shared/log/logger.h"
#include "shared/config/config_loader.h"
#include "shared/config/config_schema.h"
#include "shared/model/endpoint_registry.h"
#include "shared/mqtt/paho_transport.h"
#include "shared/command/command_dispatcher.h"
#include "shared/telemetry/telemetry_publisher.h"

#include "platform/linux_rpi/gpio/libgpiod_provider.h"
#include "platform/linux_rpi/gpio/mock_gpio_provider.h"
#include "platform/linux_rpi/i2c/i2c_bus.h"
#include "platform/linux_rpi/i2c/mcp23017.h"
#include "platform/linux_rpi/endpoints/digital_in.h"
#include "platform/linux_rpi/endpoints/digital_out.h"
#include "platform/linux_rpi/endpoints/keypad_matrix.h"
#include "platform/linux_rpi/endpoints/patch_map.h"

// ─── signal handling ──────────────────────────────────────────────────────────

static volatile std::sig_atomic_t g_running = 1;
static void onSignal(int) { g_running = 0; }

// ─── topic resolution ─────────────────────────────────────────────────────────

static std::string resolveTopic(const std::string &base, const std::string &io)
{
    if (!io.empty() && io.front() == '/')
        return io.substr(1);
    return base + "/" + io;
}

// ─── endpoint factory ─────────────────────────────────────────────────────────

// Build the GpioProvider map: bus name → provider.  Native GPIO uses the
// shared default provider.  MCP23017 expanders get their own Mcp23017 instance.
static std::unordered_map<std::string, std::shared_ptr<GpioProvider>>
buildProviderMap(const AppConfig &cfg,
                 std::shared_ptr<GpioProvider> native_gpio)
{
    std::unordered_map<std::string, std::shared_ptr<GpioProvider>> map;
    map[""] = native_gpio; // "" = native GPIO

    for (const auto &bus : cfg.buses)
    {
        if (bus.type == "mcp23017")
        {
            // Find parent i2c bus device path
            std::string device;
            for (const auto &b : cfg.buses)
            {
                if ("bus:" + b.name == bus.parent || b.name == bus.parent)
                {
                    device = b.device;
                    break;
                }
            }
            if (device.empty())
            {
                LOG_WARN("main: cannot find parent bus for " + bus.name + " — skipped");
                continue;
            }
            auto expander = std::make_shared<Mcp23017>(device,
                                                       static_cast<uint8_t>(bus.address));
            if (!expander->openChip(""))
            {
                LOG_WARN("main: MCP23017 " + bus.name + " failed to open — skipped");
                continue;
            }
            map[bus.name] = expander;
            LOG_INFO("main: registered expander provider " + bus.name);
        }
    }
    return map;
}

// Select provider for a pin spec; returns nullptr on failure.
static std::shared_ptr<GpioProvider> providerForPin(
    const std::string &pin_str,
    const std::unordered_map<std::string, std::shared_ptr<GpioProvider>> &providers)
{
    auto colon = pin_str.find(':');
    if (colon == std::string::npos)
        return providers.at("");
    std::string bus_name = pin_str.substr(0, colon);
    auto it = providers.find(bus_name);
    if (it == providers.end())
    {
        LOG_WARN("main: no provider found for bus '" + bus_name + "'");
        return nullptr;
    }
    return it->second;
}

// All pin specs in a compound must share one bus (native or one expander).
static std::shared_ptr<GpioProvider> providerForPinList(
    const std::vector<std::string> &specs,
    const std::unordered_map<std::string, std::shared_ptr<GpioProvider>> &providers,
    const std::string &ctx)
{
    std::string bus;
    for (const auto &s : specs)
    {
        auto colon = s.find(':');
        std::string b = (colon == std::string::npos) ? std::string() : s.substr(0, colon);
        if (bus.empty() && !b.empty())
            bus = b;
        else if (!b.empty() && b != bus)
        {
            LOG_WARN("main: " + ctx + " mixes buses '" + bus + "' and '" + b + "' — skipped");
            return nullptr;
        }
    }
    if (bus.empty())
        return providers.count("") ? providers.at("") : nullptr;
    auto it = providers.find(bus);
    if (it == providers.end())
    {
        LOG_WARN("main: no provider found for bus '" + bus + "' (" + ctx + ")");
        return nullptr;
    }
    return it->second;
}

// Populate the EndpointRegistry from config. Skips endpoints that fail init.
static void buildRegistry(
    const AppConfig &cfg,
    const std::unordered_map<std::string, std::shared_ptr<GpioProvider>> &providers,
    EndpointRegistry &registry)
{

    // GPIO endpoints
    for (const auto &gcfg : cfg.gpio_endpoints)
    {
        auto provider = providerForPin(gcfg.pin, providers);
        if (!provider)
        {
            LOG_WARN("main: skipping gpio:" + gcfg.name + " — no provider for pin " + gcfg.pin);
            continue;
        }

        std::shared_ptr<Endpoint> ep;
        if (gcfg.type == "digital_in")
        {
            ep = std::make_shared<DigitalIn>(gcfg, provider,
                                             cfg.mqtt.global_retain);
        }
        else if (gcfg.type == "digital_out")
        {
            ep = std::make_shared<DigitalOut>(gcfg, provider,
                                              cfg.mqtt.global_retain);
        }
        else
        {
            LOG_WARN("main: unsupported gpio type '" + gcfg.type + "' on gpio:" + gcfg.name + " — skipped");
            continue;
        }

        if (!ep->init())
        {
            LOG_WARN("main: endpoint gpio:" + gcfg.name + " failed init — skipped (endpoint_unavailable)");
            continue;
        }

        ep->setResolvedTopic(resolveTopic(cfg.mqtt.base_topic, gcfg.io_topic));

        registry.add("gpio:" + gcfg.name, ep);
        LOG_INFO("main: registered gpio:" + gcfg.name + " → " + ep->resolvedTopic());
    }

    // Compound endpoints
    for (const auto &ccfg : cfg.compound_endpoints)
    {
        std::shared_ptr<Endpoint> ep;

        if (ccfg.type == "keypad_matrix")
        {
            std::vector<std::string> specs = ccfg.row_pins;
            specs.insert(specs.end(), ccfg.col_pins.begin(), ccfg.col_pins.end());
            auto provider = providerForPinList(specs, providers, "compound:" + ccfg.name);
            if (!provider)
                continue;
            ep = std::make_shared<KeypadMatrix>(ccfg, provider,
                                                cfg.mqtt.global_retain);
        }
        else if (ccfg.type == "patch_map")
        {
            auto provider = providers.count("") ? providers.at("") : nullptr;
            if (!provider)
                continue;
            ep = std::make_shared<PatchMap>(ccfg, provider,
                                            cfg.mqtt.global_retain);
        }
        else
        {
            LOG_WARN("main: unsupported compound type '" + ccfg.type + "' on compound:" + ccfg.name + " — skipped");
            continue;
        }

        if (!ep->init())
        {
            LOG_WARN("main: endpoint compound:" + ccfg.name + " failed init — skipped");
            continue;
        }

        ep->setResolvedTopic(resolveTopic(cfg.mqtt.base_topic, ccfg.io_topic));

        registry.add("compound:" + ccfg.name, ep);
        LOG_INFO("main: registered compound:" + ccfg.name + " → " + ep->resolvedTopic());
    }
}

// ─── CLI parsing ──────────────────────────────────────────────────────────────

struct CliArgs
{
    std::string config_path = "pxio.ini";
    std::string log_file; // empty = not set via CLI
    bool use_mock_gpio = false;
};

static CliArgs parseCli(int argc, char *argv[])
{
    CliArgs args;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if ((a == "--config" || a == "-c") && i + 1 < argc)
            args.config_path = argv[++i];
        else if (a == "--log-file" && i + 1 < argc)
            args.log_file = argv[++i];
        else if (a == "--mock-gpio")
            args.use_mock_gpio = true;
    }
    return args;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    CliArgs cli = parseCli(argc, argv);

    // Bootstrap logger (file sink applied after config load if needed)
    LOG_INFO("PxIO v" + std::string(PXIO_VERSION) + " starting");

    // Load config
    AppConfig cfg;
    if (!ConfigLoader::load(cli.config_path, cfg))
    {
        LOG_ERROR("Fatal: config load failed, exiting");
        return 1;
    }

    // Apply log settings (CLI overrides INI)
    Logger::instance().setLevel(cfg.global.log_level);
    std::string log_file = cli.log_file.empty() ? cfg.global.log_file : cli.log_file;
    if (!log_file.empty())
        Logger::instance().setLogFile(log_file);

    LOG_INFO("Config loaded: " + cfg.config_file_path);
    LOG_INFO("Instance: " + cfg.global.instance_name);

    // GPIO provider
    std::shared_ptr<GpioProvider> native_gpio;
#ifdef PXIO_GPIO_MOCK
    LOG_INFO("main: using MockGpioProvider");
    auto mock_prov = std::make_shared<MockGpioProvider>();
    mock_prov->openChip("/dev/gpiochip0");
    native_gpio = mock_prov;
#else
    if (cli.use_mock_gpio)
    {
        auto mock_prov = std::make_shared<MockGpioProvider>();
        mock_prov->openChip("/dev/gpiochip0");
        native_gpio = mock_prov;
        LOG_INFO("main: using MockGpioProvider (--mock-gpio)");
    }
    else
    {
        auto real_prov = std::make_shared<LibgpiodProvider>();
        if (!real_prov->openChip("/dev/gpiochip0"))
        {
            LOG_ERROR("Fatal: cannot open /dev/gpiochip0");
            return 1;
        }
        native_gpio = real_prov;
    }
#endif

    // Build provider map (includes I2C expanders)
    auto provider_map = buildProviderMap(cfg, native_gpio);

    // MQTT transport
    auto transport = std::make_shared<PahoTransport>(cfg.mqtt);
    if (!transport->connect())
    {
        LOG_ERROR("Fatal: cannot connect to MQTT broker " + cfg.mqtt.host);
        return 1;
    }

    // Build endpoint registry
    auto registry = std::make_shared<EndpointRegistry>();
    buildRegistry(cfg, provider_map, *registry);
    LOG_INFO("main: " + std::to_string(registry->size()) + " endpoint(s) active");

    // Telemetry publisher
    auto telemetry = std::make_shared<TelemetryPublisher>(
        transport, cfg.mqtt, cfg.global, registry,
        cfg.config_file_path, PXIO_VERSION);

    // Command dispatcher
    auto dispatcher = std::make_shared<CommandDispatcher>(
        registry, transport, cfg.mqtt, telemetry);

    // reload_config callback — rebuild endpoints in place
    dispatcher->setReloadCallback([&]() -> bool
                                  {
        AppConfig new_cfg;
        if (!ConfigLoader::load(cfg.config_file_path, new_cfg)) return false;
        // Shutdown existing endpoints
        for (auto& ep : registry->all()) ep->shutdown();
        registry->clear();
        cfg = new_cfg;
        buildRegistry(cfg, provider_map, *registry);
        LOG_INFO("main: config reloaded; " + std::to_string(registry->size())
                 + " endpoint(s) active");
        return true; });

    // Subscribe to commands
    transport->subscribe(cfg.mqtt.commands_topic, cfg.mqtt.qos,
                         [&dispatcher](const std::string &topic, const std::string &payload)
                         {
                             dispatcher->onCommandMessage(topic, payload);
                         });

    auto republishIo = [&]()
    {
        telemetry->publishAnnounce();
        telemetry->publishState();
        for (auto &ep : registry->all())
        {
            if (!ep->isAvailable())
                continue;
            const std::string &val = ep->lastValue();
            if (!val.empty())
                transport->publish(ep->resolvedTopic(), val,
                                   cfg.mqtt.qos, ep->retainEnabled());
        }
    };

    republishIo();

    // ── main loop ─────────────────────────────────────────────────────────────
    LOG_INFO("main: bridge running (poll_interval=" + std::to_string(cfg.global.poll_interval_ms) + "ms)");

    bool mqtt_was_connected = transport->isConnected();
    while (g_running)
    {
        const bool mqtt_connected = transport->isConnected();
        if (mqtt_connected && !mqtt_was_connected)
        {
            LOG_INFO("main: MQTT reconnected — republishing IO state");
            republishIo();
        }
        mqtt_was_connected = mqtt_connected;

        for (auto &ep : registry->all())
        {
            if (!ep->isAvailable())
                continue;
            if (ep->pollAndCheckChanged())
            {
                std::string val = ep->lastValue();
                transport->publish(ep->resolvedTopic(), val,
                                   cfg.mqtt.qos, ep->retainEnabled());
                LOG_DEBUG("main: published " + ep->name() + " = " + val);
            }
        }

        telemetry->tick();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(cfg.global.poll_interval_ms));
    }

    // ── shutdown ──────────────────────────────────────────────────────────────
    LOG_INFO("main: shutting down");

    for (auto &ep : registry->all())
        ep->shutdown();
    registry->clear();

    transport->disconnect();
    native_gpio->closeChip();

    LOG_INFO("main: shutdown complete");
    return 0;
}
