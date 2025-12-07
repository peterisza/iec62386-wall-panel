#include <msp430.h>
#include "dali_phy_mac.h"
#include "uart.h"
#include "util.h"

// Majority filter lookup table
static const uint8_t majority_filter3[8] = {0,0,0,1,0,1,1,1};

// DALI PHY state variables
volatile uint8_t g_dali_last_three_unfiltered_samples = 0;
volatile uint8_t g_dali_last_filtered_sample = 8;
volatile uint8_t g_dali_mode = DALI_PHY_MODE_IDLE;
volatile uint8_t g_dali_current_run_length = 0;

volatile uint32_t g_dali_current_frame = 0;
volatile uint8_t g_dali_current_frame_length = 0;
volatile uint8_t g_dali_frame_processing = 0;  // Flag to prevent re-entrancy

volatile uint8_t g_dali_manchester_state = 0;
volatile uint8_t g_dali_watch_bus_before_sending_counter = 0;
volatile uint32_t g_dali_frame_to_send_with_start_bit = 0;
volatile uint8_t g_dali_frame_to_send_length_cycles = 0;

volatile uint16_t g_adc_last_value = 0;

// Internal helper functions
static inline void dali_phy_init_frame(void) {
   g_dali_current_frame = 0;
   g_dali_manchester_state = 0;
   g_dali_current_frame_length = 0;
}

static inline void dali_phy_build_frame(uint8_t filtered_sample) {
   if(g_dali_manchester_state == 0) {
       g_dali_current_frame <<= 1;
       g_dali_current_frame |= filtered_sample ^ 1;
       g_dali_current_frame_length++;
   } else if((g_dali_current_frame & 1) != filtered_sample) {
       g_dali_mode = DALI_PHY_MODE_INVALID_FRAME;
   }

   g_dali_manchester_state ^= 1;
}

// Initialize DALI PHY layer (ADC and Timer configuration)
void dali_phy_init(void)
{ 
   // Configure P1.0 as Analog Input (A0) for DALI_RX
   // Clear P1DIR to ensure Input mode
   P1DIR &= ~BIT0;
  
   // Clear P1SEL bits to disable Timer function and use as GPIO/Analog
   P1SEL0 &= ~BIT0;
   P1SEL1 &= ~BIT0;
  
   // Disable pull-up/pull-down resistor
   P1REN &= ~BIT0;
  
   // Disable digital input buffer for P1.0 (A0) - enable analog function
   // On FR2xx: SYSCFG2 register controls ADC pin connections
   // SYSCFG2 bit positions correspond to pin numbers
   // For P1.0 (A0), we need to set the appropriate bit in SYSCFG2
   // ADCPCTL0 means ADC Pin Control for pin 0
   // On FR2xx, this is typically bit 0 of SYSCFG2
   SYSCFG2 |= BIT0; // ADCPCTL0 - Enable ADC on P1.0
  
   // Also ensure P1.0 is not used by any other peripheral
   // Clear any other function selects that might interfere

   // Configure ADC (12-bit)
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

   // ADCMCTL0: Input Channel A0 (ADCINCH_0), Vref=AVCC (Default)
   ADCMCTL0 = ADCINCH_0;

   // Enable ADC conversion (waiting for trigger)
   ADCCTL0 |= ADCENC;

   // Configure Timer A2 (TA2) for 9600Hz @ 8MHz
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

// Send a DALI frame
void dali_tx_send_frame(uint32_t frame, uint8_t frame_length, uint8_t watch_bus_cycles) {
    g_dali_frame_to_send_with_start_bit = 0;
    for(uint8_t i = 0; i < frame_length; i++) {
        g_dali_frame_to_send_with_start_bit |= (frame & 1) ^ 1;
        g_dali_frame_to_send_with_start_bit <<= 1;
        frame >>= 1;
    }
    g_dali_frame_to_send_length_cycles = frame_length*8+15;
    g_dali_mode = DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING;
    g_dali_watch_bus_before_sending_counter = watch_bus_cycles;
    uart_send_string("Sending frame: ");
    uart_send_hex((uint8_t*)&g_dali_frame_to_send_with_start_bit, (frame_length+7)/8);
    uart_send_string("\r\n");
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
   dali_tx_send_frame(207, 8, 30);
}

// Timer A2 interrupt handler for DALI PHY processing
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
        case DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING:
            if(adc_value <= DALI_PHY_ADC_THRESHOLD) {
                g_dali_mode = DALI_PHY_MODE_IDLE;
            } else {
                g_dali_watch_bus_before_sending_counter--;
                if(g_dali_watch_bus_before_sending_counter == 0) {
                    g_dali_mode = DALI_PHY_MODE_SEND;
                    //dali_tx_activate();
                }
            }
            break;
        case DALI_PHY_MODE_SEND:
            /*if(!is_dali_tx_active() && adc_value <= DALI_PHY_ADC_THRESHOLD) {
                g_dali_mode = DALI_PHY_MODE_BREAK_AFTER_COLLISION;
                break;
            }*/
            uint8_t current_bus_level =
                (((uint8_t)g_dali_frame_to_send_with_start_bit) ^
                    (g_dali_frame_to_send_length_cycles >> 2)) & 1;
            //uart_send_hex(g_dali_frame_to_send_with_start_bit, 1);
            if(current_bus_level) {
                dali_tx_activate();
            } else {
                dali_tx_deactivate();
            }
            if(g_dali_frame_to_send_length_cycles == 0) {
                g_dali_mode = DALI_PHY_MODE_IDLE;
                dali_tx_deactivate();
            }
            if((g_dali_frame_to_send_length_cycles & 7) == 0) {
                g_dali_frame_to_send_with_start_bit >>= 1;
            }
            g_dali_frame_to_send_length_cycles--;
            break;
        case DALI_PHY_MODE_BREAK_AFTER_COLLISION:
            break;
        case DALI_PHY_MODE_RECOVERY_AFTER_COLLISION:
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

