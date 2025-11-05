#include "ens210_temperature_sensor.h"
#include "i2c.h"

// CRC-7 a 17 bites payloadra (polinom 0x89, init 0x7F) – lásd datasheet
static uint8_t ens210_crc7(uint32_t val)
{
    const uint8_t poly = 0x89;
    val <<= 7;                 // hely CRC-nek
    val |= 0x7F;               // init vector
    uint32_t pol = ((uint32_t)poly) << (17 - 7 - 1);
    uint32_t bit = (((uint32_t)1) << 16);
    while (bit & (0x1FFFFu)) {
        if (val & bit) val ^= pol;
        bit >>= 1;
        pol >>= 1;
    }
    return (uint8_t)(val & 0x7Fu);
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
