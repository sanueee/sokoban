#include "level.h"
#include "types.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};


static void RandomWalk(Level *const level)
{ // сheck the corridors
    int total_square = (level->height - 2) * (level->width - 2);
    int target = total_square * 0.6;
    int free_count = 0;

    int x = level->width / 2;
    int y = level->height / 2;
    level->cells[y][x] = CELL_FLOOR;
    free_count++;

    while (free_count < target)
    {
        int dir = rand() % 4;
        int nx = x + DX[dir];
        int ny = y + DY[dir];

        if (nx > 0 && nx < level->width &&
            ny > 0 && ny < level->height)
        {
            x = nx;
            y = ny;
            if (level->cells[y][x] == CELL_WALL)
            {
                level->cells[y][x] = CELL_FLOOR;
                free_count++;
            }
        }
    }
}

static void FloodFill(Level *const level, int x, int y, int visited[MAX_FIELD][MAX_FIELD])
{ // mark the field like flood (ret 1 if all cells are available)
    if (x < 0 || x >= level->width || y < 0 || y >= level->height) return;
    if (visited[y][x] || level->cells[y][x] == CELL_WALL) return;

    visited[y][x] = 1;
    FloodFill(level, x + 1, y, visited);
    FloodFill(level, x - 1, y, visited);
    FloodFill(level, x, y + 1, visited);
    FloodFill(level, x, y - 1, visited);
}

static int IsConnected(Level *const level)
{ // check for passability 
    int visited[MAX_FIELD][MAX_FIELD] = {0};

    int startX = -1; int startY = -1;
    for (int y = 0; y < level->height && startX == -1; y++) // search for start cell
    {
        for (int x = 0; x < level->width && startX == -1; x++)
        {
            if (level->cells[y][x] == CELL_FLOOR)
            {
                startX = x;
                startY = y;
            }
        }
    }
    if (startX == -1) return 0;

    FloodFill(level, startX, startY, visited);

    for (int y = 0; y < level->height; y++)
    {
        for (int x = 0; x < level->width; x++)
        {
            if (level->cells[y][x] == CELL_FLOOR && !visited[y][x])
            {
                return 0;
            }
        }
    }

    return 1;
}

static int Distance(Position a, Position b)
{ // calculate the distance
    return abs(a.x - b.x) + abs(a.y - b.y);
}

static int PlaceGoalsAndBoxes(Level *const level)
{ // just place goal num of boxes (ret count of placed)
    int placed = 0;
    int attempts = 0;

    while (placed < level->num_boxes && attempts < 1000)
    {
        attempts++;
        int x = rand() % level->width;
        int y = rand() % level->height;

        if (level->cells[y][x] != CELL_FLOOR) continue;

        int too_close = 0;
        for (int i = 0; i < placed; i++)
        {
            Position p = {x, y};
            if (Distance(p, level->goals[i]) < 2)
            {
                too_close = 1;
                break;
            }
        }
        if (too_close) continue;

        level->goals[placed].x = x;
        level->goals[placed].y = y;
        level->boxes[placed].x = x;
        level->boxes[placed].y = y;
        placed++;
    }
    return placed == level->num_boxes;
}

static void ReverseSolve(Level *const level, int target_moves)
{ // place boxes on start positions
    int last_box = -1;
    int same_box_count = 0;

    for (int i = 0; i < target_moves; i++)
    {
        int attempts = 0;
        while (attempts < 20)
        {
            attempts++;
            
            int box_idx = rand() % level->num_boxes;
            if (box_idx == last_box) // чтобы не двигать один бокс вечно
            {
                same_box_count++;
                if (same_box_count > 5)
                {
                    continue;
                }
                else
                {
                    same_box_count = 0;
                }
            }

            int dir = rand() % 4;
            int bx = level->boxes[box_idx].x;
            int by = level->boxes[box_idx].y;

            int new_bx = bx - DX[dir];
            int new_by = by - DY[dir];

            int px = bx + DX[dir];
            int py = by + DY[dir];

            if (new_bx <= 0 || new_bx >= level->width - 1) continue;
            if (new_by <= 0 || new_by >= level->height - 1) continue;
            if (level->cells[new_by][new_bx] != CELL_FLOOR) continue;
            if (level->cells[py][px] != CELL_FLOOR) continue;

            int occupied = 0;
            for (int j = 0; j < level->num_boxes; j++) {
                if (j == box_idx) continue;
                if (level->boxes[j].x == new_bx && level->boxes[j].y == new_by) {
                    occupied = 1;
                    break;
                }
            }
            if (occupied) continue;

            level->boxes[box_idx].x = new_bx;
            level->boxes[box_idx].y = new_by;
            level->player.x = bx;
            level->player.y = by;
            last_box = box_idx;
            break;
        }
    }
}

Level GenerateLevel(Difficulty difficulty)
{ // generate level depends on difficulty
    srand(time(NULL));
    Level level = {0};
    level.difficulty = difficulty;

    int valid = 0;
    while (!valid)
    {
        switch (difficulty)
        {
            case DIFF_EASY:
                level.width = 8 + rand() % 3; // 8-10
                level.height = 8 + rand() % 3;
                level.num_boxes = 3 + rand() % 2; // 3-4
                break;
            case DIFF_MEDIUM:
                level.width = 12 + rand() %3; // 12-14
                level.height = 12 + rand() % 3;
                level.num_boxes = 5 + rand() % 2; // 5-6
                break;
            case DIFF_HARD:
                level.width = 15 + rand() % 4; // 15-18
                level.height = 15 + rand() % 4;
                level.num_boxes = 7 + rand() % 2; // 7-8
                break;
        }

        for (size_t i = 0; i < level.height; i++)
        {
            for (size_t j = 0; j < level.width; j++) 
            {
                level.cells[i][j] = CELL_WALL;
            }
        }

        RandomWalk(&level);

        if (!IsConnected(&level)) continue;

        if (!PlaceGoalsAndBoxes(&level)) continue;

        int target_moves;
        switch (difficulty)
        {
            case DIFF_EASY:   target_moves = 15 + rand() % 16; break; // 15-30
            case DIFF_MEDIUM: target_moves = 40 + rand() % 31; break; // 40-70
            case DIFF_HARD:   target_moves = 80 + rand() % 41; break; // 80-120
        }

        ReverseSolve(&level, target_moves);
        valid = 1;
    }

    return level;
}

void RestartLevel(Level *const level)
{ // set session parameters to zero
    level->undo_top = 0;
    level->time_elapsed = 0;
    level->step_count = 0;
}
