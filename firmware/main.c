#include <msp430.h>
#include "uart.h"
#include "watchdog.h"
#include "i2c.h"
#include "util.h"
#include "led.h"
#include "buzzer.h"
#include "touch.h"
#include "clock.h"
#include "scheduler.h"
#include "sensors.h"
#include "captivate.h"
#include "dali_phy_mac.h"
#include "random.h"

#define CLOCK_HZ 8000000u

// Generate random seed from ADC noise (LSB bits)
static uint16_t generate_random_seed_from_adc(void)
{
    uint16_t seed = 0;
    
    // Configure ADC for A6 (P1.6) for random seed generation
    ADCCTL0 &= ~ADCENC;  // Disable to modify
    
    // Configure P1.6 as Analog Input (A6)
    P1DIR &= ~BIT6;      // Input mode
    P1SEL0 &= ~BIT6;     // GPIO/Analog mode
    P1SEL1 &= ~BIT6;
    P1REN &= ~BIT6;      // No pull-up/pull-down
    SYSCFG2 |= BIT6;     // ADCPCTL6 - Enable ADC on P1.6
    
    // Configure ADC properly (same as dali_phy_mac.c)
    // ADCCTL0: Sample hold time, enable ADC core
    ADCCTL0 = ADCSHT_2 | ADCON;
    
    // ADCCTL1: Software trigger, sample-and-hold pulse mode, single conversion
    ADCCTL1 = ADCSHS_0 | ADCSHP | ADCCONSEQ_0;
    
    // ADCCTL2: 12-bit resolution
    ADCCTL2 = ADCRES_2;
    
    // ADCMCTL0: Input Channel A6, Vref=AVCC
    ADCMCTL0 = ADCINCH_6;
    
    // Enable ADC
    ADCCTL0 |= ADCENC;
    
    // Collect LSB bits from multiple ADC readings
    // Each reading contributes 1 bit to the seed

    for (uint8_t i = 0; i < 16; i++) {
        // Trigger ADC conversion
        ADCCTL0 |= ADCSC;
        
        // Wait for conversion to complete
        // Poll ADCBUSY bit in ADCCTL1 - it clears when conversion is done
        while (ADCCTL1 & ADCBUSY);
        
        // Get LSB and shift into seed
        uint16_t adc_value = ADCMEM0;
        seed = (seed << 1) | (adc_value & 1);
        
        // Small delay to ensure different noise samples
        __delay_cycles(10000);
    }
    
    // Ensure seed is not zero
    if (seed == 0) {
        seed = 1;
    }
    
    return seed;
}

int main(void)
{
    disable_watchdog();
    PM5CTL0 &= ~LOCKLPM5;
    
    clock_init_8mhz();
    delay_ms_8mhz(200); 
    led_init();

    uart_init();
    buzzer_init(CLOCK_HZ);
    buzzer_beep(1000, 100);
    
    // Generate random seed from ADC noise and initialize RNG
    uint16_t random_seed = generate_random_seed_from_adc();
    random_init(random_seed);
    
    // Print seed to UART for verification
    uart_send_string("Random seed: ");
    uart_send_hex((uint8_t*)&random_seed, 2);
    uart_send_string("\r\n");
    
    dali_phy_init();
    
    i2c_init(CLOCK_HZ, 100000u);

    sensors_init();
    delay_ms_8mhz(100);

    touch_init();
    
    // Configure P1.5 as GPIO Output
    // Stop Timer A1 first (already done in adc_init, but ensure it's stopped)
    TA1CTL = MC__STOP | TACLR;
    TA1CCTL1 = 0;

    __bis_SR_register(GIE);        // Enable global interrupts

    uint16_t loop_counter = 0;

    scheduler_init(g_uiApp.ui16ActiveModeScanPeriod, 1000);

    scheduler_add_task(ens210_start_measurement_task, 250);
    scheduler_add_task(ens210_read_results_task, 4000);

    scheduler_add_task(lps22hh_start_measurement_task, 500);
    scheduler_add_task(lps22hh_read_results_task, 4000);

    scheduler_add_task(stcc4_push_compensation_task, 300);
    scheduler_add_task(stcc4_start_measurement_task, 400);
    scheduler_add_task(stcc4_read_results_task, 5000);

    uart_send_string("==== DALI PHY test ====\r\n");
    while(1) {
        CAPT_appHandler();
        if(g_eventTap) {
            buzzer_beep(2000, 5);
            uart_send_string("Tap\r\n");
            dali_tx_send_frame(0xFA5222, 24);
            g_eventTap = 0;
        }
        if(g_eventUp) {
            buzzer_beep(4000, 5);
            uart_send_string("Up\r\n");
            g_eventUp = 0;
        }
        if(g_eventDown) {
            buzzer_beep(1000, 5);
            uart_send_string("Down\r\n");
            g_eventDown = 0;
        }
        /*loop_counter += g_uiApp.ui16ActiveModeScanPeriod;
        if(loop_counter >= 1000) {
            loop_counter = 0;
            uart_send_string("*tick*\r\n");
        }*/
        scheduler_tick();
        CAPT_appSleep();
    }
}

