#include "game.h"
#include "raylib.h"
#include <string.h>
#include "level.h"

static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};

static int BoxAt(const Level *level, int x, int y)
{ // gets box index on pos x, y
    for (int i = 0; i < level->num_boxes; i++)
        if (level->boxes[i].x == x && level->boxes[i].y == y)
            return i;
    return -1;
}

void PushUndo(Level *level)
{ // adds state in stack
    if (level->undo_top >= MAX_UNDO) return;
    level->undo_stack[level->undo_top].player = level->player;
    memcpy(level->undo_stack[level->undo_top].boxes,
           level->boxes, sizeof(level->boxes));
    level->undo_stack[level->undo_top].step_count = level->step_count;
    level->undo_top++;
}

void PopUndo(Level *level)
{ // undo
    if (level->undo_top <= 0) return;
    level->undo_top--;
    level->player = level->undo_stack[level->undo_top].player;
    memcpy(level->boxes,
           level->undo_stack[level->undo_top].boxes, sizeof(level->boxes));
    level->step_count = level->undo_stack[level->undo_top].step_count;
}

int CheckWin(const Level *level)
{ // checks if all boxes on goals
    for (int i = 0; i < level->num_boxes; i++)
    {
        int on_goal = 0;
        for (int j = 0; j < level->num_boxes; j++)
        {
            if (level->boxes[i].x == level->goals[j].x &&
                level->boxes[i].y == level->goals[j].y)
            {
                on_goal = 1;
                break;
            }
        }
        if (!on_goal) return 0;
    }
    return 1;
}

void HandleInput(Level *level)
{ // controlling the player
    int dx = 0, dy = 0;

    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) { dx = 0;  dy = -1; }
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) { dx = 0;  dy =  1; }
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) { dx = -1; dy =  0; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { dx = 1;  dy =  0; }

    if (IsKeyPressed(KEY_Z))
    {
        PopUndo(level);
        return;
    }

    if (IsKeyPressed(KEY_R))
    {
        RestartLevel(level);
        return;
    }

    if (dx == 0 && dy == 0) return;

    int nx = level->player.x + dx;
    int ny = level->player.y + dy;

    if (level->cells[ny][nx] == CELL_WALL) return;

    int box_idx = BoxAt(level, nx, ny);

    if (box_idx != -1)
    { // met the box
        int bnx = nx + dx;
        int bny = ny + dy;

        if (level->cells[bny][bnx] == CELL_WALL) return;
        if (BoxAt(level, bnx, bny) != -1) return;

        PushUndo(level);
        level->boxes[box_idx].x = bnx;
        level->boxes[box_idx].y = bny;
    } 
    else
        PushUndo(level);

    level->player.x = nx;
    level->player.y = ny;
    level->step_count++;
}