#include "buzzer.h"

static volatile uint32_t g_smclk = 1000000u;
static volatile uint32_t g_periods_left = 0;
static volatile uint8_t  g_busy = 0;

static inline void pins_init(void)
{
    // P1.1 (TA0.1) és P1.5 (TA1.1) periféria funkció
    P1SEL1 |=  (BIT1 | BIT5);   // 10
    P1SEL0 &= ~(BIT1 | BIT5);
    P1DIR  |=  (BIT1 | BIT5);
}

void buzzer_click(void)
{
    P1OUT |= BIT1;
    __delay_cycles(100000);
    P1OUT &= ~BIT1;
    __delay_cycles(100000);
    P1OUT |= BIT5;
    __delay_cycles(100000);
    P1OUT &= ~BIT5;
}

void buzzer_init(uint32_t smclk_hz)
{
    g_smclk = smclk_hz ? smclk_hz : 1000000u;
    g_busy = 0; g_periods_left = 0;

    pins_init();

    // Timer-ek leállítása, kimenetek alapállapot
    TA0CTL = MC__STOP | TACLR;  TA1CTL = MC__STOP | TACLR;
    TA0CCTL1 = 0;               TA1CCTL1 = 0;
    P1OUT &= ~(BIT1 | BIT5);

    // TA0 CCR0 megszakítás engedélyezés (periódus végén fog futni)
    TA0CCTL0 = CCIE;
}

static void pwm_apply(uint16_t period_ticks)
{
    uint16_t half = (uint16_t)((period_ticks + 1u) / 2u);

    // TA0: P1.1 = TA0.1
    TA0CCR0  = period_ticks;    // periódus
    TA0CCR1  = half;            // 50% duty
    TA0CCTL1 = OUTMOD_7;        // reset/set

    // TA1: P1.5 = TA1.1
    TA1CCR0  = period_ticks;
    TA1CCR1  = half;
    TA1CCTL1 = OUTMOD_3;        // set/reset → komplementer polaritás

    // (opcionális: pontos 180° fázis) TA1R = half;
    // Indítás: SMCLK, up mode
    TA0CTL = TASSEL__SMCLK | ID__1 | MC__UP | TACLR;
    TA1CTL = TASSEL__SMCLK | ID__1 | MC__UP | TACLR;
}

static void pwm_stop_all(void)
{
    TA0CTL = MC__STOP; TA1CTL = MC__STOP;
    TA0CCTL1 = 0;      TA1CCTL1 = 0;
    P1OUT &= ~(BIT1 | BIT5);
}

bool buzzer_beep(uint16_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz < 20) freq_hz = 20;
    if (freq_hz > 40000) freq_hz = 40000;
    if (duration_ms == 0) return false;

    // Periódus ticks = SMCLK / f  (up-módban CCR0 = ticks-1)
    uint32_t ticks = g_smclk / (uint32_t)freq_hz;
    if (ticks < 2) ticks = 2;
    if (ticks > 65535u) ticks = 65535u;
    uint16_t period = (uint16_t)(ticks - 1u);

    // Hány egész periódus fér bele az időtartamba?
    // periods = round(duration_ms * f / 1000)
    uint32_t periods = ((uint64_t)duration_ms * (uint64_t)freq_hz + 500u) / 1000u;
    if (periods == 0) periods = 1;

    __disable_interrupt();
    g_periods_left = periods;
    g_busy = 1;
    __enable_interrupt();

    pwm_apply(period);

    return true; // azonnal visszatér
}

void buzzer_stop(void)
{
    __disable_interrupt();
    g_periods_left = 0;
    g_busy = 0;
    __enable_interrupt();

    pwm_stop_all();
}

bool buzzer_busy(void)
{
    return g_busy != 0;
}

// --- TA0 CCR0 ISR: periódus vége ---
#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA0_CCR0_ISR(void)
{
    if (g_periods_left > 0) {
        g_periods_left--;
        if (g_periods_left == 0) {
            g_busy = 0;
            pwm_stop_all();
            // Timer0_A mehet is tovább, de meg is állíthatjuk (nem kötelező):
            TA0CTL = MC__STOP;
        }
    }
}
