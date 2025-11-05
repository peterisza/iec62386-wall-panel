#include "watchdog.h"
#include <msp430.h>

void disable_watchdog(void)
{
    WDTCTL = WDTPW | WDTHOLD;
}