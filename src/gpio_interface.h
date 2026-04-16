#ifndef GPIO_INTERFACE_H
#define GPIO_INTERFACE_H

#include <string>
#include <map>

class GPIOInterface {
public:
    GPIOInterface();
    ~GPIOInterface();

    // GPIO operations
    bool exportPin(int pin);
    bool unexportPin(int pin);
    bool setDirection(int pin, const std::string& direction); // "in" or "out"
    bool setValue(int pin, int value); // 0 or 1
    int getValue(int pin);

private:
    std::string gpioBasePath;
    std::map<int, bool> exportedPins;
};

#endif // GPIO_INTERFACE_H
