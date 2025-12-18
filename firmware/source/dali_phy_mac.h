#ifndef DALI_PHY_MAC_H
#define DALI_PHY_MAC_H

#include <stdint.h>
#include <stdbool.h>

// DALI PHY constants
#define DALI_PHY_ADC_THRESHOLD 1000
#define DALI_PHY_STOP_CONDITION_LENGTH 25

#define DALI_PHY_BUS_IDLE_BEFORE_SENDING 200
#define DALI_PHY_BUS_IDLE_BEFORE_SENDING_JITTER 20

#define DALI_PHY_FRAME_SEND_TWICE_THRESHOLD 720 // 75 ms at 9600 baud

#define DALI_PHY_BREAK_CYCLES       13  // ~1.35ms (Standard: 1.2ms - 1.4ms)
#define DALI_PHY_RECOVER_CYCLES     40  // ~4.16ms (Standard: 4.0ms - 4.6ms)

#define DALI_SEND_MAX_RETRIES 10

// DALI TX control macros
#define dali_tx_activate() {P1OUT |= BIT7;}
#define dali_tx_deactivate() {P1OUT &= ~BIT7;}
#define is_dali_tx_active() (P1OUT & BIT7)

// Callback function type for received DALI frames
// Parameters: frame (32-bit frame data), frame_length (number of bits), is_valid (true if frame is valid)
typedef void (*dali_frame_callback_t)(uint32_t frame, uint8_t frame_length, bool is_valid, bool received_twice);

// Function declarations
void dali_phy_init(void);
void dali_tx_send_frame(uint32_t frame, uint8_t frame_length);
void dali_tx_send_backward_frame(uint8_t frame, uint8_t watch_bus_cycles);
void dali_phy_set_frame_callback(dali_frame_callback_t callback);

// External variables (for debug/status)
extern volatile uint16_t g_adc_last_value;

#endif // DALI_PHY_MAC_H

