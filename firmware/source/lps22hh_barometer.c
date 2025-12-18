#include "lps22hh_barometer.h"
#include "i2c.h"
#include "uart.h"


static bool rd8(uint8_t a, uint8_t r, uint8_t *v) { return i2c_read_reg1(a, r, v, I2C_TIMEOUT_SHORT); }
static bool wr8(uint8_t a, uint8_t r, uint8_t v)  { return i2c_write_reg(a, r, &v, 1, I2C_TIMEOUT_SHORT); }

static bool lps22hh_set_bits(uint8_t addr7, uint8_t reg, uint8_t mask)
{
    uint8_t v;
    if (!rd8(addr7, reg, &v)) return false;
    v |= mask;
    return wr8(addr7, reg, v);
}

bool lps22hh_init(uint8_t addr7)
{
    /*// IF_ADD_INC (bit4) ON
    if (!lps22hh_set_bits(addr7, LPS22HH_REG_CTRL2, (1u<<4))) return false;

    // BDU (bit1) ON
    if (!lps22hh_set_bits(addr7, LPS22HH_REG_CTRL1, (1u<<1))) return false;*/
    
    // Auto-increment bekapcs
    if (!lps22hh_set_bits(addr7, LPS22HH_REG_CTRL2, LPS22HH_CTRL2_IF_ADD_INC)) return false;

    // BDU=1 + ODR=0 (power-down → ONE-SHOT-hoz)
    uint8_t c1;
    if (!rd8(addr7, LPS22HH_REG_CTRL1, &c1)) return false;
    c1 |= LPS22HH_CTRL1_BDU;            // BDU=1
    c1 &= (uint8_t)~LPS22HH_CTRL1_ODR_Mask; // ODR=000
    if (!wr8(addr7, LPS22HH_REG_CTRL1, c1)) return false;
    // Gondoskodj róla, hogy ODR != 0 (Power-downban STATUS nem frissül)
    // Példa: ha 10 Hz kell, az ODR biteket (6:4) beállítod itt vagy korábban.
    return true;
}

bool lps22hh_start_one_shot(uint8_t addr7)
{
    /*uint16_t gie = __get_SR_register() & GIE;
    __bic_SR_register(GIE);*/
    
    bool result = lps22hh_set_bits(addr7, LPS22HH_REG_CTRL2, LPS22HH_CTRL2_ONE_SHOT);
    if(!result){
        uart_send_status_prefix(false);
        uart_send_string("LPS22HH: start one shot failed\r\n");
        return false;
    }
    /*if (gie) {
        __bis_SR_register(GIE);
    }*/
    return result;
}

bool lps22hh_read_results(uint8_t addr7, lps22hh_data_t *out)
{
    if (!out) return false;
    out->p_ready = out->t_ready = false;
    out->pressure_hPa = 0.0f;
    out->temperature_C = 0.0f;

    // STATUS: P_DA (bit0), T_DA (bit1) – rövid, limitált poll
    uint8_t st=0;
    int poll_count = 0;
    bool status_ready = false;
    /*uint16_t gie = __get_SR_register() & GIE;
    __bic_SR_register(GIE);*/
 
    for (poll_count = 0; poll_count < 50; poll_count++) { // ~pár ms-nyi próbálkozás
 
        if (!rd8(addr7, LPS22HH_REG_STATUS, &st)) {
            uart_send_status_prefix(false);
            uart_send_string("LPS22HH: status read failed\r\n");
            return false;
        }
        if (st & 0x03) {
            status_ready = true;
            break;
        }
        __delay_cycles(2000); // ~2 ms @1MHz
    }
    /*if (gie) {
        __bis_SR_register(GIE);
    }*/
    if (!status_ready) {
        uart_send_status_prefix(false);
        uart_send_string("LPS22HH: status timeout\r\n");
    }
    
    // Blokk olvasás 0x28..0x2C (5 byte): P(24b) + T(16b)
    uint8_t buf[5] = {0};
    bool read_ok = i2c_read_reg(addr7, LPS22HH_REG_PRESS_OUT_XL, buf, sizeof(buf), I2C_TIMEOUT_SHORT);

    if (!read_ok) {
        uart_send_status_prefix(false);
        uart_send_string("LPS22HH: data read failed\r\n");
        return false;
    }
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
