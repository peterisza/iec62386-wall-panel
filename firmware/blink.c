#include <msp430.h>

static void delay(volatile unsigned long c){ while(c--) __no_operation(); }

int main(void){
    WDTCTL = WDTPW | WDTHOLD;          // Watchdog off
    PM5CTL0 &= ~LOCKLPM5;              // <-- FRAM: GPIO-k levétele high-Z-ről

    // P2.0 és P2.1: GPIO kimenet
    P2SEL0 &= ~(BIT0 | BIT1);
    P2SEL1 &= ~(BIT0 | BIT1);
    P2DIR  |=  (BIT0 | BIT1);

    // Kezdőállapot: 10
    P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;

    for(;;){
        delay(25000UL);
        // 10 <-> 01 váltogatás (polaritásfüggetlen hajtás)
        if (P2OUT & BIT0) {
            P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT1;
        } else {
            P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;
        }
    }
}
