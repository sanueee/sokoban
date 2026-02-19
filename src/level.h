#ifndef LEVEL_H
#define LEVEL_H

#include "types.h"

Level GenerateLevel(Difficulty difficulty);
void RestartLevel(Level *level);

#endif