#include "raylib.h"
#include "types.h"
#include "level.h"
#include "render.h"
#include "game.h"

int main(void)
{
    InitWindow(800, 600, "sokoban");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(30);

    Level level = GenerateLevel(DIFF_HARD);

    while (!WindowShouldClose())
    {
        HandleInput(&level);

        if (CheckWin(&level)) {
            // пока просто закрываем окно
            break;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        RenderLevel(&level);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}