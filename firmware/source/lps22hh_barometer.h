#ifndef LPS22HH_H_
#define LPS22HH_H_

#include <stdint.h>
#include <stdbool.h>

#define LPS22HH_ADDR_SA0_GND  0x5C
#define LPS22HH_ADDR_SA0_VDD  0x5D

#define LPS22HH_REG_WHO_AM_I      0x0F
#define LPS22HH_REG_CTRL1         0x10
#define LPS22HH_REG_CTRL2         0x11
#define LPS22HH_REG_STATUS        0x27
#define LPS22HH_REG_PRESS_OUT_XL  0x28
#define LPS22HH_REG_TEMP_OUT_L    0x2B

#define LPS22HH_CTRL1_BDU         (1u<<1)
#define LPS22HH_CTRL2_ONE_SHOT    (1u<<0)
#define LPS22HH_CTRL2_IF_ADD_INC  (1u<<4)   // I2C auto‑increment

#define LPS22HH_WHO_AM_I_VAL      0xB3

typedef struct {
    float pressure_hPa;
    float temperature_C;
    bool  p_ready;
    bool  t_ready;
} lps22hh_data_t;

bool lps22hh_detect_address(uint8_t *addr_out);     // WHO_AM_I alapján 0x5C/0x5D
bool lps22hh_start_one_shot(uint8_t addr7);         // ONE_SHOT indítás
bool lps22hh_read_results(uint8_t addr7, lps22hh_data_t *out); // blokkos kiolvasás

#endif /* LPS22HH_H_ */
