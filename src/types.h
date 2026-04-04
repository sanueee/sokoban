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

typedef enum
{
    UNDO_MOVE,
    UNDO_PUSH
} UndoType;

typedef struct
{
    Position player_pos;
} UndoMove;

typedef struct
{
    Position player_pos;
    int box_index;
    Position box_pos;
} UndoPush;

typedef union
{
    UndoMove move;
    UndoPush push;
} UndoData;

typedef struct UndoNode
{
    UndoType type;
    UndoData data;
    int step_count;
    struct UndoNode *next;
} UndoNode;

typedef struct
{
    int *moves;
    int num_moves;
    int current_move;
    bool active;
    float timer;
} Solver;

typedef struct
{
    unsigned short player;
    unsigned short boxes[MAX_BOXES];
} PackedState;

// узел A*
typedef struct
{
    PackedState state;
    int parent;      // индекс родителя в пуле (-1 для корня)
    int direction;   // направление хода (0-3), -1 для корня
    int g;           // стоимость пути от старта
    int f;           // g + h (приоритет в куче)
} AStarNode;

// пул узлов (динамический массив, растёт ×2)
typedef struct
{
    AStarNode *data;
    int count;
    int capacity;
} NodePool;

// бинарная мин-куча по f (приоритетная очередь)
typedef struct
{
    int *idx;       // индексы в NodePool
    int size;
    int capacity;
} MinHeap;

// хеш-таблица с открытой адресацией (PackedState)
typedef struct
{
    PackedState *states;
    char *occupied;
    int capacity;
    int mask;        // capacity - 1
    int count;
} HashSet;

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
    UndoNode *undo_head;
    int undo_count;
    GameState initial_state;
} Level;

#endif