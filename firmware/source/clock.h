#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

void clock_init_8mhz(void);
void delay_ms_8mhz(uint16_t ms);
void delay_ms_1mhz(uint16_t ms);

#endif