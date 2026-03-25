#include "game.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include "level.h"

static const int MDX[4] = {0, 0, -1, 1};
static const int MDY[4] = {-1, 1, 0, 0};

static int BoxAt(const Level *level, int x, int y)
{
    for (int i = 0; i < level->num_boxes; i++)
        if (level->boxes[i].x == x && level->boxes[i].y == y)
            return i;
    return -1;
}

void PushUndoMove(Level *level)
{
    UndoNode *node = (UndoNode *)malloc(sizeof(UndoNode));
    if (!node) return;

    node->type = UNDO_MOVE;
    node->data.move.player_pos = level->player;
    node->step_count = level->step_count;

    node->next = level->undo_head;
    level->undo_head = node;
    level->undo_count++;
}

void PushUndoPush(Level *level, int box_index)
{
    UndoNode *node = (UndoNode *)malloc(sizeof(UndoNode));
    if (!node) return;

    node->type = UNDO_PUSH;
    node->data.push.player_pos = level->player;
    node->data.push.box_index = box_index;
    node->data.push.box_pos = level->boxes[box_index];
    node->step_count = level->step_count;

    node->next = level->undo_head;
    level->undo_head = node;
    level->undo_count++;
}

void PopUndo(Level *level)
{
    if (!level->undo_head) return;

    UndoNode *top = level->undo_head;

    if (top->type == UNDO_PUSH)
    {
        level->boxes[top->data.push.box_index] = top->data.push.box_pos;
        level->player = top->data.push.player_pos;
    }
    else
    {
        level->player = top->data.move.player_pos;
    }

    level->step_count = top->step_count;
    level->undo_head = top->next;
    level->undo_count--;
    free(top);
}

void FreeUndoStack(Level *level)
{
    while (level->undo_head)
    {
        UndoNode *tmp = level->undo_head;
        level->undo_head = tmp->next;
        free(tmp);
    }
    level->undo_count = 0;
}

int CheckWin(const Level *level)
{
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
{
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
    {
        int bnx = nx + dx;
        int bny = ny + dy;

        if (level->cells[bny][bnx] == CELL_WALL) return;
        if (BoxAt(level, bnx, bny) != -1) return;

        PushUndoPush(level, box_idx);
        level->boxes[box_idx].x = bnx;
        level->boxes[box_idx].y = bny;
    }
    else
        PushUndoMove(level);

    level->player.x = nx;
    level->player.y = ny;
    level->step_count++;
}

void ApplyMove(Level *level, int dir)
{
    int dx = MDX[dir], dy = MDY[dir];
    int nx = level->player.x + dx;
    int ny = level->player.y + dy;

    if (level->cells[ny][nx] == CELL_WALL) return;

    int box_idx = BoxAt(level, nx, ny);

    if (box_idx != -1)
    {
        int bnx = nx + dx;
        int bny = ny + dy;

        if (level->cells[bny][bnx] == CELL_WALL) return;
        if (BoxAt(level, bnx, bny) != -1) return;

        PushUndoPush(level, box_idx);
        level->boxes[box_idx].x = bnx;
        level->boxes[box_idx].y = bny;
    }
    else
        PushUndoMove(level);

    level->player.x = nx;
    level->player.y = ny;
    level->step_count++;
}