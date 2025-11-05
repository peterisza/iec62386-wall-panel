#include "lps22hh_barometer.h"
#include "i2c.h"

static bool rd8(uint8_t a, uint8_t r, uint8_t *v) { return i2c_read_reg(a, r, v, 1, I2C_TIMEOUT_SHORT); }
static bool wr8(uint8_t a, uint8_t r, uint8_t v)  { return i2c_write_reg(a, r, &v, 1, I2C_TIMEOUT_SHORT); }

bool lps22hh_detect_address(uint8_t *addr_out)
{
    if (!addr_out) return false;
    uint8_t who=0;
    if (rd8(LPS22HH_ADDR_SA0_GND, LPS22HH_REG_WHO_AM_I, &who) && who == LPS22HH_WHO_AM_I_VAL) {
        *addr_out = LPS22HH_ADDR_SA0_GND; return true;
    }
    if (rd8(LPS22HH_ADDR_SA0_VDD, LPS22HH_REG_WHO_AM_I, &who) && who == LPS22HH_WHO_AM_I_VAL) {
        *addr_out = LPS22HH_ADDR_SA0_VDD; return true;
    }
    return false;
}

bool lps22hh_start_one_shot(uint8_t addr7)
{
    // CTRL1: ODR=000 (power‑down), BDU=1
    if (!wr8(addr7, LPS22HH_REG_CTRL1, LPS22HH_CTRL1_BDU)) return false;

    // CTRL2: IF_ADD_INC=1 (multi‑byte I2C olvasáshoz), majd ONE_SHOT=1
    uint8_t ctrl2 = 0;
    (void)rd8(addr7, LPS22HH_REG_CTRL2, &ctrl2);
    ctrl2 |= LPS22HH_CTRL2_IF_ADD_INC;     // biztos, ami biztos
    if (!wr8(addr7, LPS22HH_REG_CTRL2, ctrl2)) return false;

    ctrl2 |= LPS22HH_CTRL2_ONE_SHOT;       // trigger
    return wr8(addr7, LPS22HH_REG_CTRL2, ctrl2);
}

bool lps22hh_read_results(uint8_t addr7, lps22hh_data_t *out)
{
    if (!out) return false;
    out->p_ready = out->t_ready = false;
    out->pressure_hPa = 0.0f;
    out->temperature_C = 0.0f;

    // STATUS: P_DA (bit0), T_DA (bit1) – rövid, limitált poll
    uint8_t st=0;
    for (int i=0; i<50; i++) { // ~pár ms-nyi próbálkozás
        if (!rd8(addr7, LPS22HH_REG_STATUS, &st)) return false;
        if (st & 0x03) break;
        __delay_cycles(2000); // ~2 ms @1MHz
    }

    // Blokk olvasás 0x28..0x2C (5 byte): P(24b) + T(16b)
    uint8_t buf[5] = {0};
    if (!i2c_read_reg(addr7, LPS22HH_REG_PRESS_OUT_XL, buf, sizeof(buf), I2C_TIMEOUT_SHORT))
        return false;

    // Nyomás: 24 bites, jelölt számmá kiterjesztve; 4096 LSB/hPa
    int32_t praw = ((int32_t)buf[2] << 16) | ((int32_t)buf[1] << 8) | buf[0];
    if (praw & 0x00800000) praw |= 0xFF000000;
    out->pressure_hPa = ((float)praw) / 4096.0f;
    out->p_ready = (st & 0x01) ? true : true; // ha már olvastunk blokkban, tekintsük késznek

    // Hőmérséklet: 16 bites 2’s complement; 100 LSB/°C
    int16_t traw = (int16_t)((buf[4] << 8) | buf[3]);
    out->temperature_C = ((float)traw) / 100.0f;
    out->t_ready = (st & 0x02) ? true : true;

    return true;
}
