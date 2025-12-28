#include "dali_device_generic.h"

uint32_t randomAddress = 0;
volatile uint16_t g_debug_frame = 0;

void process_dali_frame(uint32_t frame, uint8_t frame_length, bool is_valid, bool received_twice) {
    if(!is_valid) {
        uart_send_string("Invalid DALI frame: ");
        uart_send_hex((uint8_t*)&frame, (frame_length+7)/8);
        uart_send_string("\r\n");
        return;
    }
    g_debug_frame = frame;
    dali_tx_send_backward_frame(frame & 0xFF, 40);
    /*uart_send_uint_dec(frame & 0xFF);
    uart_send_string("\r\n");
    uart_send_string("DALI frame: ");
    uart_send_hex((uint8_t*)&frame, (frame_length+7)/8);
    uart_send_string(" - length: ");
    uart_send_uint_dec((uint32_t)frame_length);
    uart_send_string(" - received twice: ");
    uart_send_string(received_twice ? "yes" : "no");
    uart_send_string("\r\n");*/
}