#include <msp430.h>

// Note: MSP430FR2675 does not define CSKEY constant in headers
// Clock system registers may be directly writable on this device

// UART debug message function
void uart_send_string(const char* str) {
    while (*str) {
        while (!(UCA0IFG & UCTXIFG));  // Wait for TX buffer to be ready
        UCA0TXBUF = *str++;            // Send character
    }
    // Wait for transmission to complete
    while (!(UCA0IFG & UCTXIFG));
}

// Send single character for simple oscilloscope testing
void uart_send_char(char c) {
    while (!(UCA0IFG & UCTXIFG));      // Wait for TX buffer to be ready
    UCA0TXBUF = c;                     // Send character
    // Wait for transmission to complete
    while (!(UCA0IFG & UCTXIFG));
}

// Simple delay function for debugging
void delay_ms(volatile unsigned int ms) {
    while (ms--) {
        __delay_cycles(4000);  // Approximate 1ms delay at 4MHz
    }
}

// Clock initialization for 250k baud (4MHz SMCLK target)
void clock_init(void) {
    // First configure FLL for higher DCO frequency
    // Target: 4MHz from 32768Hz REFO → FLLN = 122 (4,000,000 / 32,768 ≈ 122)
    CSCTL2 = (CSCTL2 & ~0x03FF) | 122; // FLLN = 122
    
    // Try to configure SMCLK to use DCOCLKDIV
    // For MSP430FR2675, SMCLK source might be in CSCTL4 or different register
    // Let's try a simpler approach - just set up DCO and see what happens
    
    // Clear fault flags and wait for FLL lock
    CSCTL7 &= ~(DCOFFG | FLLULIFG);    // Clear fault flags
    do {
        SFRIFG1 &= ~OFIFG;             // Clear oscillator fault flag
        CSCTL7 &= ~(DCOFFG | FLLULIFG); // Clear fault flags again
    } while (SFRIFG1 & OFIFG);         // Wait until oscillator is stable
    
    // Try to configure SMCLK source after FLL is locked
    CSCTL4 &= ~SELMS;                  // Clear SMCLK selection bits
    CSCTL4 |= SELMS_0;                 // Select DCOCLKDIV for SMCLK
}

// Initialize UART 
void uart_init(void) {
    // Temporarily disable clock_init to restore UART functionality
    // clock_init();
    
    // Configure UCA0TXD on P1.4 for MSP430FR2675
    P1SEL0 |= BIT4;                    // Enable UART function on P1.4
    P1SEL1 &= ~BIT4;
    
    // Ensure pin starts HIGH as GPIO before switching to UART function
    P1OUT |= BIT4;                     // Set pin HIGH before enabling UART
    P1DIR |= BIT4;                     // Set as output initially
    
    // UART idle state should be HIGH (mark state)
    
    // Configure UART module - explicit 8N1 settings
    UCA0CTLW0 |= UCSWRST;              // Put UART in reset state
    UCA0CTLW0 |= UCSSEL__SMCLK;        // Use SMCLK as clock source
    
    // Explicitly set 8N1 format (8 data bits, no parity, 1 stop bit)
    UCA0CTLW0 &= ~UCSPB;               // 1 stop bit (clear UCSPB)
    UCA0CTLW0 &= ~UCPEN;               // No parity (clear UCPEN)
    UCA0CTLW0 &= ~UCPAR;               // Even parity if enabled (clear UCPAR)
    UCA0CTLW0 &= ~UCMSB;               // LSB first (clear UCMSB)
    UCA0CTLW0 &= ~UC7BIT;              // 8 data bits (clear UC7BIT)
    
    // Try higher baud rate: 57600 baud
    // With SMCLK (~1.05MHz): target = 1050000 / (57600 * 16) = 1.14
    // Use UCBR = 1, UCBRF = 2 (1 + 2/16 = 1.125) for closest match
    UCA0BR0 = 1;                       // Base divisor = 1
    UCA0BR1 = 0;                       // High byte of baud rate divisor  
    UCA0MCTLW = UCOS16 | UCBRF_2;      // Fractional part 2/16 = 0.125
    
    UCA0CTLW0 &= ~UCSWRST;             // Release UART from reset
}

int main(void){
    WDTCTL = WDTPW | WDTHOLD;      // watchdog off
    PM5CTL0 &= ~LOCKLPM5;          // FRxx: GPIO-k engedélyezése
    
    // P2.0, P2.1 kimenet, kezdőállapot 10
    P2SEL0 &= ~(BIT0 | BIT1);
    P2SEL1 &= ~(BIT0 | BIT1);
    P2REN  &= ~(BIT0 | BIT1);
    P2DIR  |=  (BIT0 | BIT1);
    P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;

    // Timer_A0: ACLK forrás, up mode, CCR0 megszakítással
    // Alapértelmezésben ACLK egy lassú belső óra (REFO~32768 Hz vagy VLO~10 kHz).
    TA0CTL   = TASSEL__ACLK | ID__1 | MC__UP | TACLR;
    // Ha ACLK=~32768 Hz → 16384 ~ 0.5 s. Ha ACLK=~10 kHz → 5000 ~ 0.5 s.
    TA0CCR0  = 10000;          // Kezdd ezzel; ha túl gyors/lassú, állítsd 5000-re.
    TA0CCTL0 = CCIE;               // engedélyezzük a CCR0 megszakítást

    __bis_SR_register(GIE);        // globál interrupt engedély
    
    // Initialize UART for debug output AFTER timer setup
    uart_init();
    
    // Send debug message at startup - testing 57600 baud
    uart_send_string("MSP430FR2675 started at 57600 baud\r\n");
    
    // Continuous debug loop for UART testing
    for(;;){
        // Send simple patterns for easier recognition
        uart_send_char(0x55);              // 01010101 pattern - easy to see on scope
        uart_send_char(0xAA);              // 10101010 pattern - inverted
        
        // Send clear text message
        uart_send_string("DEBUG 57600 baud\r\n");
        
        // Longer delay to ensure transmission completes and for easier observation
        volatile unsigned long delay = 100000UL;
        while (delay--) {
            __no_operation();
        }
    }
}

// GCC-stílusú ISR a TIMER0_A0 vektorhoz
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TIMER0_A0_ISR(void){
    // Polarításfüggetlen váltás: 10 <-> 01
    if (P2OUT & BIT0) {
        P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT1;
    } else {
        P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;
    }
    // CCR0 flag általában automatikusan törlődik, külön nem kell
}
