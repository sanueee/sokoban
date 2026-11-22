#include "rng.h"

uint32_t rng_state = 1;

void rng_seed(uint32_t seed)
{
    rng_state = seed ? seed : 1;
}

uint32_t rng_next(void)
{
    uint32_t x = rng_state;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    return rng_state = x;
}
