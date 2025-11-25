#include "dali_phy_mac.h"
#include <msp430.h>
#include <stdbool.h>
#include <stddef.h>

#define DALI_HALF_TICKS    ((uint16_t)417)   /* ~416.7 µs @ 1 MHz */
#define DALI_HALF_OFFSET   ((uint16_t)(DALI_HALF_TICKS / 2u))
#define DALI_MAX_BITS      (32u)
#define DALI_MIN_BITS      (8u)
#define DALI_MAX_HALVES    (DALI_MAX_BITS * 2u + 6u)

static volatile dali_frame_callback_t s_callback = NULL;

static volatile bool s_receiving = false;
static volatile bool s_start_seen = false;
static volatile uint8_t s_prev_half = 1u;
static volatile uint16_t s_half_count = 0u;
static volatile uint8_t s_bit_count = 0u;
static volatile uint32_t s_frame = 0u;
static volatile uint8_t s_last_bits = 0u;
static volatile uint32_t s_last_frame = 0u;
static volatile uint16_t s_debug_ecomp_hits = 0u;
static volatile uint16_t s_debug_timer_hits = 0u;

static void dali_debug_putc(char c)
{
    while ((UCA0IFG & UCTXIFG) == 0) {
        /* wait for TX buffer */
    }
    UCA0TXBUF = (uint8_t)c;
}

static void dali_debug_puts(const char *s)
{
    if (s == NULL) {
        return;
    }
    while (*s) {
        dali_debug_putc(*s++);
    }
}

static void dali_debug_hex16(const char *prefix, uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    if (prefix) {
        dali_debug_puts(prefix);
    }
    dali_debug_putc(hex[(value >> 12) & 0xFu]);
    dali_debug_putc(hex[(value >> 8) & 0xFu]);
    dali_debug_putc(hex[(value >> 4) & 0xFu]);
    dali_debug_putc(hex[value & 0xFu]);
    dali_debug_puts("\r\n");
}

#define DALI_DEBUG_LOG(msg) dali_debug_puts(msg)

static void dali_reset_state(void)
{
    TA2CCTL0 &= (uint16_t)~CCIE;
    s_receiving = false;
    s_start_seen = false;
    s_prev_half = 1u;
    s_half_count = 0u;
    s_bit_count = 0u;
    s_frame = 0u;
    s_last_bits = 0u;
    s_last_frame = 0u;
    CP0INT &= (uint16_t)~(CPIFG | CPIIFG);
    (void)CP0IV;
    {
        uint16_t sr = __get_SR_register();
        __bic_SR_register(GIE);
        CP0CTL1 |= CPIE;
        if ((sr & GIE) != 0u) {
            __bis_SR_register(GIE);
        }
    }
}

static void dali_begin_reception(void)
{
    s_receiving = true;
    s_start_seen = false;
    s_prev_half = 1u;
    s_half_count = 0u;
    s_bit_count = 0u;
    s_frame = 0u;
    s_last_bits = 0u;
    s_last_frame = 0u;

    CP0CTL1 &= (uint16_t)~CPIE;   /* ne jöjjenek új startok a keret közben */

    /* első mintavételezés a start félbit közepén */
    uint16_t now = TA2R;
    uint16_t first = (DALI_HALF_OFFSET != 0u) ? DALI_HALF_OFFSET : 1u;
    TA2CCR0 = now + first;
    TA2CCTL0 = CCIE;
}

void dali_phy_set_callback(dali_frame_callback_t cb)
{
    s_callback = cb;
}

void dali_phy_init(dali_frame_callback_t cb)
{
    s_callback = cb;
    DALI_DEBUG_LOG("DALI init start\r\n");

    /* P2.2 (eCOMP0 input C1) analóg bemenetként */
    P2DIR &= (uint8_t)~BIT2;
    P2SEL1 |= BIT2;
    P2SEL0 |= BIT2;
    DALI_DEBUG_LOG("DALI init: port mux ok\r\n");

    /* eCOMP konfiguráció: V+ = C1 (P2.2), V- = belső DAC ~0.5*VCC */
    CP0CTL1 = 0; /* biztosítsuk, hogy comparator kikapcsolt állapotból indul */
    DALI_DEBUG_LOG("DALI init: CP0CTL1 reset\r\n");
    dali_debug_hex16("CP0CTL1=", CP0CTL1);
    CP0CTL0 = CPPEN | CPPSEL_1 | CPNEN | CPNSEL_6;  /* V+ channel 1, V- DAC */
    DALI_DEBUG_LOG("DALI init: CP0CTL0 set\r\n");

    CP0DACCTL = CPDACEN;      /* DAC engedélyezése, VREF = VCC */
    //CP0DACDATA = (uint16_t)(32u & 0x3Fu);   /* ~0.5 * VCC */
    CP0DACDATA = (uint16_t)(34 & 0x3Fu);
    DALI_DEBUG_LOG("DALI init: DAC on\r\n");

    CP0CTL1 = CPEN;
    DALI_DEBUG_LOG("DALI init: CPEN set\r\n");
    dali_debug_hex16("CP0CTL1=", CP0CTL1);
    CP0CTL1 &= (uint16_t)~CPIES;  /* rising edge for CPIFG (0→1 él = start bit) */
    DALI_DEBUG_LOG("DALI init: CPIES=0 (rising edge)\r\n");
    dali_debug_hex16("CP0CTL1=", CP0CTL1);
    CP0CTL1 &= (uint16_t)~(CPFLT | CPFLTDLY_3 | CPMSEL | CPHSEL_3);
    DALI_DEBUG_LOG("DALI init: no filter/hysteresis\r\n");
    dali_debug_hex16("CP0CTL1=", CP0CTL1);

    CP0INT &= (uint16_t)~(CPIFG | CPIIFG);
    DALI_DEBUG_LOG("DALI init: CP0INT cleared\r\n");
    dali_debug_hex16("CP0INT=", CP0INT);
    {
        volatile uint16_t dummy_iv = CP0IV;
        (void)dummy_iv;
    }
    DALI_DEBUG_LOG("DALI init: CP0IV flushed\r\n");

    /* Timer2_A: folyamatos üzem, 1 MHz */
    TA2CTL = TASSEL__SMCLK | MC__CONTINUOUS | TACLR;
    TA2CCTL0 = 0;
    DALI_DEBUG_LOG("DALI init: TA2 start\r\n");

    {
        uint16_t sr = __get_SR_register();
        dali_debug_hex16("SR before CPIE=", sr);
        DALI_DEBUG_LOG("DALI init: about to set CPIE\r\n");
        __bic_SR_register(GIE);
        CP0CTL1 |= CPIE;
        dali_debug_hex16("CP0INT after CPIE=", CP0INT);
        (void)CP0IV;
        if ((sr & GIE) != 0u) {
            __bis_SR_register(GIE);
        }
    }
    DALI_DEBUG_LOG("DALI init: CPIE set\r\n");
    dali_debug_hex16("CP0CTL1=", CP0CTL1);
}

void dali_phy_enable(void)
{
    dali_reset_state();
}

void dali_phy_disable(void)
{
    dali_reset_state();
    CP0CTL1 &= (uint16_t)~CPIE;
}

static void dali_process_half(uint8_t level)
{
    bool deliver = false;
    uint32_t frame = 0u;
    uint8_t bits = 0u;
    uint8_t local_bits = 0u;
    uint32_t local_frame = 0u;

    TA2CCR0 += DALI_HALF_TICKS;   /* ütemezzük a következő mintát */

    if (!s_receiving) {
        return;
    }

    if (s_half_count == 0u && level != 0u) {
        dali_reset_state();
        return;
    }

    s_half_count++;

    if ((s_half_count & 0x01u) != 0u) {
        /* bit első fele */
        s_prev_half = level;
        if (!s_start_seen && s_prev_half != 0u) {
            dali_reset_state();
        }
        if (s_half_count > DALI_MAX_HALVES) {
            dali_reset_state();
        }
        return;
    }

    /* bit második fele */
    if (!s_start_seen) {
        if (s_prev_half == 0u && level == 1u) {
            s_start_seen = true;
        } else {
            dali_reset_state();
        }
        return;
    }

    if (s_prev_half == 1u && level == 1u) {
        if (s_bit_count >= DALI_MIN_BITS && s_bit_count <= DALI_MAX_BITS) {
            deliver = true;
            frame = s_frame;
            bits = s_bit_count;
            s_last_frame = frame;
            s_last_bits = bits;
        }
        dali_reset_state();
    } else if (s_prev_half == 0u && level == 1u) {
        if (s_bit_count < DALI_MAX_BITS) {
            s_frame = (s_frame << 1) | 0x1u;
            s_bit_count++;
        } else {
            dali_reset_state();
        }
    } else if (s_prev_half == 1u && level == 0u) {
        if (s_bit_count < DALI_MAX_BITS) {
            s_frame <<= 1;
            s_bit_count++;
        } else {
            dali_reset_state();
        }
    } else {
        dali_reset_state();
    }

    if (deliver) {
        local_frame = frame;
        local_bits = bits;
    }

    if (deliver && s_callback) {
        s_callback(local_frame, local_bits);
    }
}

uint8_t dali_phy_get_last_bits(void)
{
    return s_last_bits;
}

uint32_t dali_phy_get_last_frame(void)
{
    return s_last_frame;
}

#pragma vector = TIMER2_A0_VECTOR
__interrupt static void TIMER2_A0_ISR(void)
{
    uint8_t level = (CP0CTL1 & CPOUT) ? 1u : 0u;
    if (s_debug_timer_hits < 8u) {
        s_debug_timer_hits++;
        dali_debug_puts("TIMER ISR\r\n");
        dali_debug_hex16("TA2CCR0=", TA2CCR0);
        dali_debug_hex16("TA2CTL=", TA2CTL);
    }
    dali_process_half(level);
}
#pragma vector = ECOMP0_VECTOR
__interrupt static void ECOMP0_ISR(void)
{
    uint16_t iv = CP0IV;
    if (s_debug_ecomp_hits < 8u) {
        s_debug_ecomp_hits++;
        dali_debug_puts("ECOMP ISR enter\r\n");
        dali_debug_hex16("IV=", iv);
        dali_debug_hex16("CP0CTL1=", CP0CTL1);
        dali_debug_hex16("CP0INT=", CP0INT);
    }

    switch (__even_in_range(iv, CPIV__CPIIFG)) {
    case CPIV__NONE:
        break;
    case CPIV__CPIFG:
    case CPIV__CPIIFG:
        CP0INT &= (uint16_t)~(CPIFG | CPIIFG);
        if (!s_receiving) {
            if ((CP0CTL1 & CPOUT) != 0u) {  /* rising edge: CPOUT should be 1 after 0→1 transition */
                dali_begin_reception();
            }
        }
        break;
    default:
        break;
    }
}
