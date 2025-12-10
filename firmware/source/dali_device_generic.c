#include "dali_device_generic.h"

uint32_t randomAddress = 0;

void process_dali_frame(uint32_t frame, uint8_t frame_length, bool is_valid, bool received_twice) {
    if(!is_valid) {
        uart_send_string("Invalid DALI frame: ");
        uart_send_hex((uint8_t*)&frame, (frame_length+7)/8);
        uart_send_string("\r\n");
        return;
    }
    uart_send_string("DALI frame: ");
    uart_send_hex((uint8_t*)&frame, (frame_length+7)/8);
    uart_send_string("\r\n");
}