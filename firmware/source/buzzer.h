#ifndef BUZZER_H_
#define BUZZER_H_

#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

// P1.1 -> TA0.1, P1.5 -> TA1.1 (ellenfázis)
void buzzer_init(uint32_t smclk_hz);

// Non-blocking indítás: freq_hz (20..40k), duration_ms (1..60000 tipikusan)
bool buzzer_beep(uint16_t freq_hz, uint32_t duration_ms);

// Korai leállítás
void buzzer_stop(void);

// Fut-e éppen?
bool buzzer_busy(void);

#endif