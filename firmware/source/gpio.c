#include "gpio.h"
#include <msp430.h>

void gpio_init(void)
{
    PM5CTL0 &= ~LOCKLPM5;
    
    // P2.0 kimenetként
    P2DIR |= BIT0;
    
    // P2.0 = LOW
    P2OUT &= ~BIT0;  // P2.0 = LOW
}