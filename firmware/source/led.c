#include "led.h"
#include <msp430.h>

void led_init(void)
{   
    // P2.0 kimenetként
    P2DIR |= BIT0;
    
    // P2.0 = LOW
    P2OUT &= ~BIT0;  // P2.0 = LOW
}

void led_on(void)
{
    P2OUT |= BIT0;
}

void led_off(void)
{
    P2OUT &= ~BIT0;
}