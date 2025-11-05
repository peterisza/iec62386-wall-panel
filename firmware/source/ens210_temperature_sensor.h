#ifndef ENS210_H_
#define ENS210_H_

#include <stdint.h>
#include <stdbool.h>

#define ENS210_I2C_ADDR   0x43

// Regiszterek
#define ENS210_REG_SYS_CTRL   0x10
#define ENS210_REG_SYS_STAT   0x11
#define ENS210_REG_SENS_RUN   0x21
#define ENS210_REG_SENS_START 0x22
#define ENS210_REG_SENS_STOP  0x23
#define ENS210_REG_SENS_STAT  0x24
#define ENS210_REG_T_VAL      0x30  // 3 byte (DATA[15:0], VALID, CRC7)
#define ENS210_REG_H_VAL      0x33  // 3 byte

// Start bitek
#define ENS210_START_T   (1u << 0)
#define ENS210_START_H   (1u << 1)

typedef struct {
    float temperature_C; // Celsius
    float humidity_RH;   // %
    bool  t_valid_crc_ok;
    bool  h_valid_crc_ok;
} ens210_data_t;

bool ens210_start_single_shot(uint8_t which); // which: ENS210_START_T/H (vagy mindkettő)
bool ens210_read_results(ens210_data_t *out); // kiolvassa T_VAL és H_VAL, konvertál

#endif /* ENS210_H_ */
