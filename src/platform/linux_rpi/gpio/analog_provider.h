#pragma once

// Phase 2 stub — AnalogProvider defines the interface contract for ADC, DAC,
// and PWM so that phase 2 endpoint types can be added without restructuring
// the platform layer.
//
// PHASE 1 RULE: No implementation of this interface ships. The header exists
// solely to establish the API boundary. Implementations (LinuxAnalogProvider,
// MockAnalogProvider) are added in phase 2.

class AnalogProvider {
public:
    virtual ~AnalogProvider() = default;

    // ADC — read a normalised value in [0.0, 1.0] from channel.
    virtual bool readScaled(int channel, double& value_out) = 0;

    // DAC — write a normalised value in [0.0, 1.0] to channel.
    virtual bool writeScaled(int channel, double value) = 0;

    // PWM — set duty cycle [0.0, 1.0] and frequency in Hz on channel.
    virtual bool setPwm(int channel, double duty_cycle, double frequency_hz) = 0;
};
