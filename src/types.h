#ifndef _TYPES_H
#define _TYPES_H

#define MAX_BOXES 10
#define MAX_UNDO 512
#define MAX_FIELD 20

typedef enum
{
    CELL_FLOOR,
    CELL_WALL
} CellType;

typedef enum
{
    SCREEN_MENU,
    SCREEN_DIFFICULTY,
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_WIN,
    SCREEN_STATS,
    SCREEN_RULES
} Screen;

typedef enum
{
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD
} Difficulty;

typedef struct
{
    int x, y;
} Position;

typedef struct
{
    Position player;
    Position boxes[MAX_BOXES];
    int step_count;
} GameState;

typedef struct
{
    int session_id;
    int width, height;
    CellType cells[MAX_FIELD][MAX_FIELD];
    Position goals[MAX_BOXES];
    int num_boxes;
    Position boxes[MAX_BOXES];
    Position player;
    Difficulty difficulty;
    int step_count;
    float time_elapsed;
    GameState undo_stack[MAX_UNDO];
    int undo_top;
} Level;

#endif