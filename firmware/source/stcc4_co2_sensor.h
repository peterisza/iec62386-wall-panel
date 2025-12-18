#ifndef STCC4_H_
#define STCC4_H_

#include <stdint.h>
#include <stdbool.h>

// I2C cím: 0x64 (ADDR=GND) vagy 0x65 (ADDR=VDD)
#define STCC4_ADDR_GND  0x64
#define STCC4_ADDR_VDD  0x65

typedef struct {
    uint16_t co2_ppm;    // 16-bit, ppm (közvetlen)
    float    temperature_C; // SHT4x formula alapján
    float    humidity_RH;   // SHT4x formula alapján
    uint16_t status_raw; // nyers státusz (Byte9..Byte10)
    bool     crc_ok;     // mindhárom szó CRC-je OK volt
    bool     valid;
} stcc4_data_t;

// Single-shot mérés indítása (végrehajtási idő ~500 ms)
bool stcc4_start_single_shot(uint8_t addr7);

// Eredmények kiolvasása (read_measurement parancs + 12 byte olvasás)
bool stcc4_read_measurement(uint8_t addr7, stcc4_data_t *out);

// Kompenzáció beállítása (RH/T, P)
bool stcc4_set_rht_compensation(uint8_t addr, float temp_c, float rh_percent);
bool stcc4_set_pressure_compensation(uint8_t addr, uint32_t pressure_pa);
bool stcc4_push_compensation(uint8_t addr, float temp_c, float rh_percent, uint32_t pressure_pa);

#endif /* STCC4_H_ */
