#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void rng_seed(uint32_t);
uint32_t rng_next(void);

#endif