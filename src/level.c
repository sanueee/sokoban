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

void FloodFill(Level *const level, int x, int y, int visited[MAX_FIELD][MAX_FIELD])
{ // mark the field like flood (ret 1 if all cells are available)
    if (x < 0 || x >= level->width || y < 0 || y >= level->height) return;
    if (visited[y][x] || level->cells[y][x] == CELL_WALL) return;

    visited[y][x] = 1;
    FloodFill(level, x + 1, y, visited);
    FloodFill(level, x - 1, y, visited);
    FloodFill(level, x, y + 1, visited);
    FloodFill(level, x, y - 1, visited);
}

int IsConnected(Level *const level)
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

        if (IsConnected(&level))
        {
            valid = 1;
        }
    }

    return level;
}

void RestartLevel(Level *const level)
{ // set session parameters to zero
    level->undo_top = 0;
    level->time_elapsed = 0;
    level->step_count = 0;
}
