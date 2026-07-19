cmake_minimum_required(VERSION 3.15)
project(PxIO VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────────────────────
# Set to "mock" to build with MockGpioProvider instead of LibgpiodProvider.
set(PXIO_GPIO_BACKEND "libgpiod" CACHE STRING "GPIO backend: libgpiod or mock")
option(PXIO_BUILD_TESTS "Build unit and integration tests" OFF)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

# libgpiod v2 (Trixie baseline)
pkg_check_modules(GPIOD REQUIRED libgpiod>=2.0)

# Paho MQTT C async — use upstream CMake package if available
find_package(eclipse-paho-mqtt-c REQUIRED)
set(PAHO_MQTT_TARGET eclipse-paho-mqtt-c::paho-mqtt3a)
find_path(PAHO_MQTT_INCLUDE_DIR NAMES MQTTAsync.h
          HINTS /usr/include /usr/local/include
          REQUIRED)

# nlohmann/json (header-only, Trixie: nlohmann-json3-dev)
find_package(nlohmann_json 3.0 QUIET)
if(NOT nlohmann_json_FOUND)
    # fallback: pkg-config
    pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json>=3.0)
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SHARED_SOURCES
    src/shared/log/logger.cpp
    src/shared/config/config_loader.cpp
    src/shared/model/endpoint_registry.cpp
    src/shared/mqtt/paho_transport.cpp
    src/shared/mqtt/mock_transport.cpp
    src/shared/command/command_dispatcher.cpp
    src/shared/telemetry/telemetry_publisher.cpp
)

set(PLATFORM_SOURCES
    src/platform/linux_rpi/gpio/libgpiod_provider.cpp
    src/platform/linux_rpi/gpio/mock_gpio_provider.cpp
    src/platform/linux_rpi/i2c/i2c_bus.cpp
    src/platform/linux_rpi/i2c/mcp23017.cpp
    src/platform/linux_rpi/endpoints/digital_in.cpp
    src/platform/linux_rpi/endpoints/digital_out.cpp
    src/platform/linux_rpi/endpoints/keypad_matrix.cpp
    src/platform/linux_rpi/endpoints/patch_map.cpp
)

# ── Binary ────────────────────────────────────────────────────────────────────
add_executable(pxio
    ${SHARED_SOURCES}
    ${PLATFORM_SOURCES}
    src/platform/linux_rpi/main_rpi.cpp
)

target_include_directories(pxio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GPIOD_INCLUDE_DIRS}
    ${PAHO_MQTT_INCLUDE_DIR}
)

if(nlohmann_json_FOUND)
    target_link_libraries(pxio PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(pxio PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

target_link_libraries(pxio PRIVATE
    pthread
    ${GPIOD_LIBRARIES}
    ${PAHO_MQTT_TARGET}
)

# Mock GPIO backend override
if(PXIO_GPIO_BACKEND STREQUAL "mock")
    target_compile_definitions(pxio PRIVATE PXIO_GPIO_MOCK=1)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(PXIO_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    set(TEST_SHARED_SOURCES
        ${SHARED_SOURCES}
        ${PLATFORM_SOURCES}     # tests use mock providers, platform code compiled in
    )

    # Helper to register a test target
    function(pxio_test name src)
        add_executable(${name} ${src} ${TEST_SHARED_SOURCES})
        target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${GPIOD_INCLUDE_DIRS}
            ${PAHO_MQTT_INCLUDE_DIR}
        )
        if(nlohmann_json_FOUND)
            target_link_libraries(${name} PRIVATE nlohmann_json::nlohmann_json)
        else()
            target_include_directories(${name} PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
        endif()
        target_link_libraries(${name} PRIVATE
            pthread
            ${GPIOD_LIBRARIES}
            ${PAHO_MQTT_TARGET}
            GTest::GTest
            GTest::Main
        )
        target_compile_definitions(${name} PRIVATE PXIO_GPIO_MOCK=1)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    pxio_test(test_config_loader    tests/unit/test_config_loader.cpp)
    pxio_test(test_command_dispatcher tests/unit/test_command_dispatcher.cpp)
    pxio_test(test_topic_resolution tests/unit/test_topic_resolution.cpp)
    pxio_test(test_patch_map        tests/unit/test_patch_map_canonicalize.cpp)
    pxio_test(test_mqtt_roundtrip   tests/integration/test_mqtt_roundtrip.cpp)
endif()
