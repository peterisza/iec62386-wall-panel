#include "gpio.h"
#include <msp430.h>

void gpio_init(void)
{
    PM5CTL0 &= ~LOCKLPM5;
    
    // P2.0 és P2.1 kimenetként
    P2DIR |= BIT0 | BIT1;
    
    // P2.0 = LOW, P2.1 = HIGH
    P2OUT &= ~BIT0;  // P2.0 = LOW
    P2OUT |= BIT1;   // P2.1 = HIGH
}