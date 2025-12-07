#include "random.h"

// LCG (Linear Congruential Generator) parameters
// These are good values for 16-bit: a=11035, c=12345, m=2^16 (implicit)
#define RANDOM_A 11035
#define RANDOM_C 12345

static uint16_t g_random_seed = 1;

void random_init(uint16_t seed)
{
    g_random_seed = seed;
    if (g_random_seed == 0) {
        g_random_seed = 1;  // Seed cannot be 0
    }
}

uint16_t random_next(void)
{
    g_random_seed = g_random_seed * RANDOM_A + RANDOM_C;
    return g_random_seed;
}

uint16_t random_range(uint16_t max)
{
    if (max == 0) {
        return 0;
    }
    return random_next() % max;
}

