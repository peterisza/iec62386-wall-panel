#ifndef DALI_PHY_MAC_H
#define DALI_PHY_MAC_H

#include <stdint.h>
#include <stdbool.h>

// DALI PHY constants
#define DALI_PHY_ADC_THRESHOLD 1000
#define DALI_PHY_STOP_CONDITION_LENGTH 25

// DALI PHY mode definitions
#define DALI_PHY_MODE_IDLE 0x00
#define DALI_PHY_MODE_RECEIVE 0x02
#define DALI_PHY_MODE_INVALID_FRAME 0x04
#define DALI_PHY_MODE_WATCH_BUS_BEFORE_SENDING 0x06
#define DALI_PHY_MODE_SEND 0x08
#define DALI_PHY_MODE_BREAK_AFTER_COLLISION 0x0A
#define DALI_PHY_MODE_RECOVERY_AFTER_COLLISION 0x0C
#define DALI_PHY_MODE_MAX 0x0C

#define DALI_PHY_BUS_IDLE_BEFORE_SENDING 70
#define DALI_PHY_BUS_IDLE_BEFORE_SENDING_JITTER 20

#define DALI_PHY_BREAK_CYCLES       13  // ~1.35ms (Standard: 1.2ms - 1.4ms)
#define DALI_PHY_RECOVER_CYCLES     40  // ~4.16ms (Standard: 4.0ms - 4.6ms)
// DALI TX control macros
#define dali_tx_activate() {P1OUT |= BIT7;}
#define dali_tx_deactivate() {P1OUT &= ~BIT7;}
#define is_dali_tx_active() (P1OUT & BIT7)

// Function declarations
void dali_phy_init(void);
void dali_tx_send_frame(uint32_t frame, uint8_t frame_length);
void dali_tx_send_backward_frame(uint8_t frame, uint8_t watch_bus_cycles);
void dali_process_frame(uint32_t frame, uint8_t frame_length, bool is_valid);

// External variables (for debug/status)
extern volatile uint8_t g_dali_mode;
extern volatile uint16_t g_adc_last_value;

#endif // DALI_PHY_MAC_H

