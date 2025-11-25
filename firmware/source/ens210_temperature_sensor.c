#include "ens210_temperature_sensor.h"
#include "i2c.h"
#include "uart.h"

// CRC-7 a 17 bites payloadra (polinom 0x89, init 0x7F) – lásd datasheet
// ENS210 CRC-7: polinom 0x89 (x^7 + x^3 + x^0), init 0x7F
// Reference: ENS210 datasheet - CRC-7 calculation algorithm
// Based on official ENS210 datasheet reference implementation
static uint8_t ens210_crc7(uint32_t val)
{
    const uint8_t poly = 0x89;      // 0b10001001 = x^7 + x^3 + x^0
    const uint8_t init = 0x7F;       // init vector
    const uint8_t data_width = 17;   // 17 bit payload
    const uint8_t crc_width = 7;     // CRC-7
    const uint32_t data_mask = ((uint32_t)1 << data_width) - 1;  // 0x1FFFF
    const uint32_t data_msb = (uint32_t)1 << (data_width - 1);   // 0x10000
    
    // Initialize polynomial and bit mask
    uint32_t pol = (uint32_t)poly << (data_width - crc_width - 1);  // 0x89 << 9 = 0x12400
    uint32_t bit = data_msb;  // 0x10000
    
    // Shift payload left by CRC width to make room for CRC
    val = val << crc_width;   // val << 7
    bit = bit << crc_width;   // bit << 7 = 0x800000
    pol = pol << crc_width;   // pol << 7
    val |= init;              // OR in initial value
    
    // Process bits from MSB down to CRC position
    while (bit & (data_mask << crc_width)) {  // while bit & 0xFFFF80
        if (bit & val) {
            val ^= pol;
        }
        bit >>= 1;
        pol >>= 1;
    }
    
    // Return the 7-bit CRC (bits 6:0)
    return (uint8_t)(val & 0x7F);
}

bool ens210_start_single_shot(uint8_t which)
{
    uint8_t val = which & (ENS210_START_T | ENS210_START_H);
    if (val == 0) return false;
    // single-shot mód az alapértelmezett; csak SENS_START-ot írunk
    return i2c_write_reg(ENS210_I2C_ADDR, ENS210_REG_SENS_START, &val, 1, I2C_TIMEOUT_SHORT);
}

static bool read_val24(uint8_t reg, uint32_t *val24)
{
    uint8_t buf[3];
    if (!i2c_read_reg(ENS210_I2C_ADDR, reg, buf, 3, I2C_TIMEOUT_SHORT))
        return false;

    // little endian 3 byte -> 24 bites érték (pl. 0xFD 0x49 0x0B -> 0x0B49FD)
    *val24 = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[1] << 8) | buf[0];
    return true;
}

bool ens210_read_results(ens210_data_t *out)
{
    if (!out) return false;

    uint32_t t_val24=0, h_val24=0;
    bool ok_t = read_val24(ENS210_REG_T_VAL, &t_val24);
    bool ok_h = read_val24(ENS210_REG_H_VAL, &h_val24);

    out->t_valid_crc_ok = false;
    out->h_valid_crc_ok = false;
    out->temperature_C  = 0.0f;
    out->humidity_RH    = 0.0f;

    if (ok_t) {
        uint16_t t_data  =  (uint16_t)( t_val24        & 0xFFFFu);
        uint8_t  t_valid = (uint8_t) ((t_val24 >> 16) & 0x01u);
        uint8_t  t_crc   = (uint8_t) ((t_val24 >> 17) & 0x7Fu);

        // CRC a (VALID<<16 | DATA) 17 bites payload-ra
        uint32_t payload = ((uint32_t)t_valid << 16) | t_data;
        uint8_t crc_calc = ens210_crc7(payload);
        float t_K = ((float)t_data) / 64.0f;            // 1/64 Kelvin
        out->temperature_C = t_K - 273.15f;
        out->t_valid_crc_ok = (t_valid == 1u) && (t_crc == crc_calc);
    }
    if (ok_h) {
        uint16_t h_data  =  (uint16_t)( h_val24        & 0xFFFFu);
        uint8_t  h_valid = (uint8_t) ((h_val24 >> 16) & 0x01u);
        uint8_t  h_crc   = (uint8_t) ((h_val24 >> 17) & 0x7Fu);

        uint32_t payload = ((uint32_t)h_valid << 16) | h_data;
        uint8_t crc_calc = ens210_crc7(payload);
        out->humidity_RH = ((float)h_data) / 512.0f;    // 1/512 %RH
        out->h_valid_crc_ok = (h_valid == 1u) && (h_crc == crc_calc);
    }

    return ok_t || ok_h;
}
