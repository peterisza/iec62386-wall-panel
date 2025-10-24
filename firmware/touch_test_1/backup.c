#include <stdint.h>
#include <msp430.h>

//#include "rom_captivate.h"
//#include "CAPT_Type.h"


//extern tSensor sensor_buttons;      // a kétgombos szenzorod
//extern tCaptivateApplication g_uiApp;

#define HALF_PERIOD_CYCLES 250
#define BUZZ1   BIT1   // P1.1
#define BUZZ2   BIT5   // P1.5


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

void delay_ms(volatile unsigned int ms) {
    while (ms--) {
        __delay_cycles(4000);  // Approximate 1ms delay at 4MHz
    }
}

static void uart_tx_init_9600_smclk_1mhz(void)
{
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

static inline void uart_tx_byte(uint8_t b)
{
    while (!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = b;
}
static void uart_tx_str(const char* s)
{
    while (*s) uart_tx_byte((uint8_t)*s++);
}

void gpio_init()
{
    // Válasszuk a GPIO funkciót ezekre a pinekre
    P1SEL0 &= ~(BUZZ1 | BUZZ2);
    P1SEL1 &= ~(BUZZ1 | BUZZ2);

    // Irány és kezdeti szint
    P1DIR  |=  (BUZZ1 | BUZZ2);
    P1OUT  &= ~(BUZZ1 | BUZZ2);

    P2SEL0 &= ~(BIT0 | BIT1);
    P2SEL1 &= ~(BIT0 | BIT1);
    P2REN  &= ~(BIT0 | BIT1);
    P2DIR  |=  (BIT0 | BIT1);
    P2OUT = (P2OUT & ~(BIT0 | BIT1)) | BIT0;
}

void timer_init()
{
    TA0CTL   = TASSEL__ACLK | ID__1 | MC__UP | TACLR;
    TA0CCR0  = 10000;          // Kezdd ezzel; ha túl gyors/lassú, állítsd 5000-re.
    TA0CCTL0 = CCIE;               // engedélyezzük a CCR0 megszakítást
    __bis_SR_register(GIE);        // globál interrupt engedély
}

static inline uint16_t get_buttons_mask(void)
{
    // ROM-os hívás esetén:
    return MAP_CAPT_getSensorState(&sensor_buttons);
    return 0;
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    gpio_init();
    delay_ms(500);
    beep_2k_short();

    timer_init();
    uart_tx_init_9600_smclk_1mhz();

    // CapTIvate indulás
//    MAP_CAPT_appStart();
//    MAP_CAPT_calibrateUI(&g_uiApp);

    uint16_t prev = 0;

    for (;;)
    {
//        MAP_CAPT_updateUI(&g_uiApp);
        uint16_t mask = get_buttons_mask();
        uint16_t rising = (~prev) & mask;

        // egyszerű CSV sor: "t,btn0,btn1\r\n"
        uart_tx_str("t,");
        uart_tx_byte((mask & BIT0) ? '1' : '0'); uart_tx_byte(',');
        uart_tx_byte((mask & BIT1) ? '1' : '0'); uart_tx_str("\r\n");

        if (rising & (BIT0 | BIT1)) {
            beep_2k_short();
        }

        prev = mask;
        __delay_cycles(1000);
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
