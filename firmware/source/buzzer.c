#include "buzzer.h"
#include "random.h"

static volatile uint32_t g_smclk = 1000000u;
static volatile uint32_t g_periods_left = 0;
static volatile uint8_t  g_busy = 0;
static volatile uint8_t  g_stop_requested = 0;  // Flag to request PWM stop from main loop

// Zajgenerátor változók
static volatile uint32_t g_noise_periods_left = 0;
static volatile uint8_t  g_noise_busy = 0;

static inline void pins_init(void)
{
    // P1.1 (TA0.1) és P1.5 (TA1.1) periféria funkció
    P1SEL1 |=  (BIT1 | BIT5);   // 10
    P1SEL0 &= ~(BIT1 | BIT5);
    P1DIR  |=  (BIT1 | BIT5);
}

// Helper függvény: timer-alapú polling delay
// Timer B0-t használjuk (TA0-t ne használjuk, mert PWM-hez van lefoglalva)
// Elindítja a timert, reseteli, és poll-olja a TB0R regisztert
// Blocking, de nem használ CPU ciklusokat úgy, mint a __delay_cycles
static inline void delay_cycles_timer_poll(uint32_t cycles)
{
    if (cycles == 0) return;
    
    // Timer B0 elindítása continuous mode-ban, SMCLK, reset-tel
    TB0CTL = TBSSEL__SMCLK | ID__1 | MC__CONTINUOUS | TBCLR;
    
    // Cél időpont (0-tól indul, mert reset-eltük)
    uint16_t target_time = (uint16_t)cycles;
    
    // Poll-oljuk a TB0R-t, amíg el nem éri a cél értéket
    while (TB0R < target_time) {
        // Üres loop, csak poll-oljuk
    }
    
    // Timer B0 leállítása
    TB0CTL = MC__STOP;
}

void buzzer_click(uint16_t cycles1, int16_t cycles2, uint8_t periods)
{
    //uint32_t cycles = ((uint32_t)duration_us * g_smclk) / 1000000u;
    uint16_t cycles = cycles1;
    // Pin-ek beállítása GPIO módba
    P1SEL1 &= ~(BIT1 | BIT5);
    P1SEL0 &= ~(BIT1 | BIT5);
    P1DIR  |=  (BIT1 | BIT5);  // Output

    for(int i = 0; i < periods; i++) {  
        // Első irány: P1.1 HIGH, P1.5 LOW
        P1OUT |= BIT1;
        P1OUT &= ~BIT5;
        
        // Várakozás (duration_us mikroszekundum)
        // cycles = (duration_us * g_smclk) / 1000000
        //uint16_t cycles = cycles1 + random_next() % cycles2;
        /*delay_cycles_timer_poll(500);
        P1DIR &= ~(BIT1 | BIT5);
        delay_cycles_timer_poll(cycles-500);
        P1DIR  |=  (BIT1 | BIT5);*/
        delay_cycles_timer_poll(cycles);
        cycles += cycles2;
        
        // Második irány: P1.1 LOW, P1.5 HIGH
        P1OUT &= ~BIT1;
        P1OUT |= BIT5;
        
        /*delay_cycles_timer_poll(500);
        P1DIR &= ~(BIT1 | BIT5);
        delay_cycles_timer_poll(cycles-500);
        P1DIR  |=  (BIT1 | BIT5);*/
        delay_cycles_timer_poll(cycles);
        cycles += cycles2;
    }
    // Mindkettő kikapcsolása
    P1OUT &= ~(BIT1 | BIT5);
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

    // Ha a zajgenerátor fut, leállítjuk
    if (g_noise_busy) {
        buzzer_noise_stop();
    }

    // Pin-ek visszaállítása periféria módba (ha esetleg GPIO módban lennének)
    pins_init();

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
            TA0CTL = MC__STOP;
        }
    }
}

// --- Zajgenerátor funkciók ---
static inline void noise_pins_init(void)
{
    // P1.1 és P1.5 GPIO módban (nem periféria)
    P1SEL1 &= ~(BIT1 | BIT5);
    P1SEL0 &= ~(BIT1 | BIT5);
    P1DIR  |=  (BIT1 | BIT5);  // Output
    P1OUT &= ~(BIT1 | BIT5);   // Alapértelmezett: mindkettő LOW
}

static void noise_stop_all(void)
{
    TA1CTL = MC__STOP;
    TA1CCTL0 = 0;
    P1OUT &= ~(BIT1 | BIT5);
    // Pin-ek visszaállítása periféria módba (buzzer PWM-hez)
    pins_init();
}

bool buzzer_noise(uint16_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz < 1) freq_hz = 1;
    if (freq_hz > 100000) freq_hz = 100000;  // Magasabb limit, mert nincs PWM
    if (duration_ms == 0) return false;

    // Ha a buzzer fut, leállítjuk (mindkét timer-t használja)
    if (g_busy) {
        buzzer_stop();
    }

    // Periódus ticks = SMCLK / f  (up-módban CCR0 = ticks-1)
    uint32_t ticks = g_smclk / (uint32_t)freq_hz;
    if (ticks < 2) ticks = 2;
    if (ticks > 65535u) ticks = 65535u;
    uint16_t period = (uint16_t)(ticks - 1u);

    // Hány egész periódus fér bele az időtartamba?
    uint32_t periods = ((uint64_t)duration_ms * (uint64_t)freq_hz + 500u) / 1000u;
    if (periods == 0) periods = 1;

    __disable_interrupt();
    g_noise_periods_left = periods;
    g_noise_busy = 1;
    __enable_interrupt();

    // Pin-ek beállítása GPIO módra
    noise_pins_init();

    // Timer A1 beállítása (TA0 a buzzer-hez van használva)
    TA1CTL = MC__STOP | TACLR;
    TA1CCR0 = period;
    TA1CCTL0 = CCIE;  // CCR0 interrupt engedélyezés
    
    // Timer indítása: SMCLK, up mode
    TA1CTL = TASSEL__SMCLK | ID__1 | MC__UP | TACLR;

    return true;
}

void buzzer_noise_stop(void)
{
    __disable_interrupt();
    g_noise_periods_left = 0;
    g_noise_busy = 0;
    __enable_interrupt();

    noise_stop_all();
}

bool buzzer_noise_busy(void)
{
    return g_noise_busy != 0;
}

// --- TA1 CCR0 ISR: zajgenerátor interrupt ---
#pragma vector = TIMER1_A0_VECTOR
__interrupt void TA1_CCR0_ISR(void)
{
    if (g_noise_periods_left > 0) {
        // Random érték generálása
        uint16_t r = random_next();
        
        // Mindig ellentétes fázisban: ha P1.1 HIGH, akkor P1.5 LOW és fordítva
        if (r & 1) {
            // Páratlan: P1.1 HIGH, P1.5 LOW
            P1OUT |= BIT1;
            P1OUT &= ~BIT5;
        } else {
            // Páros: P1.1 LOW, P1.5 HIGH
            P1OUT &= ~BIT1;
            P1OUT |= BIT5;
        }
        
        g_noise_periods_left--;
        if (g_noise_periods_left == 0) {
            g_noise_busy = 0;
            noise_stop_all();
        }
    }
}
