#ifndef RNG_H
#define RNG_H

#include <stdint.h>

extern uint32_t rng_state;

void rng_seed(uint32_t seed);

static inline uint32_t rng_next(void)
{
    uint32_t x = rng_state;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    x = x ^ (x << 5);
    return rng_state = x;
}

#endif
