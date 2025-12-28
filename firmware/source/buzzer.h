#ifndef BUZZER_H_
#define BUZZER_H_

#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

// P1.1 -> TA0.1, P1.5 -> TA1.1 (ellenfázis)
void buzzer_init(uint32_t smclk_hz);

// Click hang: ellentétes fázisban váltogatja a pin-eket
void buzzer_click(uint16_t cycles1, int16_t cycles2, uint8_t periods);

// Non-blocking indítás: freq_hz (20..40k), duration_ms (1..60000 tipikusan)
bool buzzer_beep(uint16_t freq_hz, uint32_t duration_ms);

// Korai leállítás
void buzzer_stop(void);

// Fut-e éppen?
bool buzzer_busy(void);

// Zajgenerátor: random értékekkel ellentétes fázisban állítja be a két pinit
// freq_hz: timer interrupt frekvencia, duration_ms: hány ms-ig tartson
bool buzzer_noise(uint16_t freq_hz, uint32_t duration_ms);

// Zajgenerátor leállítása
void buzzer_noise_stop(void);

// Fut-e éppen a zajgenerátor?
bool buzzer_noise_busy(void);

#endif