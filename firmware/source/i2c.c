#include "i2c.h"
#include "uart.h"
#include <string.h>

// A mi lábkiosztásunk: P1.2 = SDA, P1.3 = SCL
#define I2C_PORT_DIR   P1DIR
#define I2C_PORT_OUT   P1OUT
#define I2C_PORT_IN    P1IN
#define I2C_PORT_REN   P1REN
#define I2C_PORT_SEL0  P1SEL0
#define I2C_PORT_SEL1  P1SEL1
#define I2C_SDA_BIT    BIT2
#define I2C_SCL_BIT    BIT3

static bool wait_while(volatile uint16_t *reg, uint16_t mask, bool while_set, uint32_t to)
{
    while (to > 0) {
        if (while_set) {
            if (((*reg) & mask) == 0) return true;   // akkor jó, ha már NEM set
        } else {
            if (((*reg) & mask) != 0) return true;   // akkor jó, ha MÁR set
        }
        to--;
    }
    return false; // timeout
}

static void force_stop_and_clear(void)
{
    // Mindig próbáljunk STOP‑ot kiadni és a NACK flaget törölni
    UCB0CTLW0 |= UCTXSTP;
    // várunk egy kicsit – ha be volt akadva, ez is segíthet
    (void)wait_while(&UCB0CTLW0, UCTXSTP, true, I2C_TIMEOUT_LONG);
    // NACK flag tisztítás
    UCB0IFG &= ~UCNACKIFG;
}

static void i2c_debug(const char *msg)
{
    uart_send_status_prefix(false);
    uart_send_string("I2C: ");
    uart_send_string(msg);
    uart_send_string("\r\n");
}

static bool wait_while_debug(const char *msg, volatile uint16_t *reg, uint16_t mask, bool while_set, uint32_t to)
{
    if (!wait_while(reg, mask, while_set, to)) {
        if (msg) {
            i2c_debug(msg);
        }
        force_stop_and_clear();
        return false;
    }
    return true;
}

static bool ensure_bus_ready(uint32_t to)
{
    if (!wait_while_debug("timeout waiting for STOP to clear", &UCB0CTLW0, UCTXSTP, true, to)) {
        return false;
    }
    if (!wait_while_debug("timeout waiting for bus free", &UCB0STATW, UCBBUSY, true, to)) {
        return false;
    }
    return true;
}

static bool wait_tx_ready(uint32_t to)
{
    if (!wait_while_debug("timeout waiting for TXIFG", &UCB0IFG, UCTXIFG0, false, to)) {
        return false;
    }
    if (UCB0IFG & UCNACKIFG) {
        i2c_debug("NACK received during transmit");
        force_stop_and_clear();
        return false;
    }
    return true;
}

static bool write_byte(uint8_t value, uint32_t to)
{
    UCB0TXBUF = value;
    return wait_tx_ready(to);
}

static bool write_buffer(const uint8_t *data, uint16_t len, uint32_t to)
{
    for (uint16_t i = 0; i < len; i++) {
        uint8_t value = data ? data[i] : 0x00;
        if (!write_byte(value, to)) {
            i2c_debug("failed while writing buffer");
            return false;
        }
    }
    return true;
}
static bool read_bytes(uint8_t *data, uint16_t len, uint32_t to)
{
    if (len == 1) {
        // 1 bájtos vétel: STT törlődés után AZONNAL állíts STOP-ot
        UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) {
            i2c_debug("timeout waiting for RXIFG (len=1)");
            force_stop_and_clear();
            return false;
        }
        data[0] = UCB0RXBUF;
    } else {
        for (uint16_t i = 0; i < len; i++) {
            if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) {
                i2c_debug("timeout waiting for RXIFG");
                force_stop_and_clear();
                return false;
            }
            if (i == (len - 2)) {
                // N–1. bájt megérkezett -> már most kérj STOP-ot
                UCB0CTLW0 |= UCTXSTP;
            }
            data[i] = UCB0RXBUF;
        }
    }
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
        i2c_debug("timeout waiting for STOP after read");
        force_stop_and_clear();
        return false;
    }
    return true;
}

bool i2c_bus_busy(void)
{
    return (UCB0STATW & UCBBUSY) != 0;
}

void i2c_init(uint32_t smclk_hz, uint32_t bus_hz)
{
    // Pin funkció beállítás
    I2C_PORT_SEL0 |=  I2C_SDA_BIT | I2C_SCL_BIT;
    I2C_PORT_SEL1 &= ~(I2C_SDA_BIT | I2C_SCL_BIT);

    UCB0CTLW0 = UCSWRST; // reset alatt

    UCB0CTLW0 |= UCMST | UCMODE_3 | UCSSEL__SMCLK | UCSWRST; // I2C master, SMCLK

    if (bus_hz == 0) bus_hz = 100000u;
    uint16_t brw = (uint16_t)(smclk_hz / bus_hz);
    if (brw == 0) brw = 1;
    UCB0BRW = brw;

    UCB0CTLW1 = UCASTP_2 | UCGLIT_0;

    UCB0CTLW0 &= ~UCSWRST; // engedély

    UCB0IFG = 0;
}

void i2c_force_reset(void)
{
    UCB0CTLW0 |= UCSWRST;
    UCB0IFG = 0;
    UCB0CTLW0 &= ~UCSWRST;
}

bool i2c_recover_bus_gpio(void)
{
    // 1) eUSCI reset + pin vissza GPIO‑ra
    UCB0CTLW0 |= UCSWRST;
    I2C_PORT_SEL0 &= ~(I2C_SDA_BIT | I2C_SCL_BIT);
    I2C_PORT_SEL1 &= ~(I2C_SDA_BIT | I2C_SCL_BIT);

    // Engedjünk felhúzót (ha nincs külső), és engedjük fel a vonalakat
    I2C_PORT_REN |= (I2C_SDA_BIT | I2C_SCL_BIT);
    I2C_PORT_OUT |= (I2C_SDA_BIT | I2C_SCL_BIT);

    // 2) 9 órapulzus SCL‑en (SDA release), hogy bármi beragadt slave elengedjen
    for (int i = 0; i < 9; i++) {
        // SCL low: kimenetként 0
        I2C_PORT_DIR |=  I2C_SCL_BIT;
        I2C_PORT_OUT &= ~I2C_SCL_BIT;
        __delay_cycles(150);
        // SCL release: bemenet (pull‑up felhúzza)
        I2C_PORT_DIR &= ~I2C_SCL_BIT;
        __delay_cycles(150);
        // Ha közben SDA felszabadult, folytatjuk a ciklust, a 9 pulzus nem árt
    }

    // 3) Generáljunk egy „mű STOP‑ot”: SDA low → SCL high → SDA high
    // SDA low
    I2C_PORT_DIR |=  I2C_SDA_BIT;
    I2C_PORT_OUT &= ~I2C_SDA_BIT;
    __delay_cycles(150);
    // SCL release
    I2C_PORT_DIR &= ~I2C_SCL_BIT;
    __delay_cycles(150);
    // SDA release (felhúzás magasra teszi)
    I2C_PORT_DIR &= ~I2C_SDA_BIT;
    __delay_cycles(150);

    // 4) Vissza I2C funkcióra
    I2C_PORT_SEL0 |=  I2C_SDA_BIT | I2C_SCL_BIT;
    I2C_PORT_SEL1 &= ~(I2C_SDA_BIT | I2C_SCL_BIT);
    UCB0IFG = 0;
    UCB0CTLW0 &= ~UCSWRST;

    // Ha még mindig BUSY, jelezzük
    return !i2c_bus_busy();
}

bool i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t to)
{
    bool ok = false;

    do {
        if (!ensure_bus_ready(to)) {
            break;
        }

        UCB0I2CSA  = addr7;
        UCB0CTLW0 |= UCTR | UCTXSTT; // TX + START

        if (!wait_tx_ready(to)) {
            break;
        }

        if (!write_buffer(data, len, to)) {
            break;
        }

        UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
            force_stop_and_clear();
            break;
        }

        ok = true;
    } while (0);

    return ok;
}

bool i2c_read(uint8_t addr7, uint8_t *data, uint16_t len, uint32_t to)
{
    if (len == 0) return true;
    if (!data) return false;

    memset(data, 0, len);

    bool ok = false;

    do {
        if (!ensure_bus_ready(to)) {
            break;
        }

        UCB0I2CSA  = addr7;
        UCB0CTLW0 &= ~UCTR;     // RX
        UCB0CTLW0 |=  UCTXSTT;  // START

        if (!wait_while(&UCB0CTLW0, UCTXSTT, true, to)) {
            i2c_debug("timeout waiting for RX START to clear");
            force_stop_and_clear();
            break;
        }
        if (UCB0IFG & UCNACKIFG) {
            i2c_debug("NACK received on RX START");
            force_stop_and_clear();
            break;
        }

        ok = read_bytes(data, len, to);
    } while (0);

    return ok;
}

bool i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len, uint32_t to)
{
    bool ok = false;

    do {
        if (!ensure_bus_ready(to)) {
            break;
        }

        UCB0I2CSA  = addr7;
        UCB0CTLW0 |= UCTR | UCTXSTT;

        if (!wait_tx_ready(to)) {
            break;
        }

        if (!write_byte(reg, to)) {
            break;
        }

        if (!write_buffer(data, len, to)) {
            break;
        }

        UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
            force_stop_and_clear();
            break;
        }

        ok = true;
    } while (0);

    return ok;
}

bool i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len, uint32_t to)
{
    if (len == 0) return true;
    if (!data) return false;

    memset(data, 0, len);

    bool ok = false;

    do {
        if (!ensure_bus_ready(to)) {
            break;
        }

        UCB0I2CSA  = addr7;

        // subaddress kiírása
        UCB0CTLW0 |= UCTR | UCTXSTT;
        if (!wait_tx_ready(to)) {
            break;
        }

        if (!write_byte(reg, to)) {
            i2c_debug("failed to send register address");
            break;
        }

        // repeated START olvasáshoz
        UCB0CTLW0 &= ~UCTR;
        UCB0CTLW0 |=  UCTXSTT;
        if (!wait_while(&UCB0CTLW0, UCTXSTT, true, to)) {
            i2c_debug("timeout on repeated START");
            force_stop_and_clear();
            break;
        }
        if (UCB0IFG & UCNACKIFG) {
            i2c_debug("NACK received after repeated START");
            force_stop_and_clear();
            break;
        }

        ok = read_bytes(data, len, to);
    } while (0);

    return ok;
}

// Egyetlen bájt olvasása regisztercímről (robosztus, auto-STOP + TBCNT=1)
bool i2c_read_reg1(uint8_t addr7, uint8_t reg, uint8_t *value, uint32_t to)
{
    if (!value) return false;
    *value = 0;

    // biztos, ami biztos: előző tranzakció nyomai ne zavarjanak
    UCB0IFG  &= ~UCNACKIFG;
    UCB0TBCNT = 0;

    if (!ensure_bus_ready(to)) {
        return false;
    }

    UCB0I2CSA = addr7;

    // --- 1) Regisztercím kiírása (WRITE + START) ---
    UCB0CTLW0 |= UCTR | UCTXSTT;
    if (!wait_tx_ready(to)) {
        // START / cím NACK
        return false;
    }
    if (!write_byte(reg, to)) {
        i2c_debug("failed to send register address (1B)");
        return false;
    }

    // --- 2) 1 bájtos olvasás hardveres auto-STOP-pal ---
    // TBCNT-et a READ START ELŐTT kell beállítani
    UCB0TBCNT = 1;

    UCB0CTLW0 &= ~UCTR;      // RX mód
    UCB0CTLW0 |=  UCTXSTT;   // repeated START

    if (!wait_while_debug("timeout waiting for RX START to clear (1B)",
                          &UCB0CTLW0, UCTXSTT, true, to)) {
        return false; // force_stop_and_clear már lefutott
    }
    if (UCB0IFG & UCNACKIFG) {
        i2c_debug("NACK after RX START (1B)");
        force_stop_and_clear();
        return false;
    }

    // várjuk az egyetlen bájtot
    if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) {
        i2c_debug("timeout waiting for RXIFG (1B)");
        force_stop_and_clear();
        return false;
    }
    *value = UCB0RXBUF;   // a STOP-ot a hardver ütemezi (NACK+STOP)

    // várjuk meg, míg a STOP kifut
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
        i2c_debug("timeout waiting for STOP (1B)");
        force_stop_and_clear();
        return false;
    }

    // opcionális: vissza alapra
    UCB0TBCNT = 0;

    return true;
}

/*
bool i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len, uint32_t to)
{
    if (len == 0) return true;
    if (!data)    return false;

    memset(data, 0, len);

    // Biztosan NE legyen auto-STOP ezen a tranzakción
    UCB0TBCNT = 0;
    // Elővigyázatosságból töröljük a NACK flag-et
    UCB0IFG &= ~UCNACKIFG;

    if (!ensure_bus_ready(to)) {
        return false;
    }

    UCB0I2CSA  = addr7;

    // --- 1) Regisztercím kiírása (WRITE + START) ---
    UCB0CTLW0 |= UCTR | UCTXSTT;

    if (!wait_tx_ready(to)) {
        // START vagy cím NACK
        return false;
    }
    if (!write_byte(reg, to)) {
        i2c_debug("failed to send register address");
        return false;
    }

    // --- 2) Repeated START olvasáshoz ---
    UCB0CTLW0 &= ~UCTR;       // RX mód
    UCB0CTLW0 |=  UCTXSTT;    // START

    if (!wait_while_debug("timeout waiting for RX START to clear",
                          &UCB0CTLW0, UCTXSTT, true, to)) {
        return false; // force_stop_and_clear() már lefutott
    }
    if (UCB0IFG & UCNACKIFG) {
        i2c_debug("NACK after repeated START");
        force_stop_and_clear();
        return false;
    }

    // --- 3) Vétel + STOP időzítés ---
    if (len == 1) {
        // 1 bájt: START törlődése után azonnal kérj STOP-ot,
        // majd várj RXIFG-re, olvass, és várd meg a STOP törlődését.
        UCB0CTLW0 |= UCTXSTP;

        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) {
            i2c_debug("timeout waiting for RXIFG (len=1)");
            force_stop_and_clear();
            return false;
        }
        data[0] = UCB0RXBUF;

        if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
            i2c_debug("timeout waiting for STOP (len=1)");
            force_stop_and_clear();
            return false;
        }
        return true;
    }

    // len >= 2: amikor 2 bájt marad, ELŐBB kérj STOP-ot, AZUTÁN olvasd a (N-1). bájtot
    for (uint16_t i = 0; i < len; i++) {
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) {
            i2c_debug("timeout waiting for RXIFG (len>=2)");
            force_stop_and_clear();
            return false;
        }

        if (i == (len - 2)) {          // most a második utolsó bájt érkezett meg
            UCB0CTLW0 |= UCTXSTP;      // kérjük a STOP-ot még a kiolvasás ELŐTT
        }

        data[i] = UCB0RXBUF;           // RXBUF olvasása mindig felszabadítja a következő bájtot
    }

    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) {
        i2c_debug("timeout waiting for STOP (len>=2)");
        force_stop_and_clear();
        return false;
    }

    return true;
}
*/