#ifndef GAME_H
#define GAME_H

#include "types.h"

void HandleInput(Level *level);
void ApplyMove(Level *level, int dir);
int CheckWin(const Level *level);
void PushUndoMove(Level *level);
void PushUndoPush(Level *level, int box_index);
void PopUndo(Level *level);
void FreeUndoStack(Level *level);

#endif