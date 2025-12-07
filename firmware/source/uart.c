#include <msp430.h>
#include <stddef.h>
#include "uart.h"

#define UART_TX_BUFFER_SIZE 128

// Ring buffer for TX
static volatile char tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;

static const char prefix_ok[] =    "\033[1;32m[   OK   ]\033[0m ";
static const char prefix_error[] = "\033[1;31m[ ERROR! ]\033[0m ";

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
    
    // 115200 baud configuration in high-frequency mode (oversampling)
    // With SMCLK (8 MHz): N = 8000000 / (16 * 115200) = 4.34
    // High-frequency mode (oversampling): UCBR = 4, UCBRF = 5
    // UCOS16 enables 16x oversampling for better accuracy
    UCA0BR0 = 4;                       // Base divisor = 4
    UCA0BR1 = 0;                       // High byte of baud rate divisor  
    UCA0MCTLW = UCOS16 | UCBRF_5;     // Oversampling enabled, fractional part = 5
    
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
        // Enable TX interrupt - the interrupt handler will send the first character
        // This avoids race conditions by letting the interrupt handler manage all TX
        // If UCTXIFG is already set, the interrupt will fire immediately
        UCA0IE |= UCTXIE;
    }
}

// Pause TX interrupt to protect critical section (buffer modification)
// Wait for any pending interrupt to complete before disabling
static void uart_tx_pause(void) {
    // Disable interrupt
    UCA0IE &= ~UCTXIE;
    // Wait a few cycles to ensure any in-flight interrupt completes
    // This prevents race conditions where we modify the buffer while interrupt is reading it
    __delay_cycles(10);
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

void uart_send_status_prefix(bool ok) {
    uart_send_string(ok ? prefix_ok : prefix_error);
}

void uart_send_uint_dec(uint32_t value)
{
    char buf[11];
    int idx = (int)sizeof(buf) - 1;
    buf[idx] = '\0';
    if (value == 0u) {
        uart_send_char('0');
        return;
    }
    while (value > 0u && idx > 0) {
        idx--;
        buf[idx] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    uart_send_string(&buf[idx]);
}

void uart_wait_for_tx_empty(void) {
    while (tx_head != tx_tail) {
        __delay_cycles(1);
    }
}

// UART TX interrupt handler
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void) {
    // Check if TX interrupt flag is set
    if (UCA0IFG & UCTXIFG) {
        if (tx_head != tx_tail) {
            // Send next byte from buffer
            // Read tx_tail before modifying to avoid race condition
            uint8_t tail = tx_tail;
            UCA0TXBUF = tx_buffer[tail];
            // Update tail after writing to TXBUF to ensure atomicity
            tx_tail = (tail + 1) % UART_TX_BUFFER_SIZE;
        } else {
            // Buffer empty, disable TX interrupt
            UCA0IE &= ~UCTXIE;
        }
    }
}