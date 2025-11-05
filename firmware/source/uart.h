#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_send_string(const char* str);
void uart_send_char(char c);
void uart_send_hex_byte(uint8_t byte);
void uart_send_hex(const uint8_t* data, uint16_t len);

#endif // UART_H

