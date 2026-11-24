#include "rng.h"

uint32_t rng_state = 1;

void rng_seed(uint32_t seed)
{
    rng_state = seed ? seed : 1;
}
