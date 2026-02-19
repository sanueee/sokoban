#include "raylib.h"
#include "types.h"

int main(void)
{
    InitWindow(800, 600, "sokoban");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(30);

    Screen current_screen = SCREEN_MENU;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Sokoban", 350, 280, 20, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}