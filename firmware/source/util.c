#include "util.h"
#include <stdint.h>
#include <stdbool.h>

// Simple ftoa implementation for embedded systems
// Converts float to string with fixed 2 decimal places
char* ftoa(float value) {
    static char buffer[16];  // Static buffer for result
    char *ptr = buffer;
    
    // Handle negative numbers
    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }
    
    // Convert integer part
    int32_t int_part = (int32_t)value;
    float frac_part = value - (float)int_part;
    
    // Handle integer part
    if (int_part == 0) {
        *ptr++ = '0';
    } else {
        // Convert integer to string (reversed)
        char int_buf[12];
        char *int_ptr = int_buf;
        int32_t n = int_part;
        do {
            *int_ptr++ = '0' + (n % 10);
            n /= 10;
        } while (n > 0);
        
        // Reverse and copy to output
        int_ptr--;
        while (int_ptr >= int_buf) {
            *ptr++ = *int_ptr--;
        }
    }
    
    // Decimal point
    *ptr++ = '.';
    
    // Convert fractional part (2 decimal places)
    frac_part *= 100.0f;
    int32_t frac_int = (int32_t)(frac_part + 0.5f);  // Round to nearest
    
    // Ensure we don't go over 99
    if (frac_int >= 100) {
        frac_int = 99;
    }
    
    // Two digits
    *ptr++ = '0' + (frac_int / 10);
    *ptr++ = '0' + (frac_int % 10);
    
    // Null terminator
    *ptr = '\0';
    
    return buffer;
}

// Simple itoa_padded implementation for embedded systems
// Converts int32_t to string with leading space padding
// Negative sign comes after padding: "  -123" instead of "-  123"
char* itoa_padded(int32_t value, uint8_t width) {
    static char buffer[16];  // Static buffer for result
    char *ptr = buffer;
    
    bool is_negative = (value < 0);
    if (is_negative) {
        value = -value;
        if (width > 0) width--;  // Account for minus sign
    }
    
    // Convert to string (reversed)
    char int_buf[12];
    char *int_ptr = int_buf;
    uint32_t n = (uint32_t)value;
    int digits = 0;
    
    if (n == 0) {
        *int_ptr++ = '0';
        digits = 1;
    } else {
        do {
            *int_ptr++ = '0' + (n % 10);
            n /= 10;
            digits++;
        } while (n > 0);
    }
    
    // Add leading spaces if needed (before minus sign)
    while (digits < width) {
        *ptr++ = ' ';
        width--;
    }
    
    // Add minus sign after padding if negative
    if (is_negative) {
        *ptr++ = '-';
    }
    
    // Reverse and copy to output
    int_ptr--;
    while (int_ptr >= int_buf) {
        *ptr++ = *int_ptr--;
    }
    
    // Null terminator
    *ptr = '\0';
    
    return buffer;
}

