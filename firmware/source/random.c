#include "random.h"
#include <msp430.h>

// LCG (Linear Congruential Generator) parameters
// These are good values for 16-bit: a=11035, c=12345, m=2^16 (implicit)
#define RANDOM_A 11035
#define RANDOM_C 12345

static uint16_t g_random_seed = 1;

void random_init(uint16_t seed)
{
    g_random_seed = seed;
    if (g_random_seed == 0) {
        g_random_seed = 1;  // Seed cannot be 0
    }
}

uint16_t random_next(void)
{
    g_random_seed = g_random_seed * RANDOM_A + RANDOM_C;
    return g_random_seed;
}

uint16_t random_range(uint16_t max)
{
    if (max == 0) {
        return 0;
    }
    return random_next() % max;
}

// Generate random seed from ADC noise (LSB bits)
uint16_t generate_random_seed_from_adc(void)
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

