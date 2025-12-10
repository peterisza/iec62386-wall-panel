#include <msp430.h>
#include "dali_phy_mac.h"
#include "uart.h"
#include "util.h"
#include "random.h"

// Majority filter lookup table
static const uint8_t majority_filter3[8] = {0,0,0,1,0,1,1,1};

volatile uint8_t g_mode;
// DALI PHY mode definitions
#define DALI_PHY_MODE_IDLE 0x00
#define DALI_PHY_MODE_RECEIVE 0x02
#define DALI_PHY_MODE_INVALID_FRAME 0x04
#define DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING 0x06
#define DALI_PHY_MODE_SEND 0x08
#define DALI_PHY_MODE_BREAK_AFTER_COLLISION 0x0A
#define DALI_PHY_MODE_RECOVERY_AFTER_COLLISION 0x0C
#define DALI_PHY_MODE_MAX 0x0C

// DALI PHY state variables
volatile uint8_t g_last_three_unfiltered_samples = 0;
volatile uint8_t g_last_filtered_sample = 8;
volatile uint8_t g_mode = DALI_PHY_MODE_IDLE;
volatile uint8_t g_current_run_length = 0;

volatile uint32_t g_current_frame = 0;
volatile uint8_t g_current_frame_length = 0;
volatile uint32_t g_last_received_frame = 0;
volatile uint8_t g_last_received_frame_length = 0;
volatile uint8_t g_frame_processing = 0;  // Flag to prevent re-entrancy

volatile uint8_t g_manchester_state = 0;
volatile uint8_t g_watch_bus_before_sending_counter = 0;
volatile uint32_t g_frame_to_send_with_start_bit = 0;
volatile uint32_t g_last_frame_to_send = 0;
volatile uint8_t g_frame_to_send_length_cycles = 0;
volatile uint8_t g_last_frame_to_send_length = 0;
volatile uint8_t g_is_backward_frame = 0;

volatile uint8_t g_break_after_collision_counter = 0;
volatile uint8_t g_send_retries = 0;
volatile uint16_t g_adc_last_value = 0;

// Callback function for received frames (NULL if not set)
static dali_frame_callback_t g_frame_callback = 0;

struct dali_tx_frame_t {
    uint32_t frame;
    uint8_t frame_length;
};

typedef struct dali_tx_frame_t dali_tx_frame_t;

#define DALI_PHY_TX_BUFFER_SIZE 10
volatile dali_tx_frame_t g_tx_buffer[DALI_PHY_TX_BUFFER_SIZE];
volatile uint8_t g_tx_buffer_start = 0;
volatile uint8_t g_tx_buffer_end = 0;

// Internal helper functions
static inline void dali_phy_init_frame(void) {
   g_current_frame = 0;
   g_manchester_state = 0;
   g_current_frame_length = 0;
}

static inline void dali_phy_build_frame(uint8_t filtered_sample) {
   if(g_manchester_state == 0) {
       g_current_frame <<= 1;
       g_current_frame |= filtered_sample ^ 1;
       g_current_frame_length++;
   } else if((g_current_frame & 1) != filtered_sample) {
       g_mode = DALI_PHY_MODE_INVALID_FRAME;
   }

   g_manchester_state ^= 1;
}

void dali_phy_init(void)
{ 
    // P1.7: DALI_TX output
    P1DIR |= BIT7;
    P1OUT &= ~BIT7;

    // P1.0: DALI_RX analog input (A0)
    P1DIR &= ~BIT0;
    P1REN &= ~BIT0;                 // no pull
    P1SEL0 &= ~BIT0;                // maradhat GPIO
    P1SEL1 &= ~BIT0;
    SYSCFG2 |= BIT0;                // ADCPCTL0 – ADC on P1.0

    // (Ha P2.0-at villogtatod az ISR-ben, itt érdemes:)
    P2DIR |= BIT0;
    P2OUT &= ~BIT0;
 

    ADCCTL0 &= ~ADCENC;
    ADCCTL0 = ADCSHT_2 | ADCON;

    // Hardware trigger from RTC event, repeat single channel
    // ADCSHS_3: RTC event trigger (check datasheet for correct value)
    // If ADCSHS_3 doesn't work, try ADCSHS_2 or check datasheet
    ADCCTL1 = ADCSHS_1        // RTC event trigger
            | ADCSHP
            | ADCCONSEQ_2     // repeat-single-channel
            | ADCSSEL_2;      // ADC clock = SMCLK

    ADCCTL2 = ADCRES_2;       // 12 bit
    ADCMCTL0 = ADCINCH_0;     // A0

    ADCIE |= ADCIE0;          // MEM0 interrupt enable

    ADCCTL0 |= ADCENC;       

    // Configure RTC: Clock source SMCLK, prescaler 1
    RTCCTL = RTCSS__SMCLK | RTCPS__1;

    // Period: 8 MHz / 9600 ≈ 833
    RTCMOD = 833 - 1;
}

// Send a DALI frame
void dali_tx_start_sending_frame(uint32_t frame, uint8_t frame_length, uint8_t watch_bus_cycles, bool is_backward_frame) {
    // Disable Timer A2 interrupt to prevent race conditions
    TA2CCTL0 &= ~CCIE;

    g_last_frame_to_send = frame;
    g_last_frame_to_send_length = frame_length;

    g_frame_to_send_with_start_bit = 0;
    for(uint8_t i = 0; i < frame_length; i++) {
        g_frame_to_send_with_start_bit |= (frame & 1) ^ 1;
        g_frame_to_send_with_start_bit <<= 1;
        frame >>= 1;
    }
    g_frame_to_send_length_cycles = frame_length*8+15;
    g_mode = DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING;
    g_watch_bus_before_sending_counter = watch_bus_cycles;
    g_is_backward_frame = is_backward_frame;

    // Re-enable Timer A2 interrupt
    TA2CCTL0 |= CCIE;
    
    g_debug_index = 0;
    uart_send_string("Sending frame: ");
    uart_send_hex((uint8_t*)&g_frame_to_send_with_start_bit, (frame_length+7)/8);
    uart_send_string("\r\n");
}

void dali_tx_send_backward_frame(uint8_t frame, uint8_t watch_bus_cycles) {
    dali_tx_start_sending_frame(frame, 8, watch_bus_cycles, true);
}

void dali_tx_send_frame(uint32_t frame, uint8_t frame_length) {
    // Disable Timer A2 interrupt to prevent race conditions on buffer
    TA2CCTL0 &= ~CCIE;
    
    g_tx_buffer[g_tx_buffer_end].frame = frame;
    g_tx_buffer[g_tx_buffer_end].frame_length = frame_length;
    g_tx_buffer_end++;
    if (g_tx_buffer_end >= DALI_PHY_TX_BUFFER_SIZE) {
        g_tx_buffer_end = 0;
    }
    
    // Re-enable Timer A2 interrupt
    TA2CCTL0 |= CCIE;
}

// Set callback function for received DALI frames
// If callback is NULL, the default processing will be used
void dali_phy_set_frame_callback(dali_frame_callback_t callback)
{
    g_frame_callback = callback;
}

inline void dali_tx_pop_front_buffer(void) {
    if(g_tx_buffer_end == g_tx_buffer_start)
        return;
    g_tx_buffer_start++;
    if(g_tx_buffer_start >= DALI_PHY_TX_BUFFER_SIZE) {
        g_tx_buffer_start = 0;
    }
}

// ADC interrupt handler for DALI PHY processing
// This is called at 9600 Hz when ADC conversion is complete
#pragma vector = ADC_VECTOR
__interrupt void ADC_ISR(void)
{
    P2OUT |= BIT0;
    
    // Read ADC value (reading ADCIV clears the interrupt flag)
    uint16_t adc_value = ADCMEM0;
    
    g_last_three_unfiltered_samples >>= 1;
    if(adc_value > DALI_PHY_ADC_THRESHOLD) { 
        g_last_three_unfiltered_samples |= 4;
    }
    uint8_t filtered_sample = majority_filter3[g_last_three_unfiltered_samples];
    bool frame_ready = false, frame_valid = false;
    
    g_adc_last_value = filtered_sample;
   
    switch(__even_in_range(g_mode, DALI_PHY_MODE_MAX)) {
        case DALI_PHY_MODE_IDLE:
            if(!filtered_sample) {
                dali_phy_init_frame();
                g_mode = DALI_PHY_MODE_RECEIVE;
            } else if(
                g_tx_buffer_end != g_tx_buffer_start &&
                adc_value > DALI_PHY_ADC_THRESHOLD &&
                g_current_run_length >= DALI_PHY_BUS_IDLE_BEFORE_SENDING) {
                    dali_tx_start_sending_frame(
                        g_tx_buffer[g_tx_buffer_start].frame,
                        g_tx_buffer[g_tx_buffer_start].frame_length,
                        random_range(DALI_PHY_BUS_IDLE_BEFORE_SENDING_JITTER)+5,
                        false
                    );
            } else if(g_current_run_length >= DALI_PHY_FRAME_SEND_TWICE_THRESHOLD) {
                g_last_received_frame = 0;
                g_last_received_frame_length = 0;
            } 
            break;
        case DALI_PHY_MODE_RECEIVE:
            if(filtered_sample != g_last_filtered_sample) {
                if(g_current_run_length >= 3 && g_current_run_length <= 5) {
                    dali_phy_build_frame(g_last_filtered_sample);
                } else if(g_current_run_length >= 7 && g_current_run_length <= 9) {
                    dali_phy_build_frame(g_last_filtered_sample);
                    dali_phy_build_frame(g_last_filtered_sample);
                } else {
                    g_mode = DALI_PHY_MODE_INVALID_FRAME;
                }
            }
            if(!filtered_sample && g_current_run_length >= 10) {
                g_mode = DALI_PHY_MODE_INVALID_FRAME;
            } else if(filtered_sample && g_current_run_length >= DALI_PHY_STOP_CONDITION_LENGTH) {
                g_mode = DALI_PHY_MODE_IDLE;
                frame_ready = true;
                frame_valid = true;
            }
            break;
        case DALI_PHY_MODE_INVALID_FRAME:
            if(filtered_sample && g_current_run_length >= DALI_PHY_STOP_CONDITION_LENGTH) {
                g_mode = DALI_PHY_MODE_IDLE;
                frame_ready = true;
            }
            break;
        case DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING:
            if(adc_value <= DALI_PHY_ADC_THRESHOLD && !g_is_backward_frame) {
                g_mode = DALI_PHY_MODE_IDLE;
            } else {
                g_watch_bus_before_sending_counter--;
                if(g_watch_bus_before_sending_counter == 0) {
                    g_mode = DALI_PHY_MODE_SEND;
                }
            }
            break;
        case DALI_PHY_MODE_SEND:
            /*uart_send_hex_byte(adc_value >> 8);
            uart_send_hex_byte(adc_value & 0xFF);
            uart_send_string(" ");*/
            if(!g_is_backward_frame && !is_dali_tx_active() && adc_value <= DALI_PHY_ADC_THRESHOLD) {
                g_send_retries++;
                g_mode = DALI_PHY_MODE_BREAK_AFTER_COLLISION;
                g_break_after_collision_counter = DALI_PHY_BREAK_CYCLES;
                dali_tx_activate();
                break;
            }
            uint8_t current_bus_level =
                (((uint8_t)g_frame_to_send_with_start_bit) ^
                    (g_frame_to_send_length_cycles >> 2)) & 1;

            if(current_bus_level) {
                dali_tx_activate();
            } else {
                dali_tx_deactivate();
            }
            if(g_frame_to_send_length_cycles == 0) {
                g_mode = DALI_PHY_MODE_IDLE;
                dali_tx_deactivate();
                if(!g_is_backward_frame) {
                    dali_tx_pop_front_buffer();
                }
                g_send_retries = 0;
            }
            if((g_frame_to_send_length_cycles & 7) == 0) {
                g_frame_to_send_with_start_bit >>= 1;
            }
            g_frame_to_send_length_cycles--;
            break;
        case DALI_PHY_MODE_BREAK_AFTER_COLLISION:
            if(g_break_after_collision_counter == 0) {
                dali_tx_deactivate();
                if(g_send_retries < DALI_SEND_MAX_RETRIES) {
                    //uart_send_string("Resending frame after collision\r\n");
                    dali_tx_start_sending_frame(
                        g_last_frame_to_send,
                        g_last_frame_to_send_length,
                        DALI_PHY_RECOVER_CYCLES,
                        false);
                } else {
                    /*uart_send_string("Failed to send frame after ");
                    uart_send_uint_dec(g_send_retries);
                    uart_send_string(" retries\r\n");*/
                    g_mode = DALI_PHY_MODE_IDLE;
                    if(!g_is_backward_frame) {
                        dali_tx_pop_front_buffer();
                    }
                    g_send_retries = 0;
                }
            }
            g_break_after_collision_counter--;
            break;
        default:
            __never_executed();
            break;
    }

    if(filtered_sample == g_last_filtered_sample) {
        g_current_run_length++;
    } else {
        g_last_filtered_sample = filtered_sample;
        g_current_run_length = 1;   
    }
   
    if(frame_ready && !g_frame_processing && g_frame_callback != 0) {
        bool is_received_twice =
            g_current_frame_length == g_last_received_frame_length &&
            g_current_frame == g_last_received_frame;

        g_last_received_frame = g_current_frame;
        g_last_received_frame_length = g_current_frame_length;

        g_frame_processing = 1;

        __enable_interrupt();
        g_frame_callback(
            g_current_frame,
            g_current_frame_length,
            frame_valid,
            is_received_twice
        );

        g_frame_processing = 0;
    }
    P2OUT &= ~BIT0;
}

