#include "gpio_interface.h"
#include <fstream>
#include <iostream>

GPIOInterface::GPIOInterface()
    : gpioBasePath("/sys/class/gpio") {
}

GPIOInterface::~GPIOInterface() {
}

bool GPIOInterface::exportPin(int pin) {
    std::string exportPath = gpioBasePath + "/export";
    std::ofstream exportFile(exportPath);
    
    if (!exportFile.is_open()) {
        std::cerr << "Failed to open GPIO export file for pin " << pin << std::endl;
        return false;
    }
    
    exportFile << pin;
    exportFile.close();
    
    exportedPins[pin] = true;
    return true;
}

bool GPIOInterface::unexportPin(int pin) {
    std::string unexportPath = gpioBasePath + "/unexport";
    std::ofstream unexportFile(unexportPath);
    
    if (!unexportFile.is_open()) {
        std::cerr << "Failed to open GPIO unexport file for pin " << pin << std::endl;
        return false;
    }
    
    unexportFile << pin;
    unexportFile.close();
    
    exportedPins[pin] = false;
    return true;
}

bool GPIOInterface::setDirection(int pin, const std::string& direction) {
    std::string directionPath = gpioBasePath + "/gpio" + std::to_string(pin) + "/direction";
    std::ofstream directionFile(directionPath);
    
    if (!directionFile.is_open()) {
        std::cerr << "Failed to open GPIO direction file for pin " << pin << std::endl;
        return false;
    }
    
    directionFile << direction;
    directionFile.close();
    
    return true;
}

bool GPIOInterface::setValue(int pin, int value) {
    std::string valuePath = gpioBasePath + "/gpio" + std::to_string(pin) + "/value";
    std::ofstream valueFile(valuePath);
    
    if (!valueFile.is_open()) {
        std::cerr << "Failed to open GPIO value file for pin " << pin << std::endl;
        return false;
    }
    
    valueFile << (value ? "1" : "0");
    valueFile.close();
    
    return true;
}

int GPIOInterface::getValue(int pin) {
    std::string valuePath = gpioBasePath + "/gpio" + std::to_string(pin) + "/value";
    std::ifstream valueFile(valuePath);
    
    if (!valueFile.is_open()) {
        std::cerr << "Failed to open GPIO value file for pin " << pin << std::endl;
        return -1;
    }
    
    char value;
    valueFile >> value;
    valueFile.close();
    
    return (value == '1') ? 1 : 0;
}
