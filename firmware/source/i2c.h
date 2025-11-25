#ifndef I2C_H_
#define I2C_H_

#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

void i2c_init(uint32_t smclk_hz, uint32_t bus_hz);

// Alap write/read
bool i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_loops);
bool i2c_read (uint8_t addr7,       uint8_t *data, uint16_t len, uint32_t timeout_loops);

// Regiszteres write/read
bool i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len, uint32_t timeout_loops);
bool i2c_read_reg (uint8_t addr7, uint8_t reg,       uint8_t *data, uint16_t len, uint32_t timeout_loops);
bool i2c_read_reg1(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t timeout_loops);

// Hasznos kiegészítők
bool i2c_bus_busy(void);                                   // UCBUSY állapot
bool i2c_recover_bus_gpio(void);                           // GPIO‑val 9 SCL pulzus + STOP
void i2c_force_reset(void);                                // eUSCI_B0 gyors reset

#define I2C_TIMEOUT_SHORT  (20000u)
#define I2C_TIMEOUT_LONG   (60000u)

#endif /* I2C_H_ */
