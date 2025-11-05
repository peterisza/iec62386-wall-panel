#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

// Convert float to string (simple implementation for embedded systems)
// Returns pointer to static buffer (not thread-safe)
// Format: "XX.XX" (2 decimal places)
char* ftoa(float value);

// Convert int32_t to string with padding
// Returns pointer to static buffer (not thread-safe)
// Pads with leading spaces to reach 'width' characters
char* itoa_padded(int32_t value, uint8_t width);

#endif // UTIL_H

