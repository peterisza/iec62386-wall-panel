#include <msp430.h>



#include "rom_headers/rom_captivate_msp430fr2676_family.h"
/* CapTIvate alap típusok és app API */
#include "captivate.h"
#include "CAPT_Type.h"

/* A generált konfiguráció NEVEIT is ez adja (g_uiApp, g_*Sensor, stb.) */
#include "CAPT_UserConfig.h"

/* ROM + MAP (ebben a sorrendben!) */
#include "rom_captivate.h"
#include "rom_map_captivate.h"


#define BUZZ1   BIT1        // P1.1
#define BUZZ2   BIT5        // P1.5
#define HALF_PERIOD_CYCLES  125   // ~250 us @ 1 MHz  -> ~2 kHz (500 us periódus)

static void beep_2k_short(void)
{
    // ~100 ms-nyi beep: 100 ms / 250 us = 400 félperiódus togglé
    const unsigned int toggles = 150;
    unsigned int i;

    // Kezdőállapot: ellentétes fázis
    // BUZZ1 magas, BUZZ2 alacsony -> majd mindkettőt egyszerre togglézzuk
    P1OUT = (P1OUT & ~(BUZZ1 | BUZZ2)) | BUZZ1;

    for (i = 0; i < toggles; i++) {
        P1OUT ^= (BUZZ1 | BUZZ2);
        __delay_cycles(HALF_PERIOD_CYCLES);
    }

    // Leállítás: mindkettő alacsony (nincs DC a piezón)
    P1OUT &= ~(BUZZ1 | BUZZ2);
}

void uart_send_string(const char* str) {
    while (*str) {
        while (!(UCA0IFG & UCTXIFG));  // Wait for TX buffer to be ready
        UCA0TXBUF = *str++;            // Send character
    }
    // Wait for transmission to complete
    while (!(UCA0IFG & UCTXIFG));
}

void uart_send_char(char c) {
    while (!(UCA0IFG & UCTXIFG));      // Wait for TX buffer to be ready
    UCA0TXBUF = c;                     // Send character
    // Wait for transmission to complete
    while (!(UCA0IFG & UCTXIFG));
}

void delay_ms(volatile unsigned int ms) {
    while (ms--) {
        __delay_cycles(4000);  // Approximate 1ms delay at 4MHz
    }
}

// Initialize UART 
void uart_init(void) {
    // Temporarily disable clock_init to restore UART functionality
    
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
    
    // Back to working 9600 baud configuration
    // With default SMCLK (~1.05MHz): target = 1050000 / (9600 * 16) = 6.84
    // Use UCBR = 6, UCBRF = 9 (6 + 9/16 = 6.5625) for closest match to 6.84
    UCA0BR0 = 6;                       // Base divisor = 6
    UCA0BR1 = 0;                       // High byte of baud rate divisor  
    UCA0MCTLW = UCOS16 | UCBRF_9;      // Fractional part 9/16 = 0.5625
    
    UCA0CTLW0 &= ~UCSWRST;             // Release UART from reset
}


int main(void){
    WDTCTL = WDTPW | WDTHOLD;      // watchdog off
    PM5CTL0 &= ~LOCKLPM5;          // FRxx: GPIO-k engedélyezése

    // Válasszuk a GPIO funkciót ezekre a pinekre
    P1SEL0 &= ~(BUZZ1 | BUZZ2);
    P1SEL1 &= ~(BUZZ1 | BUZZ2);

    // Irány és kezdeti szint
    P1DIR  |=  (BUZZ1 | BUZZ2);
    P1OUT  &= ~(BUZZ1 | BUZZ2);

    // Rövid ~2 kHz beep
    delay_ms(500);
    beep_2k_short();

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
    
    // Send debug message at startup - back to 9600 baud
    uart_send_string("MSP430FR2675 started at 9600 baud\r\n");
    
    // Initialize the complete CapTIvate system first
    CAPT_appStart();
    uart_send_string("CapTIvate system started\r\n");
    
    // Now we can access individual sensors
    uart_send_string("Sensor initialized\r\n");
    uart_send_string("Sensor calibrated\r\n");

    for (;;)
    {
        // Update the complete UI instead of individual sensors
        CAPT_updateUI(&g_uiApp);
        //uart_send_string("UI updated\r\n");
        
        /*if (rising & (BIT0 | BIT1)) {
            beep_2k_short();
        }*/

        // Access elements through the cycle structure
        uart_send_char(arrows.pCycle[0]->pElements[0]->bTouch ? '1' : '0');
        uart_send_char(arrows.pCycle[0]->pElements[1]->bTouch ? '1' : '0');
        uart_send_string("\r\n");

        __delay_cycles(1000);
    }

}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
    // Polarításfüggetlen váltás: 10 <-> 01
    if (P2OUT & BIT0) {
        P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT1;
    } else {
        P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;
    }
    // CCR0 flag általában automatikusan törlődik, külön nem kell
}
