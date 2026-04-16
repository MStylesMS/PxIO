cmake_minimum_required(VERSION 3.15)
project(PIo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Generate compile_commands.json for clangd/IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source files
set(SOURCES
    src/main.cpp
    src/gpio_interface.cpp
    src/mqtt_client.cpp
)

# Create executable
add_executable(pio ${SOURCES})

# Include directories
target_include_directories(pio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link pthread (for GPIO and MQTT threading)
target_link_libraries(pio PRIVATE pthread)

# Optionally link paho-mqtt3c when available
# find_package(PahoMqttC REQUIRED)
# target_link_libraries(pio PRIVATE PahoMqttC::PahoMqttC)
