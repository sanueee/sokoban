#ifndef _TYPES_H
#define _TYPES_H

#define MAX_BOXES 10
#define MAX_FIELD 20
#include <stdbool.h>

typedef enum
{
    CELL_FLOOR,
    CELL_WALL
} CellType;

typedef enum
{
    SCREEN_MENU,
    SCREEN_DIFFICULTY,
    SCREEN_SETTINGS,
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_HISTORY,
    SCREEN_LOGIN,
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

// узел стека отмены (линейный список)
typedef struct UndoNode
{
    GameState state;
    struct UndoNode *next;
} UndoNode;

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
    UndoNode *undo_head;  // вершина стека (linked list)
    int undo_count;       // количество элементов в стеке
    GameState initial_state;
} Level;

#endif