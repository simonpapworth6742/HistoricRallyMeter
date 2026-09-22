#ifndef I2C_COUNTER_H
#define I2C_COUNTER_H

#include <cstdint>

class I2CCounter {
private:
    int i2c_fd;
    int device_address;
    
public:
    I2CCounter(int bus_number, int address);
    ~I2CCounter();

    int address() const { return device_address; }
    
    uint32_t readRegister(uint8_t reg);
    uint8_t readRegisterByte(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);

    // SSTR bit 0. Set by the chip when it loses power; CNTR goes back to zero.
    bool powerLost();
    // Clear the power-loss flag. Does not change CNTR.
    void clearPowerLoss();
};

#endif // I2C_COUNTER_H
