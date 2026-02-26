#include "level.h"
#include "types.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};


static void RandomWalk(Level *const level)
{ // makes the corridors.
    int total_square = (level->height - 2) * (level->width - 2);
    int target = total_square * 0.4;
    int free_count = 0;

    int x = level->width / 2;
    int y = level->height / 2;
    level->cells[y][x] = CELL_FLOOR;
    free_count++;

    int prev_dir = -1;
    while (free_count < target)
    {
        int dir = rand() % 4;
        // с вероятностью 60% продолжаем в том же направлении — создаёт коридоры
        if (prev_dir >= 0 && rand() % 100 < 60) {
            dir = prev_dir;
        } else {
            dir = rand() % 4;
        }
        int nx = x + DX[dir];
        int ny = y + DY[dir];

        if (nx > 1 && nx < level->width - 2 &&
            ny > 1 && ny < level->height - 2)
        {
            x = nx;
            y = ny;
            if (level->cells[y][x] == CELL_WALL)
            {
                level->cells[y][x] = CELL_FLOOR;
                free_count++;
            }
        }
        else
        {
            prev_dir = -1;
        }
    }
}

static void FloodFill(Level *const level, int startX, int startY, int visited[MAX_FIELD][MAX_FIELD])
{ // marks the field like flood (ret 1 if all cells are available).
    typedef struct { int x, y; } Point;
    Point stack[MAX_FIELD * MAX_FIELD];
    int top = 0;

    stack[top++] = (Point){startX, startY};
    visited[startY][startX] = 1;

    while (top > 0)
    {
        Point p = stack[--top];
        for (int d = 0; d < 4; d++)
        {
            int nx = p.x + DX[d];
            int ny = p.y + DY[d];
            if (nx < 0 || nx >= level->width || ny < 0 || ny >= level->height) continue;
            if (visited[ny][nx] || level->cells[ny][nx] == CELL_WALL) continue;
            visited[ny][nx] = 1;
            stack[top++] = (Point){nx, ny};
        }
    }
}

static void FloodFillWithBoxes(Level *level, int startX, int startY, int visited[MAX_FIELD][MAX_FIELD])
{
    typedef struct { int x, y; } Point;
    Point stack[MAX_FIELD * MAX_FIELD];
    int top = 0;

    stack[top++] = (Point){startX, startY};
    visited[startY][startX] = 1;

    while (top > 0)
    {
        Point p = stack[--top];
        for (int d = 0; d < 4; d++)
        {
            int nx = p.x + DX[d];
            int ny = p.y + DY[d];
            if (nx < 0 || nx >= level->width || ny < 0 || ny >= level->height) continue;
            if (visited[ny][nx] || level->cells[ny][nx] == CELL_WALL) continue;

            // проверка на ящик
            int is_box = 0;
            for (int i = 0; i < level->num_boxes; i++)
                if (level->boxes[i].x == nx && level->boxes[i].y == ny) { is_box = 1; break; }
            if (is_box) continue;

            visited[ny][nx] = 1;
            stack[top++] = (Point){nx, ny};
        }
    }
}

static int IsConnected(Level *const level)
{ // checks for passability.
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
{ // calculates the distance.
    return abs(a.x - b.x) + abs(a.y - b.y);
}

// places boxes on start positions.
static void ReverseSolve(Level *level, int target_moves) {
    int last_box = -1;
    int same_box_count = 0;

    for (int i = 0; i < target_moves; i++) {
        // Считаем flood fill один раз на ход, а не на каждую попытку
        int visited[MAX_FIELD][MAX_FIELD] = {0};
        FloodFillWithBoxes(level, level->player.x, level->player.y, visited);

        int moved = 0;
        for (int attempts = 0; attempts < 100 && !moved; attempts++) {
            int box_idx = rand() % level->num_boxes;

            if (box_idx == last_box && ++same_box_count > 5) continue;
            else if (box_idx != last_box) same_box_count = 0;

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
            if (px <= 0 || px >= level->width - 1) continue;
            if (py <= 0 || py >= level->height - 1) continue;
            if (level->cells[py][px] != CELL_FLOOR) continue;

            int occupied = 0;
            for (int j = 0; j < level->num_boxes; j++) {
                if (j == box_idx) continue;
                if (level->boxes[j].x == new_bx && level->boxes[j].y == new_by) {
                    occupied = 1; break;
                }
            }
            if (occupied) continue;

            // используем уже посчитанный visited
            if (!visited[py][px]) continue;

            level->boxes[box_idx].x = new_bx;
            level->boxes[box_idx].y = new_by;
            level->player.x = bx;
            level->player.y = by;
            last_box = box_idx;
            moved = 1;
        }
    }
}

static int IsCornerDeadlock(Level *const level, Position box)
{ // сhecks whether a box is trapped in a corner by two walls along adjacent axes. such a box cannot be moved if it is not on target.
    int wall_up    = (level->cells[box.y - 1][box.x] == CELL_WALL);
    int wall_down  = (level->cells[box.y + 1][box.x] == CELL_WALL);
    int wall_left  = (level->cells[box.y][box.x - 1] == CELL_WALL);
    int wall_right = (level->cells[box.y][box.x + 1] == CELL_WALL);

    return (wall_up && wall_left)  ||
           (wall_up && wall_right) ||
           (wall_down && wall_left)||
           (wall_down && wall_right);
}

static int PlaceGoalsAndBoxes(Level *const level)
{ // just place goal num of boxes (ret count of placed).
    int placed = 0;
    int attempts = 0;
    int min_goal_dist = (level->num_boxes <= 4) ? 3 : 2;

    while (placed < level->num_boxes && attempts < 1000)
    {
        attempts++;
        int x = rand() % level->width;
        int y = rand() % level->height;

        if (level->cells[y][x] != CELL_FLOOR) continue;

        // не ставить в углах — сразу дедлок
        int walls = 0;
        for (int d = 0; d < 4; d++)
            if (level->cells[y + DY[d]][x + DX[d]] == CELL_WALL) walls++;
        if (walls >= 2) {
            // проверка на угловой дедлок
            Position p = {x, y};
            if (IsCornerDeadlock(level, p)) continue;
        }
        
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

static int IsOnGoal(Level *level, Position box)
{ // returns 1 if the box is located at one of the target positions.
    for (int i = 0; i < level->num_boxes; i++)
        if (level->goals[i].x == box.x && level->goals[i].y == box.y)
            return 1;
    return 0;
}

static int HasDeadlock(Level *level)
{ // checks all the boxes on the corner deadlock. the boxes on the targets are skipped — they are already in place.
  // returns 1 if at least one box is deadlocked.
    for (int i = 0; i < level->num_boxes; i++)
    {
        if (IsOnGoal(level, level->boxes[i])) continue;
        if (IsCornerDeadlock(level, level->boxes[i])) return 1;
    }
    return 0;
}

static int AllBoxesReachable(Level *level)
{ // checking possibility to reach box from at least 1 side.
    int visited[MAX_FIELD][MAX_FIELD] = {0};
    FloodFillWithBoxes(level, level->player.x, level->player.y, visited);

    for (int i = 0; i < level->num_boxes; i++)
    {
        int bx = level->boxes[i].x;
        int by = level->boxes[i].y;

        int reachable = 0;
        for (int d = 0; d < 4; d++)
        {
            int px = bx + DX[d];
            int py = by + DY[d];
            if (px >= 0 && px < level->width &&
                py >= 0 && py < level->height &&
                visited[py][px])
            {
                reachable = 1;
                break;
            }
        }
        if (!reachable) return 0;
    }
    return 1;
}

Level GenerateLevel(Difficulty difficulty)
{ // generate level depends on difficulty.
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
            case DIFF_EASY:   target_moves = 20 + rand() % 16; break; // 20-35
            case DIFF_MEDIUM: target_moves = 40 + rand() % 31; break; // 40-70
            case DIFF_HARD:   target_moves = 80 + rand() % 41; break; // 80-120
        }

        int player_placed = 0;
        for (int a = 0; a < 500 && !player_placed; a++) {
            int px = 1 + rand() % (level.width - 2);
            int py = 1 + rand() % (level.height - 2);
            if (level.cells[py][px] != CELL_FLOOR) continue;

            int on_box = 0;
            for (int i = 0; i < level.num_boxes; i++)
                if (level.boxes[i].x == px && level.boxes[i].y == py) { on_box = 1; break; }
            if (on_box) continue;

            level.player.x = px;
            level.player.y = py;
            player_placed = 1;
        }
        if (!player_placed) continue;

        ReverseSolve(&level, target_moves);

        int min_dist = 2;
        int too_close = 0;
        for (int i = 0; i < level.num_boxes; i++) {
            if (Distance(level.boxes[i], level.goals[i]) < min_dist) {
                too_close = 1;
                break;
            }
        }
        if (too_close) continue;

        if (HasDeadlock(&level)) continue;
        if (!AllBoxesReachable(&level)) continue;

        level.initial_state.player = level.player;
        memcpy(level.initial_state.boxes, level.boxes, sizeof(level.boxes));
        level.initial_state.step_count = 0;
        
        valid = 1;
    }

    return level;
}

void RestartLevel(Level *const level)
{ // set session parameters to zero.
    level->player = level->initial_state.player;
    memcpy(level->boxes, level->initial_state.boxes, sizeof(level->boxes));
    level->step_count = 0;
    level->time_elapsed = 0;
    level->undo_top = 0;
}
