#include "i2c_counter.h"
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdexcept>

I2CCounter::I2CCounter(int bus_number, int address) : device_address(address) {
    char filename[20];
    snprintf(filename, sizeof(filename), "/dev/i2c-%d", bus_number);
    
    i2c_fd = open(filename, O_RDWR);
    if (i2c_fd < 0) {
        throw std::runtime_error("Failed to open I2C bus");
    }
    
    if (ioctl(i2c_fd, I2C_SLAVE, address) < 0) {
        close(i2c_fd);
        throw std::runtime_error("Failed to set I2C slave address");
    }
}

I2CCounter::~I2CCounter() {
    if (i2c_fd >= 0) {
        close(i2c_fd);
    }
}

uint32_t I2CCounter::readRegister(uint8_t reg) {
    struct i2c_msg messages[2];
    struct i2c_rdwr_ioctl_data ioctl_data;
    uint8_t reg_addr = reg;
    uint8_t data[4];
    
    messages[0].addr = device_address;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &reg_addr;
    
    messages[1].addr = device_address;
    messages[1].flags = I2C_M_RD;
    messages[1].len = 4;
    messages[1].buf = data;
    
    ioctl_data.msgs = messages;
    ioctl_data.nmsgs = 2;
    
    if (ioctl(i2c_fd, I2C_RDWR, &ioctl_data) < 0) {
        throw std::runtime_error("Failed to read register via I2C_RDWR");
    }
    
    uint32_t value = (static_cast<uint32_t>(data[0]) << 24) |
                     (static_cast<uint32_t>(data[1]) << 16) |
                     (static_cast<uint32_t>(data[2]) << 8) |
                     static_cast<uint32_t>(data[3]);
    
    return value;
}
