// MSP430FR2675  — UCA0TXD on P1.4  — 250000 baud, 8N1
#include <msp430.h>

static void uart_putc(char c) {
    while (!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = c;
}
static void uart_write(const char *s) {
    while (*s) uart_putc(*s++);
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;          // stop watchdog
    PM5CTL0 &= ~LOCKLPM5;              // unlock GPIO (FRxx)

    // ---- Pin mux: P1.4 = UCA0TXD, P1.5 = UCA0RXD (if you ever need RX) ----
    P1SEL1 &= ~(BIT4 | BIT5);          // select primary module function
    P1SEL0 |=  (BIT4 | BIT5);
    P1DIR  |=  BIT4;                   // TX as output

    // ---- eUSCI_A0 UART @ 250000 baud from default ~1 MHz SMCLK ----
    UCA0CTLW0 = UCSWRST;               // hold in reset
    UCA0CTLW0 |= UCSSEL__SMCLK;        // clock = SMCLK (~1 MHz by default)
    // 1,000,000 / 250,000 = 4  -> no oversampling, BRW=4, MCTLW=0
    UCA0BRW   = 4;
    UCA0MCTLW = 0;
    UCA0CTLW0 &= ~UCSWRST;             // release for operation

    for (;;) {
        uart_write("Hello @250k baud\r\n");
        __delay_cycles(1000000UL);     // ~1 s at ~1 MHz SMCLK
    }
}
