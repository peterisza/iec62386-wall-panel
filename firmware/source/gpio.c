#include "gpio.h"
#include <msp430.h>

void gpio_init(void)
{
    PM5CTL0 &= ~LOCKLPM5;
}