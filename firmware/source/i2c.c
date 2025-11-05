#include "i2c.h"

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
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    if (!wait_while(&UCB0STATW, UCBBUSY,  true, to)) { force_stop_and_clear(); return false; }

    UCB0I2CSA  = addr7;
    UCB0CTLW0 |= UCTR | UCTXSTT; // TX + START

    if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    for (uint16_t i = 0; i < len; i++) {
        UCB0TXBUF = (data ? data[i] : 0x00);
        if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
        if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }
    }

    UCB0CTLW0 |= UCTXSTP;
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    return true;
}

bool i2c_read(uint8_t addr7, uint8_t *data, uint16_t len, uint32_t to)
{
    if (len == 0) return true;

    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    if (!wait_while(&UCB0STATW, UCBBUSY,  true, to)) { force_stop_and_clear(); return false; }

    UCB0I2CSA  = addr7;
    UCB0CTLW0 &= ~UCTR;     // RX
    UCB0CTLW0 |=  UCTXSTT;  // START

    if (!wait_while(&UCB0CTLW0, UCTXSTT, true, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    if (len == 1) {
        UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) { force_stop_and_clear(); return false; }
        data[0] = UCB0RXBUF;
        if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
        return true;
    }

    for (uint16_t i = 0; i < len; i++) {
        if (i == (len - 1)) UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) { force_stop_and_clear(); return false; }
        data[i] = UCB0RXBUF;
    }
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    return true;
}

bool i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *data, uint16_t len, uint32_t to)
{
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    if (!wait_while(&UCB0STATW, UCBBUSY,  true, to)) { force_stop_and_clear(); return false; }

    UCB0I2CSA  = addr7;
    UCB0CTLW0 |= UCTR | UCTXSTT;

    if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    UCB0TXBUF = reg;
    if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    for (uint16_t i = 0; i < len; i++) {
        UCB0TXBUF = data[i];
        if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
        if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }
    }

    UCB0CTLW0 |= UCTXSTP;
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    return true;
}

bool i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *data, uint16_t len, uint32_t to)
{
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    if (!wait_while(&UCB0STATW, UCBBUSY,  true, to)) { force_stop_and_clear(); return false; }

    UCB0I2CSA  = addr7;

    // subaddress kiírása
    UCB0CTLW0 |= UCTR | UCTXSTT;
    if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    UCB0TXBUF = reg;
    if (!wait_while(&UCB0IFG, UCTXIFG0, false, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    // repeated START olvasáshoz
    UCB0CTLW0 &= ~UCTR;
    UCB0CTLW0 |=  UCTXSTT;
    if (!wait_while(&UCB0CTLW0, UCTXSTT, true, to)) { force_stop_and_clear(); return false; }
    if (UCB0IFG & UCNACKIFG) { force_stop_and_clear(); return false; }

    if (len == 1) {
        UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) { force_stop_and_clear(); return false; }
        data[0] = UCB0RXBUF;
        if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
        return true;
    }

    for (uint16_t i = 0; i < len; i++) {
        if (i == (len - 1)) UCB0CTLW0 |= UCTXSTP;
        if (!wait_while(&UCB0IFG, UCRXIFG0, false, to)) { force_stop_and_clear(); return false; }
        data[i] = UCB0RXBUF;
    }
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) { force_stop_and_clear(); return false; }
    return true;
}

bool i2c_write_then_read(uint8_t addr7,
                         const uint8_t *wbuf, uint16_t wlen,
                         uint8_t *rbuf,       uint16_t rlen,
                         uint32_t to)
{
    if (!i2c_write(addr7, wbuf, wlen, to)) return false;
    return i2c_read(addr7, rbuf, rlen, to);
}

bool i2c_probe(uint8_t addr7, uint32_t to)
{
    // Várjuk meg, hogy az előző STOP tényleg kiment
    if (!wait_while(&UCB0CTLW0, UCTXSTP, true, to)) return false;

    // IFG-k takarítása (különösen UCTXIFG0, hogy ne legyen „előre set”)
    UCB0IFG &= ~(UCTXIFG0 | UCRXIFG0 | UCNACKIFG);

    UCB0I2CSA  = addr7;
    UCB0CTLW0 |= UCTR | UCTXSTT;   // TX mód + START (cím write módban megy ki automatikusan)

    // Várunk: vagy a START bit törlődik (cím kiment), vagy NACK jön
    uint32_t t = to;
    while (t--) {
        if (UCB0IFG & UCNACKIFG) {
            // NACK → STOP, flag tisztítás, és hamis
            UCB0CTLW0 |= UCTXSTP;
            (void)wait_while(&UCB0CTLW0, UCTXSTP, true, to);
            UCB0IFG &= ~UCNACKIFG;
            return false;
        }
        if ((UCB0CTLW0 & UCTXSTT) == 0) break; // cím elküldve
    }
    if (t == 0) {
        // timeout közben → próbáljunk kulturáltan lezárni
        UCB0CTLW0 |= UCTXSTP;
        (void)wait_while(&UCB0CTLW0, UCTXSTP, true, to);
        return false;
    }

    // Ha idáig eljutottunk NACK nélkül, tekintsük ACK-olt címnek
    UCB0CTLW0 |= UCTXSTP;
    (void)wait_while(&UCB0CTLW0, UCTXSTP, true, to);
    return true;
}
