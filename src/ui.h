#ifndef UI_H
#define UI_H

#include "types.h"
#include "level.h"
#include "db.h"

void DrawMenu(Screen *screen, int *quit, const char *username);
void DrawDifficultySelect(Screen *screen, Difficulty *diff, Level *level);
void DrawSettings(Screen *screen);
void DrawPause(Screen *screen, Level *level, int user_id, Difficulty diff);
void DrawWin(Screen *screen, Level *level, Difficulty diff);
void DrawRules(Screen *screen);
void DrawStats(Screen *screen, int user_id);
void DrawLogin(Screen *screen, int *user_id, char *username);
void DrawHistory(Screen *screen, int user_id);

#endif
