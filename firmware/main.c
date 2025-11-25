#include <msp430.h>
#include "uart.h"
#include "watchdog.h"
#include "i2c.h"
#include "util.h"
#include "gpio.h"

volatile uint16_t g_adc_last_value = 0;

// ADC Trigger source definitions if not defined
#ifndef ADCSHS_3
#define ADCSHS_3 (0x0300) // Value 3 shifted to SHS position (bits 9-8 usually? No, SHS is bits 11-8 in ADCCTL1)
// Actually on FR2xx: ADCCTL1 bits 9-8 are ADCSHSx ? No.
// Let's check standard headers logic:
// ADCCTL1:
//  Bits 10-9: ADCSHSx (Sample/hold source)
//  00 = ADCSC bit
//  01 = TA0.1
//  10 = TA1.1
//  11 = TA2.1 (likely)
// Wait, 2 bits only? Or more?
// On some devices it's 2 bits (10-9). On others it's more.
// If it is 2 bits:
// ADCSHS_0 = 0x0000
// ADCSHS_1 = 0x0200
// ADCSHS_2 = 0x0400
// ADCSHS_3 = 0x0600
// Let's define ADCSHS_3 as 0x0600 if undefined.
#endif

#ifndef ADCSHS_3
#define ADCSHS_3 (0x0600)
#endif

void adc_init(void)
{
   // 1. Stop Timer A0 and A1 to release P1.1 and P1.5
   TA0CTL = MC__STOP | TACLR;
   TA1CTL = MC__STOP | TACLR;
   TA0CCTL1 = 0;
   TA1CCTL1 = 0;
  
   // 2. Configure P1.1 as Analog Input (A1)
   // Clear P1DIR to ensure Input mode
   P1DIR &= ~BIT1;
  
   // Clear P1SEL bits to disable Timer function and use as GPIO/Analog
   P1SEL0 &= ~BIT1;
   P1SEL1 &= ~BIT1;
  
   // Disable pull-up/pull-down resistor
   P1REN &= ~BIT1;
  
   // Disable digital input buffer for P1.1 (A1) - enable analog function
   // On FR2xx: SYSCFG2 register controls ADC pin connections
   // SYSCFG2 bit positions correspond to pin numbers
   // For P1.1 (A1), we need to set the appropriate bit in SYSCFG2
   // ADCPCTL1 means ADC Pin Control for pin 1
   // On FR2xx, this is typically bit 1 of SYSCFG2
   SYSCFG2 |= BIT1; // ADCPCTL1 - Enable ADC on P1.1
  
   // Also ensure P1.1 is not used by any other peripheral
   // Clear any other function selects that might interfere

   // 3. Configure ADC (12-bit)
   // Disable ENC to modify
   ADCCTL0 &= ~ADCENC;

   // ADCCTL0:
   // ADCSHT_2: 16 clocks sample hold time.
   // ADCON: Enable ADC core.
   // ADCREF_0: Vref+ = AVCC, Vref- = AVSS (default)
   ADCCTL0 = ADCSHT_2 | ADCON;

   // ADCCTL1:
   // ADCSHS_0: Software trigger (we'll trigger manually in interrupt)
   // ADCSHP: Sample-and-hold pulse mode
   // ADCCONSEQ_0: Single-channel, single-conversion mode
   // We'll manually trigger the ADC in the Timer interrupt
   // Note: ADCCONSEQ_2 (repeat mode) doesn't seem to work reliably on this device,
   // so we use software trigger instead. The CPU overhead is minimal (~0.1% at 1MHz).
   ADCCTL1 = ADCSHS_0 | ADCSHP | ADCCONSEQ_0;

   // ADCCTL2: 12-bit resolution
   ADCCTL2 = ADCRES_2;

   // ADCMCTL0: Input Channel A1 (ADCINCH_1), Vref=AVCC (Default)
   ADCMCTL0 = ADCINCH_1;

   // Enable ADC conversion (waiting for trigger)
   ADCCTL0 |= ADCENC;

   // 4. Configure Timer A2 (TA2) for 9600Hz @ 8MHz
   // Period = 8000000 / 9600 = 833.33... ≈ 833
   TA2CCR0 = 833 - 1;

   // Trigger signal on TA2.1 (ADC trigger)
   // Set CCR1 to trigger ADC - this will generate a pulse on TA2.1 output
   // The ADC trigger happens on the rising edge of TA2.1
   // Set CCR1 to a value that gives enough time for the signal to stabilize
   // Use a smaller value to trigger earlier in the period
   // At 8 MHz, scale proportionally: 10 * 8 = 80
   TA2CCR1 = 80; // Trigger early in the period to give more time for conversion
  
   // Configure TA2.1 output for ADC trigger
   // OUTMOD_7: Reset/Set mode - output goes HIGH when timer = CCR1, LOW when timer = CCR0
   // This creates a pulse that triggers the ADC on the rising edge
   // The trigger signal is internal - doesn't need to go to a pin
   // The ADC will automatically restart on each trigger in ADCCONSEQ_2 mode
   TA2CCTL1 = OUTMOD_7; // Reset/Set - generates pulse on TA2.1

   // Interrupt on TA2CCR0 (Period match) - used to trigger ADC and save value
   TA2CCTL0 = CCIE;

   // Start Timer: SMCLK, Up Mode, Clear
   TA2CTL = TASSEL__SMCLK | MC__UP | TACLR;
}

#define DALI_PHY_ADC_THRESHOLD 2000

#define DALI_PHY_STOP_CONDITION_LENGTH 25

#define DALI_PHY_MODE_IDLE 0x00
#define DALI_PHY_MODE_RECEIVE 0x02
#define DALI_PHY_MODE_INVALID_FRAME 0x04
#define DALI_PHY_MODE_MAX 0x04

//#define majority_filter3_old(x) ((x + (x&3) + 2) & 8)

static const uint8_t majority_filter3[8] = {0,0,0,1,0,1,1,1};

volatile uint8_t g_dali_last_three_unfiltered_samples = 0;
volatile uint8_t g_dali_last_filtered_sample = 8;
volatile uint8_t g_dali_mode = DALI_PHY_MODE_IDLE;
volatile uint8_t g_dali_current_run_length = 0;

volatile uint32_t g_dali_current_frame = 0;
volatile uint8_t g_dali_current_frame_length = 0;
volatile uint8_t g_dali_frame_processing = 0;  // Flag to prevent re-entrancy

volatile uint8_t g_dali_manchester_state = 0;

inline void dali_phy_init_frame(void) {
   g_dali_current_frame = 0;
   g_dali_manchester_state = 0;
   g_dali_current_frame_length = 0;
}

inline void dali_phy_build_frame(uint8_t filtered_sample) {
   if(g_dali_manchester_state == 0) {
       g_dali_current_frame <<= 1;
       g_dali_current_frame |= filtered_sample ^ 1;
       g_dali_current_frame_length++;
   } else if((g_dali_current_frame & 1) != filtered_sample) {
       g_dali_mode = DALI_PHY_MODE_INVALID_FRAME;
   }

   g_dali_manchester_state ^= 1;
}

// Process received DALI frame
// This function is called from interrupt with nested interrupts enabled
// It can take time, and new interrupts can occur during processing
void dali_process_frame(uint32_t frame, uint8_t frame_length, bool is_valid)
{
   if(!is_valid) {
       uart_send_string("Invalid DALI frame: ");
   } else {
       uart_send_string("DALI frame: ");
   }
   uart_send_hex((uint8_t*)&frame, (frame_length+7)/8);
   uart_send_string(" (length: ");
   uart_send_uint_dec(frame_length);
   uart_send_string(")");
   uart_send_string("\r\n");
}

#pragma vector = TIMER2_A0_VECTOR
__interrupt void TA2_CCR0_ISR(void)
{
    P2OUT |= BIT0;
   // clean interrupt flag
   TA2CCTL0 &= ~CCIFG;
  
   uint16_t adc_value = ADCMEM0;
   
   ADCCTL0 |= ADCSC; // Software trigger to start next conversion
   g_dali_last_three_unfiltered_samples >>= 1;
   if(adc_value > DALI_PHY_ADC_THRESHOLD) { 
       g_dali_last_three_unfiltered_samples |= 4;
   }
   uint8_t filtered_sample = majority_filter3[g_dali_last_three_unfiltered_samples];
   bool frame_ready = false, frame_valid = false;
  
   g_adc_last_value = filtered_sample;


   
    switch(__even_in_range(g_dali_mode, DALI_PHY_MODE_MAX)) {
        case DALI_PHY_MODE_IDLE:
            if(!filtered_sample) {
                dali_phy_init_frame();
                g_dali_mode = DALI_PHY_MODE_RECEIVE;
            }
            break;
        case DALI_PHY_MODE_RECEIVE:
            if(filtered_sample != g_dali_last_filtered_sample) {
                if(g_dali_current_run_length >= 3 && g_dali_current_run_length <= 5) {
                    dali_phy_build_frame(g_dali_last_filtered_sample);
                } else if(g_dali_current_run_length >= 7 && g_dali_current_run_length <= 9) {
                    dali_phy_build_frame(g_dali_last_filtered_sample);
                    dali_phy_build_frame(g_dali_last_filtered_sample);
                } else {
                    g_dali_mode = DALI_PHY_MODE_INVALID_FRAME;
                }
            }
            if(!filtered_sample && g_dali_current_run_length >= 10) {
                g_dali_mode = DALI_PHY_MODE_INVALID_FRAME;
            } else if(filtered_sample && g_dali_current_run_length >= DALI_PHY_STOP_CONDITION_LENGTH) {
                g_dali_mode = DALI_PHY_MODE_IDLE;
                frame_ready = true;
                frame_valid = true;
            }
            break;
        case DALI_PHY_MODE_INVALID_FRAME:
            if(filtered_sample && g_dali_current_run_length >= DALI_PHY_STOP_CONDITION_LENGTH) {
                g_dali_mode = DALI_PHY_MODE_IDLE;
                frame_ready = true;
            }
            break;
        
        default:
            __never_executed();
            break;
    }

    if(filtered_sample == g_dali_last_filtered_sample) {
        g_dali_current_run_length++;
    } else {
        g_dali_last_filtered_sample = filtered_sample;
        g_dali_current_run_length = 1;   
    }
   
   

   if(frame_ready && !g_dali_frame_processing) {
       g_dali_frame_processing = 1;
      
       __enable_interrupt();
       dali_process_frame(g_dali_current_frame, g_dali_current_frame_length, frame_valid);
       g_dali_frame_processing = 0;
   }
   P2OUT &= ~BIT0;
}

int main(void)
{
   disable_watchdog();

   clock_init_8mhz();
   
   __delay_cycles(800000);        // Wait ~100ms at ~8MHz for clocks to stabilize

   gpio_init();
   uart_init();

   adc_init();
   
   

   // Configure P1.5 as GPIO Output
   // Stop Timer A1 first (already done in adc_init, but ensure it's stopped)
   TA1CTL = MC__STOP | TACLR;
   TA1CCTL1 = 0;
  
   // Clear P1SEL bits to disconnect Timer function
   P1SEL0 &= ~BIT5;
   P1SEL1 &= ~BIT5;
  
   /*// Set as GPIO output
   P1DIR |= BIT5;
   P1OUT &= ~BIT5;*/

   P2DIR |= BIT0 | BIT1;
   P2OUT &= ~(BIT0 | BIT1);

   __bis_SR_register(GIE);        // Enable global interrupts

   uint16_t loop_counter = 0;

   uart_send_string("==== DALI PHY test ====\r\n");
   while(1) {
       /*loop_counter++;
       if (loop_counter >= 10) {
           loop_counter = 0;
          
           // Toggle P1.5
           P1OUT ^= BIT5;
           uart_send_string("toggle\r\n");
       }*/

       /*uart_send_string("ADC A1: ");
       uart_send_uint_dec(g_adc_last_value);
       uart_send_string("\r\n");*/

       __delay_cycles(8000000); // 100ms delay at 8MHz
   }
}

