// tests/unit/test_command_dispatcher.cpp
// CommandDispatcher unit tests using MockTransport + MockGpioProvider.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "shared/command/command_dispatcher.h"
#include "shared/command/command_schema.h"
#include "shared/model/endpoint_registry.h"
#include "shared/mqtt/mock_transport.h"
#include "shared/telemetry/telemetry_publisher.h"
#include "shared/config/config_schema.h"
#include "platform/linux_rpi/gpio/mock_gpio_provider.h"
#include "platform/linux_rpi/endpoints/digital_out.h"
#include "platform/linux_rpi/endpoints/digital_in.h"

using json = nlohmann::json;

// ─── Fixture ─────────────────────────────────────────────────────────────────

struct DispatcherFixture : ::testing::Test
{
    MqttConfig mqtt_cfg;
    GlobalConfig global_cfg;
    std::shared_ptr<MockTransport> transport;
    std::shared_ptr<EndpointRegistry> registry;
    std::shared_ptr<TelemetryPublisher> telemetry;
    std::shared_ptr<CommandDispatcher> dispatcher;
    std::shared_ptr<MockGpioProvider> gpio;

    void SetUp() override
    {
        mqtt_cfg.host = "localhost";
        mqtt_cfg.client_id = "test";
        mqtt_cfg.base_topic = "pxio/test";
        mqtt_cfg.commands_topic = "pxio/test/commands";
        mqtt_cfg.state_topic = "pxio/test/state";
        mqtt_cfg.events_topic = "pxio/test/events";
        mqtt_cfg.warnings_topic = "pxio/test/warnings";
        mqtt_cfg.announce_topic = "pxio/test/announce";
        mqtt_cfg.heartbeat_topic = "pxio/test/heartbeat";

        transport = std::make_shared<MockTransport>();
        registry = std::make_shared<EndpointRegistry>();
        telemetry = std::make_shared<TelemetryPublisher>(
            transport, mqtt_cfg, global_cfg,
            registry, "test.ini", "0.2.0");

        dispatcher = std::make_shared<CommandDispatcher>(
            registry, transport, mqtt_cfg, telemetry);

        // Register a digital_out endpoint
        gpio = std::make_shared<MockGpioProvider>();
        gpio->openChip("/dev/gpiochip0");

        GpioEndpointConfig ocfg;
        ocfg.name = "led";
        ocfg.type = "digital_out";
        ocfg.pin = "17";
        ocfg.io_topic = "gpio/led";
        ocfg.default_value = "0";

        auto out_ep = std::make_shared<DigitalOut>(ocfg, gpio, false);
        gpio->requestLine(17, PinDirection::OUTPUT);
        out_ep->init();
        out_ep->setResolvedTopic("pxio/test/gpio/led");
        registry->add("gpio:led", out_ep);

        // Register a digital_in endpoint
        GpioEndpointConfig icfg;
        icfg.name = "button";
        icfg.type = "digital_in";
        icfg.pin = "27";
        icfg.io_topic = "gpio/button";

        auto in_ep = std::make_shared<DigitalIn>(icfg, gpio, false);
        gpio->requestLine(27, PinDirection::INPUT);
        in_ep->init();
        in_ep->setResolvedTopic("pxio/test/gpio/button");
        registry->add("gpio:button", in_ep);
    }
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string makeCmd(const std::string &cmd_name,
                           const std::string &target = "",
                           const std::string &value = "",
                           const std::string &req_id = "req1")
{
    json j;
    j["command"] = cmd_name;
    j["request_id"] = req_id;
    if (!target.empty())
        j["target"] = target;
    if (!value.empty())
        j["value"] = value;
    return j.dump();
}

// ─── Invalid JSON emits warning, no command_received ─────────────────────────

TEST_F(DispatcherFixture, InvalidJsonEmitsWarning)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands", "not_json{{{");

    // no command_received published
    auto evt = transport->lastPublish(mqtt_cfg.events_topic);
    EXPECT_FALSE(evt.has_value());

    // warning published
    auto warn = transport->lastPublish(mqtt_cfg.warnings_topic);
    ASSERT_TRUE(warn.has_value());
    auto wj = json::parse(warn->payload);
    EXPECT_EQ(wj["code"], WarningCodes::INVALID_MESSAGE_FORMAT);
}

// ─── Valid envelope emits command_received ────────────────────────────────────

TEST_F(DispatcherFixture, ValidEnvelopeEmitsCommandReceived)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands",
                                 makeCmd("write", "gpio:led", "1"));

    // command_received must be the first event
    auto pubs = transport->published();
    bool found = false;
    for (auto &p : pubs)
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_RECEIVED)
            {
                found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(found) << "command_received not found in published messages";
}

// ─── Write to digital_out succeeds ───────────────────────────────────────────

TEST_F(DispatcherFixture, WriteToOutputSucceeds)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands",
                                 makeCmd("write", "gpio:led", "1"));

    // command_success must be published
    bool success_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_SUCCESS)
            {
                success_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(success_found);

    // GPIO pin 17 should have been written with value 1
    EXPECT_EQ(gpio->lastWrittenValue(17), 1);
}

// ─── Write to digital_in emits command_warning with invalid_mode ─────────────

TEST_F(DispatcherFixture, WriteToInputEmitsWarning)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands",
                                 makeCmd("write", "gpio:button", "1"));

    // Events topic: command_warning event emitted
    bool warning_event_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_WARNING)
            {
                warning_event_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(warning_event_found);

    // Warnings topic: code = INVALID_MODE
    bool code_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.warnings_topic)
        {
            auto wj = json::parse(p.payload);
            if (wj.value("code", "") == std::string(WarningCodes::INVALID_MODE))
            {
                code_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(code_found) << "INVALID_MODE warning code not found on warnings_topic";
}

// ─── Unknown target emits command_failed ─────────────────────────────────────

TEST_F(DispatcherFixture, UnknownTargetFails)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands",
                                 makeCmd("write", "gpio:does_not_exist", "1"));

    // Events topic: COMMAND_FAILED
    bool failed_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_FAILED)
            {
                failed_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(failed_found);

    // Warnings topic: code = INVALID_TARGET
    bool code_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.warnings_topic)
        {
            auto wj = json::parse(p.payload);
            if (wj.value("code", "") == std::string(WarningCodes::INVALID_TARGET))
            {
                code_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(code_found) << "INVALID_TARGET warning code not found on warnings_topic";
}

// ─── Ping returns command_success ────────────────────────────────────────────

TEST_F(DispatcherFixture, PingSucceeds)
{
    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands", makeCmd("ping"));

    bool success = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_SUCCESS && ej["command"] == "ping")
            {
                success = true;
                break;
            }
        }
    }
    EXPECT_TRUE(success);
}

// ─── reload_config: success path ─────────────────────────────────────────────

TEST_F(DispatcherFixture, ReloadConfigSuccess)
{
    bool callback_called = false;
    dispatcher->setReloadCallback([&]() -> bool
                                  {
        callback_called = true;
        return true; });

    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands", makeCmd("reload_config"));

    EXPECT_TRUE(callback_called);

    bool success = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_SUCCESS)
            {
                success = true;
                break;
            }
        }
    }
    EXPECT_TRUE(success);
}

// ─── reload_config: failure path → command_warning (not command_failed) ──────

TEST_F(DispatcherFixture, ReloadConfigFailure)
{
    dispatcher->setReloadCallback([]() -> bool
                                  { return false; });

    transport->clearPublished();
    dispatcher->onCommandMessage("pxio/test/commands", makeCmd("reload_config"));

    // Dispatcher emits COMMAND_WARNING (not COMMAND_FAILED) when reload cb
    // returns false, so the bridge continues with the current config.
    bool warning_found = false;
    for (auto &p : transport->published())
    {
        if (p.topic == mqtt_cfg.events_topic)
        {
            auto ej = json::parse(p.payload);
            if (ej["event"] == EventNames::COMMAND_WARNING)
            {
                warning_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(warning_found) << "Expected COMMAND_WARNING for failed reload";
}
