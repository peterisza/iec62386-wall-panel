#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

// Initialize random number generator with seed
void random_init(uint16_t seed);

// Generate next random number (16-bit)
uint16_t random_next(void);

// Generate random number in range [0, max)
uint16_t random_range(uint16_t max);

// Simple macro for quick random (uses function, but simpler syntax)
// Usage: uint16_t r = RANDOM();
#define RANDOM() random_next()

// Macro for range [0, max)
// Usage: uint16_t r = RANDOM_MAX(100);
#define RANDOM_MAX(max) random_range(max)

#endif // RANDOM_H

