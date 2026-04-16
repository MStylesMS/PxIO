#include <iostream>
#include <signal.h>
#include "gpio_interface.h"
#include "mqtt_client.h"

// Global flag for graceful shutdown
volatile bool running = true;

void signalHandler(int signo)
{
    if (signo == SIGINT)
    {
        std::cout << "\nShutdown signal received. Exiting..." << std::endl;
        running = false;
    }
}

int main(int argc, char *argv[])
{
    std::cout << "PIo - Paradox IO" << std::endl;
    std::cout << "GPIO to MQTT Bridge for Raspberry Pi, ESP32, and other devices" << std::endl;
    std::cout << "================================================================\n"
              << std::endl;

    // Set up signal handler for graceful shutdown
    signal(SIGINT, signalHandler);

    // Initialize GPIO interface
    GPIOInterface gpio;
    std::cout << "GPIO interface initialized" << std::endl;

    // Initialize MQTT client
    // TODO: Configuration from environment or config file
    MQTTClient mqtt("localhost", 1883);
    std::cout << "MQTT client initialized" << std::endl;

    // Connect to MQTT broker
    if (!mqtt.connect("pio-service"))
    {
        std::cerr << "Failed to connect to MQTT broker" << std::endl;
        return 1;
    }
    std::cout << "Connected to MQTT broker\n"
              << std::endl;

    // TODO: Set up GPIO pins and MQTT subscriptions
    // Example:
    // gpio.exportPin(17);
    // gpio.setDirection(17, "out");
    // mqtt.subscribe("gpio/17/set", [&gpio](const std::string& topic, const std::string& msg) {
    //     gpio.setValue(17, std::stoi(msg));
    // });

    // Main service loop
    std::cout << "Service running. Press Ctrl+C to exit.\n"
              << std::endl;
    while (running)
    {
        // TODO: Implement main service loop
        // - Monitor GPIO pins
        // - Publish MQTT updates
        // - Handle MQTT messages
        sleep(1);
    }

    // Cleanup
    mqtt.disconnect();
    std::cout << "Shutdown complete" << std::endl;

    return 0;
}
