#ifndef GAME_H
#define GAME_H

#include "types.h"

void HandleInput(Level *level);
int  CheckWin(const Level *level);
void PushUndo(Level *level);
void PopUndo(Level *level);

#endif