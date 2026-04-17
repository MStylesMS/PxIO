// tests/integration/test_mqtt_roundtrip.cpp
// Integration test: command received on commands_topic → endpoint acted on →
// event published on events_topic.  Uses MockTransport + MockGpioProvider.

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

// ─── Setup helpers ───────────────────────────────────────────────────────────

static MqttConfig makeTestMqttConfig() {
    MqttConfig c;
    c.host            = "localhost";
    c.client_id       = "pio_integration";
    c.base_topic      = "pio/integration";
    c.commands_topic  = "pio/integration/commands";
    c.state_topic     = "pio/integration/state";
    c.events_topic    = "pio/integration/events";
    c.warnings_topic  = "pio/integration/warnings";
    c.announce_topic  = "pio/integration/announce";
    c.heartbeat_topic = "pio/integration/heartbeat";
    return c;
}

static std::string makeCmdJson(const std::string& cmd,
                                const std::string& target = "",
                                const std::string& value  = "",
                                const std::string& req_id = "r1") {
    json j;
    j["command"]    = cmd;
    j["request_id"] = req_id;
    if (!target.empty()) j["target"] = target;
    if (!value.empty())  j["value"]  = value;
    return j.dump();
}

// ─── Full write roundtrip ─────────────────────────────────────────────────────

TEST(MqttRoundtrip, WriteCommandChangesGpioAndPublishesSuccess) {
    auto transport = std::make_shared<MockTransport>();
    auto registry  = std::make_shared<EndpointRegistry>();
    auto gpio      = std::make_shared<MockGpioProvider>();
    GlobalConfig global_cfg;
    auto mqtt_cfg = makeTestMqttConfig();

    gpio->openChip("/dev/gpiochip0");

    auto telemetry = std::make_shared<TelemetryPublisher>(
        transport, mqtt_cfg, global_cfg, registry, "test.ini", "0.2.0");
    auto dispatcher = std::make_shared<CommandDispatcher>(
        registry, transport, mqtt_cfg, telemetry);

    // Set up a digital_out endpoint
    GpioEndpointConfig ocfg;
    ocfg.name          = "relay";
    ocfg.type          = "digital_out";
    ocfg.pin           = "18";
    ocfg.io_topic      = "gpio/relay";
    ocfg.default_value = "0";

    auto ep = std::make_shared<DigitalOut>(ocfg, gpio, false);
    gpio->requestLine(18, PinDirection::OUTPUT);
    ep->init();
    ep->setResolvedTopic("pio/integration/gpio/relay");
    registry->add("gpio:relay", ep);

    // Subscribe commands (simulate transport subscription)
    transport->subscribe(mqtt_cfg.commands_topic, 1,
        [&dispatcher, &mqtt_cfg](const std::string& t, const std::string& p) {
            dispatcher->onCommandMessage(t, p);
        });

    transport->clearPublished();

    // Inject a write command through the MockTransport
    transport->inject(mqtt_cfg.commands_topic,
                      makeCmdJson("write", "gpio:relay", "1"));

    // GPIO pin 18 should now be set to 1
    EXPECT_EQ(gpio->lastWrittenValue(18), 1);

    // command_success event should have been published
    bool success_found = false;
    for (auto& pub : transport->published()) {
        if (pub.topic == mqtt_cfg.events_topic) {
            auto ej = json::parse(pub.payload);
            if (ej.value("event", "") == std::string(EventNames::COMMAND_SUCCESS)) {
                success_found = true;
                break;
            }
        }
    }
    EXPECT_TRUE(success_found) << "Expected command_success event not found";
}

// ─── Read command publishes current value to IO topic ────────────────────────

TEST(MqttRoundtrip, ReadCommandPublishesValueToIoTopic) {
    auto transport = std::make_shared<MockTransport>();
    auto registry  = std::make_shared<EndpointRegistry>();
    auto gpio      = std::make_shared<MockGpioProvider>();
    GlobalConfig global_cfg;
    auto mqtt_cfg = makeTestMqttConfig();

    gpio->openChip("/dev/gpiochip0");
    gpio->setPresetValue(22, 1); // simulate pin HIGH

    auto telemetry = std::make_shared<TelemetryPublisher>(
        transport, mqtt_cfg, global_cfg, registry, "test.ini", "0.2.0");
    auto dispatcher = std::make_shared<CommandDispatcher>(
        registry, transport, mqtt_cfg, telemetry);

    GpioEndpointConfig icfg;
    icfg.name     = "sensor";
    icfg.type     = "digital_in";
    icfg.pin      = "22";
    icfg.io_topic = "gpio/sensor";
    icfg.debounce_cycles = 1;

    auto ep = std::make_shared<DigitalIn>(icfg, gpio, false);
    gpio->requestLine(22, PinDirection::INPUT);
    ep->init();
    ep->setResolvedTopic("pio/integration/gpio/sensor");
    registry->add("gpio:sensor", ep);

    transport->subscribe(mqtt_cfg.commands_topic, 1,
        [&dispatcher, &mqtt_cfg](const std::string& t, const std::string& p) {
            dispatcher->onCommandMessage(t, p);
        });

    transport->clearPublished();
    transport->inject(mqtt_cfg.commands_topic,
                      makeCmdJson("read", "gpio:sensor"));

    // A publish to the IO topic should have occurred
    auto io_pub = transport->lastPublish("pio/integration/gpio/sensor");
    ASSERT_TRUE(io_pub.has_value()) << "Expected publish to IO topic for 'read' command";
    // Value should be "1" (preset value HIGH)
    EXPECT_EQ(io_pub->payload, "1");
}

// ─── republish_state publishes to state_topic ────────────────────────────────

TEST(MqttRoundtrip, RepublishStatePublishesToStateTopic) {
    auto transport = std::make_shared<MockTransport>();
    auto registry  = std::make_shared<EndpointRegistry>();
    GlobalConfig global_cfg;
    auto mqtt_cfg = makeTestMqttConfig();

    auto telemetry = std::make_shared<TelemetryPublisher>(
        transport, mqtt_cfg, global_cfg, registry, "test.ini", "0.2.0");
    auto dispatcher = std::make_shared<CommandDispatcher>(
        registry, transport, mqtt_cfg, telemetry);

    transport->subscribe(mqtt_cfg.commands_topic, 1,
        [&dispatcher, &mqtt_cfg](const std::string& t, const std::string& p) {
            dispatcher->onCommandMessage(t, p);
        });

    transport->clearPublished();
    transport->inject(mqtt_cfg.commands_topic,
                      makeCmdJson("republish_state"));

    auto state_pub = transport->lastPublish(mqtt_cfg.state_topic);
    EXPECT_TRUE(state_pub.has_value())
        << "Expected state publish after republish_state command";
}
