#include "level.h"
#include "types.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

static const int DX[4] = {0, 0, -1, 1};
static const int DY[4] = {-1, 1, 0, 0};

static int HasBox(Level *level, int x, int y)
{ // check is box
    for (int i = 0; i < level->num_boxes; i++)
        if (level->boxes[i].x == x && level->boxes[i].y == y) return 1;
    return 0;
}

static void ShuffleDirs(int *dirs)
{ // shuffle directions
    for (int i = 0; i < 4; i++)
    {
        int r = rand() % 4;
        int temp = dirs[i];
        dirs[i] = dirs[r];
        dirs[r] = temp;
    }
}

static void CarveMaze(Level *level, int x, int y)
{ // Recursive Backtracker for corridors 1x1
    level->cells[y][x] = CELL_FLOOR;

    int dirs[] = {0, 1, 2, 3};
    ShuffleDirs(dirs);

    for (int i = 0; i < 4; i++)
    {
        int dx = DX[dirs[i]];
        int dy = DY[dirs[i]];

        // cмотрим на 2 клетки вперед, чтобы оставлять стены между коридорами
        int nx = x + dx * 2;
        int ny = y + dy * 2;

        // если клетка в пределах карты и всё ещё стена — копаем к ней
        if (nx > 0 && nx < level->width - 1 && ny > 0 && ny < level->height - 1)
        {
            if (level->cells[ny][nx] == CELL_WALL)
            {
                // ломаем стену между нами и целью
                level->cells[y + dy][x + dx] = CELL_FLOOR;
                // рекурсивно идем дальше
                CarveMaze(level, nx, ny);
            }
        }
    }
}

static void GenerateMaze(Level *const level)
{ // generating map
    for (int y = 0; y < level->height; y++)
    {
        for (int x = 0; x < level->width; x++)
        {
            level->cells[y][x] = CELL_WALL;
        }
    }

    // вырезаем лабиринт. обязательно начинаем с нечетных координат!
    // иначе коридоры могут "прилипнуть" к краю карты.
    int startX = 1 + (rand() % ((level->width - 2) / 2)) * 2;
    int startY = 1 + (rand() % ((level->height - 2) / 2)) * 2;
    CarveMaze(level, startX, startY);

    // создаем "комнаты" и циклы.
    // ломаем немного случайных стен, чтобы появились открытые пространства и обходные пути.
    // количество сломанных стен зависит от размера карты (около 10% площади).
    int extra_spaces = (level->width * level->height) / 10; 
    
    for (int i = 0; i < extra_spaces; i++) {
        int rx = 1 + rand() % (level->width - 2);
        int ry = 1 + rand() % (level->height - 2);
        
        if (level->cells[ry][rx] == CELL_WALL) {
            level->cells[ry][rx] = CELL_FLOOR;
        }
    }
}

static void FloodFillWithBoxes(Level *level, int startX, int startY, int visited[MAX_FIELD][MAX_FIELD])
{ // checks for passability
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
            if (visited[ny][nx] || level->cells[ny][nx] == CELL_WALL || HasBox(level, nx, ny)) continue;

            visited[ny][nx] = 1;
            stack[top++] = (Point){nx, ny};
        }
    }
}

static int IsCornerDeadlock(Level *const level, int x, int y)
{ // is corner deadlock
    int wall_up    = (level->cells[y - 1][x] == CELL_WALL);
    int wall_down  = (level->cells[y + 1][x] == CELL_WALL);
    int wall_left  = (level->cells[y][x - 1] == CELL_WALL);
    int wall_right = (level->cells[y][x + 1] == CELL_WALL);

    return (wall_up && wall_left)  || (wall_up && wall_right) ||
           (wall_down && wall_left)|| (wall_down && wall_right);
}

static int IsDeadlock(Level *const level, Position box)
{ // is deadlock
    if (IsCornerDeadlock(level, box.x, box.y)) return 1;

    // Проверка 2x2 блоков (стены + ящики), которые невозможно сдвинуть
    int x = box.x, y = box.y;
    int up = HasBox(level, x, y-1) || level->cells[y-1][x] == CELL_WALL;
    int down = HasBox(level, x, y+1) || level->cells[y+1][x] == CELL_WALL;
    int left = HasBox(level, x-1, y) || level->cells[y][x-1] == CELL_WALL;
    int right = HasBox(level, x+1, y) || level->cells[y][x+1] == CELL_WALL;

    int ul = HasBox(level, x-1, y-1) || level->cells[y-1][x-1] == CELL_WALL;
    int ur = HasBox(level, x+1, y-1) || level->cells[y-1][x+1] == CELL_WALL;
    int dl = HasBox(level, x-1, y+1) || level->cells[y+1][x-1] == CELL_WALL;
    int dr = HasBox(level, x+1, y+1) || level->cells[y+1][x+1] == CELL_WALL;

    if (up && left && ul) return 1;
    if (up && right && ur) return 1;
    if (down && left && dl) return 1;
    if (down && right && dr) return 1;

    return 0;
}

static int PlaceGoalsAndBoxes(Level *const level)
{ // places goals and boxes
    int placed = 0;
    int attempts = 0;
    while (placed < level->num_boxes && attempts < 1000)
    {
        attempts++;
        int x = 2 + rand() % (level->width - 4);
        int y = 2 + rand() % (level->height - 4);

        if (level->cells[y][x] != CELL_FLOOR) continue;
        
        if (IsCornerDeadlock(level, x, y)) continue;

        int too_close = 0;
        for (int i = 0; i < placed; i++)
        {
            if (abs(x - level->goals[i].x) + abs(y - level->goals[i].y) < 2)
            {
                too_close = 1; break;
            }
        }
        if (too_close) continue;

        level->goals[placed] = (Position){x, y};
        level->boxes[placed] = (Position){x, y};
        placed++;
    }
    return placed == level->num_boxes;
}

static void ReverseSolve(Level *level, int target_moves)
{ // algorithm for checking solving
    for (int i = 0; i < target_moves; i++)
    {
        int visited[MAX_FIELD][MAX_FIELD] = {0};
        FloodFillWithBoxes(level, level->player.x, level->player.y, visited);

        int moved = 0;
        for (int attempts = 0; attempts < 100 && !moved; attempts++)
        {
            int box_idx = rand() % level->num_boxes;
            int dir = rand() % 4;
            
            int bx = level->boxes[box_idx].x;
            int by = level->boxes[box_idx].y;

            // направление, куда мы тянем ящик. 
            // значит, игрок должен стоять со стороны box - dir (pull_x, pull_y)
            // и отступать на шаг назад в box - 2*dir (back_x, back_y)
            int dx = DX[dir], dy = DY[dir];
            int pull_x = bx - dx; int pull_y = by - dy;
            int back_x = bx - 2 * dx; int back_y = by - 2 * dy;

            // может ли игрок подойти к ящику для тяги
            if (pull_x <= 0 || pull_x >= level->width - 1 || pull_y <= 0 || pull_y >= level->height - 1) continue;
            if (!visited[pull_y][pull_x]) continue;

            // есть ли место куда отступить
            if (back_x <= 0 || back_x >= level->width - 1 || back_y <= 0 || back_y >= level->height - 1) continue;
            if (level->cells[back_y][back_x] == CELL_WALL) continue;
            if (HasBox(level, back_x, back_y)) continue;

            level->boxes[box_idx].x = pull_x;
            level->boxes[box_idx].y = pull_y;
            level->player.x = back_x;
            level->player.y = back_y;
            moved = 1;
        }
    }
}

static int IsOnGoal(Level *level, Position box)
{ // is on goal
    for (int i = 0; i < level->num_boxes; i++)
        if (level->goals[i].x == box.x && level->goals[i].y == box.y) return 1;
    return 0;
}

static int HasDeadlock(Level *level)
{ // checks for deadlocs (returns 1 if is)
    for (int i = 0; i < level->num_boxes; i++)
    {
        if (IsOnGoal(level, level->boxes[i])) continue;
        if (IsDeadlock(level, level->boxes[i])) return 1;
    }
    return 0;
}

Level GenerateLevel(Difficulty difficulty)
{ // main function
    srand(time(NULL));
    Level level = {0};
    level.difficulty = difficulty;

    int valid = 0;
    while (!valid)
    {
        switch (difficulty) {
            case DIFF_EASY:
                level.width = 9 + rand() % 4;
                level.height = 9 + rand() % 4;
                level.num_boxes = 3 + rand() % 2;
                break;
            case DIFF_MEDIUM:
                level.width = 13 + rand() % 4;
                level.height = 13 + rand() % 4;
                level.num_boxes = 5 + rand() % 2;
                break;
            case DIFF_HARD:
                level.width = 17 + rand() % 4;
                level.height = 17 + rand() % 4;
                level.num_boxes = 7 + rand() % 3;
                break;
        }
        
        for (size_t i = 0; i < level.height; i++)
            for (size_t j = 0; j < level.width; j++) 
                level.cells[i][j] = CELL_WALL;

        GenerateMaze(&level);

        if (!PlaceGoalsAndBoxes(&level)) continue;

        int player_placed = 0;
        for (int a = 0; a < 500 && !player_placed; a++) {
            int px = 1 + rand() % (level.width - 2);
            int py = 1 + rand() % (level.height - 2);
            if (level.cells[py][px] == CELL_FLOOR && !HasBox(&level, px, py))
            {
                level.player.x = px; level.player.y = py;
                player_placed = 1;
            }
        }
        if (!player_placed) continue;

        int target_moves = (difficulty == DIFF_EASY) ? 30 : (difficulty == DIFF_MEDIUM ? 60 : 100);
        ReverseSolve(&level, target_moves);

        // отбраковываем уровни, где генератор сдался и оставил ящики на целях
        int boxes_on_goals = 0;
        for (int i = 0; i < level.num_boxes; i++)
        {
            if (IsOnGoal(&level, level.boxes[i])) boxes_on_goals++;
        }
        if (boxes_on_goals > 0) continue; // Все ящики должны быть сдвинуты с целей

        // отбраковываем уровни, которые закончились очевидным дедлоком
        if (HasDeadlock(&level)) continue;

        level.initial_state.player = level.player;
        memcpy(level.initial_state.boxes, level.boxes, sizeof(level.boxes));
        level.initial_state.step_count = 0;
        
        valid = 1;
    }

    return level;
}

void RestartLevel(Level *level)
{ // set session parameters to zero.
    level->player = level->initial_state.player;
    level->step_count = 0;
    level->time_elapsed = 0;
    level->undo_top = 0;
    memcpy(level->boxes, level->initial_state.boxes, sizeof(level->boxes));
}