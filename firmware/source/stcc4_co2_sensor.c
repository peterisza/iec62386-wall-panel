#include "stcc4_co2_sensor.h"
#include "i2c.h"
#include "uart.h"
#include <math.h>
#include <string.h>

// STCC4 parancsok
#define STCC4_CMD_MEASURE_SINGLE_SHOT   0x219D
#define STCC4_CMD_READ_MEASUREMENT      0xEC05

// Sensirion CRC-8: polinom 0x31, init 0xFF (adatpárokra)
static uint8_t crc8_sensirion(uint8_t b0, uint8_t b1)
{
    uint8_t crc = 0xFF;
    crc ^= b0;
    for (int i=0; i<8; i++) crc = (crc & 0x80) ? (crc<<1)^0x31 : (crc<<1);
    crc ^= b1;
    for (int i=0; i<8; i++) crc = (crc & 0x80) ? (crc<<1)^0x31 : (crc<<1);
    return crc;
}

bool stcc4_start_single_shot(uint8_t addr7)
{
    uint8_t cmd[2] = {
        (uint8_t)((STCC4_CMD_MEASURE_SINGLE_SHOT >> 8) & 0xFF),
        (uint8_t)( STCC4_CMD_MEASURE_SINGLE_SHOT       & 0xFF)
    };
    // Egyszerű kétbyte-os write (parancsban a 3-bit CRC be van égetve)
    return i2c_write(addr7, cmd, 2, I2C_TIMEOUT_SHORT);
}

bool stcc4_read_measurement(uint8_t addr7, stcc4_data_t *out)
{
    if (!out) return false;
    // Kiolvasási parancs
    uint8_t cmd[2] = {
        (uint8_t)((STCC4_CMD_READ_MEASUREMENT >> 8) & 0xFF),
        (uint8_t)( STCC4_CMD_READ_MEASUREMENT       & 0xFF)
    };
    if (!i2c_write(addr7, cmd, 2, I2C_TIMEOUT_SHORT)) return false;

    // A szenzor 1 ms alatt készíti elő a mérési puffer olvasását
    // (itt nem várunk külön – a legtöbb esetben single-shot után amúgy is sok idő eltelt)

    uint8_t buf[12] = {0};
    if (!i2c_read(addr7, buf, sizeof(buf), I2C_TIMEOUT_LONG)) return false;

    // Feldolgozás: [CO2 msb][CO2 lsb][crc] [T msb][T lsb][crc] [RH msb][RH lsb][crc] [STAT msb][STAT lsb][crc]
    uint16_t co2   = ((uint16_t)buf[0] << 8) | buf[1];
    uint8_t  ccrc  = buf[2];
    uint16_t traw  = ((uint16_t)buf[3] << 8) | buf[4];
    uint8_t  tcrc  = buf[5];
    uint16_t rhraw = ((uint16_t)buf[6] << 8) | buf[7];
    uint8_t  rhcrc = buf[8];
    uint16_t stat  = ((uint16_t)buf[9] << 8) | buf[10];
    uint8_t  scrc  = buf[11];

    bool okc = (crc8_sensirion(buf[0], buf[1]) == ccrc);
    bool okt = (crc8_sensirion(buf[3], buf[4]) == tcrc);
    bool okh = (crc8_sensirion(buf[6], buf[7]) == rhcrc);
    bool oks = (crc8_sensirion(buf[9], buf[10]) == scrc);

    out->crc_ok   = okc && okt && okh && oks;
    out->co2_ppm  = co2;
    out->status_raw = stat;

    // Konverzió a datasheet képletei szerint (SHT4x formátum)
    out->temperature_C = 175.0f * ((float)traw / 65535.0f) - 45.0f;
    out->humidity_RH   = 125.0f * ((float)rhraw / 65535.0f) - 6.0f;

    return true;
}


// ---- Konverziók (adatlap 3.5, Table 11) ----
static uint16_t conv_temp_input(float t_c)
{
    float x = (t_c + 45.0f) * 65535.0f / 175.0f;        // ((T+45)*(2^16-1))/175
    if (x < 0) x = 0; 
    if (x > 65535.0f) x = 65535.0f;
    // Round to nearest integer (manual rounding if lroundf not available)
    return (uint16_t)(x + 0.5f);
}

static uint16_t conv_rh_input(float rh)
{
    float x = (rh + 6.0f) * 65535.0f / 125.0f;          // ((RH+6)*(2^16-1))/125
    if (x < 0) x = 0; 
    if (x > 65535.0f) x = 65535.0f;
    // Round to nearest integer (manual rounding if lroundf not available)
    return (uint16_t)(x + 0.5f);
}

static uint16_t conv_pressure_input(uint32_t pa)
{
    // Tartomány: 40 kPa … 110 kPa (40000 … 110000 Pa)
    // 16-bit unsigned: 0 … 65535
    // P/2 konverzió: 20000 … 55000 (belefér 16-bit-be)
    // Megjegyzés: Ellenőrizd a datasheet Table 11-et, hogy valóban P/2 kell-e!
    if (pa < 40000U)   pa = 40000U;
    if (pa > 110000U)  pa = 110000U;
    return (uint16_t)(pa / 2U);                         // P/2 → u16
}

bool stcc4_set_rht_compensation(uint8_t addr, float temp_c, float rh_percent)
{
    const uint16_t t_raw  = conv_temp_input(temp_c);
    const uint16_t rh_raw = conv_rh_input(rh_percent);

    uint8_t cmd[2] = { 0xE0, 0x00 };                    // set_rht_compensation
    uint8_t payload[6];
    payload[0] = (uint8_t)(t_raw >> 8);
    payload[1] = (uint8_t)(t_raw & 0xFF);
    payload[2] = crc8_sensirion(payload[0], payload[1]);
    payload[3] = (uint8_t)(rh_raw >> 8);
    payload[4] = (uint8_t)(rh_raw & 0xFF);
    payload[5] = crc8_sensirion(payload[3], payload[4]);

    // egy tranzakcióban: CMD(2) + data(6)
    uint8_t buf[8];
    memcpy(buf, cmd, 2);
    memcpy(buf + 2, payload, 6);
    return i2c_write(addr, buf, sizeof(buf), I2C_TIMEOUT_SHORT);
}

bool stcc4_set_pressure_compensation(uint8_t addr, uint32_t pressure_pa)
{
    const uint16_t p_raw = conv_pressure_input(pressure_pa);
    uint8_t buf[5];
    buf[0] = 0xE0; buf[1] = 0x16;                       // set_pressure_compensation
    buf[2] = (uint8_t)(p_raw >> 8);
    buf[3] = (uint8_t)(p_raw & 0xFF);
    buf[4] = crc8_sensirion(buf[2], buf[3]);
    return i2c_write(addr, buf, sizeof(buf), I2C_TIMEOUT_SHORT);
}

bool stcc4_push_compensation(uint8_t addr, float temp_c, float rh_percent, uint32_t pressure_pa)
{
    // RH/T → 1 ms, P → 1 ms (adatlap); egymás után simán kiadható. 
    if (!stcc4_set_rht_compensation(addr, temp_c, rh_percent)) return false;
    return stcc4_set_pressure_compensation(addr, pressure_pa);
}