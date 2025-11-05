#include <msp430.h>
#include <stddef.h>
#include "uart.h"

#define UART_TX_BUFFER_SIZE 128

// Ring buffer for TX
static volatile char tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;

// Calculate number of bytes in buffer
static inline uint8_t tx_bytes_in_buffer(void) {
    if (tx_head >= tx_tail) {
        return tx_head - tx_tail;
    } else {
        return (UART_TX_BUFFER_SIZE - tx_tail) + tx_head;
    }
}

// Calculate free space in buffer
static inline uint8_t tx_free_space(void) {
    return UART_TX_BUFFER_SIZE - tx_bytes_in_buffer() - 1; // -1 to distinguish full from empty
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
    
    // 115200 baud configuration in low-frequency mode
    // With SMCLK (~1.05MHz): N = 1050000 / 115200 = 9.11
    // Low-frequency mode (no oversampling): UCBR = 9
    // Fractional part 0.11 is small, so no modulation needed (error ~1.2%)
    UCA0BR0 = 9;                       // Base divisor = 9
    UCA0BR1 = 0;                       // High byte of baud rate divisor  
    UCA0MCTLW = 0;                     // No oversampling, no modulation
    
    // Initialize ring buffer
    tx_head = 0;
    tx_tail = 0;
    
    // Clear any pending TX interrupt flags
    UCA0IFG &= ~UCTXIFG;
    
    // Don't enable TX interrupt yet - wait until first character is added
    // This prevents the interrupt from firing when buffer is empty
    
    UCA0CTLW0 &= ~UCSWRST;             // Release UART from reset
}

// Helper function to start transmission if buffer has data and UART is ready
static void uart_tx_start(void) {
    // If buffer is not empty
    if (tx_head != tx_tail) {
        // Enable TX interrupt if not already enabled
        if (!(UCA0IE & UCTXIE)) {
            UCA0IE |= UCTXIE;
        }
        
        // If TX buffer is ready and we haven't started sending yet, trigger immediately
        // This handles the first character case
        if ((UCA0IFG & UCTXIFG) && (tx_head != tx_tail)) {
            UCA0TXBUF = tx_buffer[tx_tail];
            tx_tail = (tx_tail + 1) % UART_TX_BUFFER_SIZE;
        }
    }
}

// Pause TX interrupt to protect critical section (buffer modification)
static void uart_tx_pause(void) {
    UCA0IE &= ~UCTXIE;
}

void uart_send_char(char c) {
    // Wait until there's space in buffer
    while (tx_free_space() == 0) {
        // Buffer full, wait for space
    }
    
    // Pause TX interrupt to protect critical section
    uart_tx_pause();
    
    // Check again with interrupt disabled (in case it was updated in interrupt)
    if (tx_free_space() > 0) {
        tx_buffer[tx_head] = c;
        tx_head = (tx_head + 1) % UART_TX_BUFFER_SIZE;
    }
    
    // Resume transmission (will re-enable interrupt if needed)
    uart_tx_start();
}

void uart_send_string(const char* str) {
    if (str == NULL) {
        return;
    }
    
    while (*str) {
        uart_send_char(*str++);
    }
}

// Send a single byte as hex (e.g., "FF")
void uart_send_hex_byte(uint8_t byte) {
    static const char hex_chars[] = "0123456789ABCDEF";
    uart_send_char(hex_chars[(byte >> 4) & 0x0F]);
    uart_send_char(hex_chars[byte & 0x0F]);
}

// Send a buffer as hex bytes (e.g., "FF AA 3B")
void uart_send_hex(const uint8_t* data, uint16_t len) {
    if (data == NULL || len == 0) {
        return;
    }
    
    for (uint16_t i = 0; i < len; i++) {
        uart_send_hex_byte(data[i]);
        if (i < len - 1) {
            uart_send_char(' ');
        }
    }
}

// UART TX interrupt handler
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void) {
    // Check if TX interrupt flag is set
    if (UCA0IFG & UCTXIFG) {
        if (tx_head != tx_tail) {
            // Send next byte from buffer
            UCA0TXBUF = tx_buffer[tx_tail];
            tx_tail = (tx_tail + 1) % UART_TX_BUFFER_SIZE;
        } else {
            // Buffer empty, disable TX interrupt
            UCA0IE &= ~UCTXIE;
        }
    }
}