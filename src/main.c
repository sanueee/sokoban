#include "raylib.h"
#include "types.h"
#include "level.h"
#include "render.h"
#include "game.h"
#include "ui.h"
#include "db.h"
#include "solver.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define SOLVER_STEP_INTERVAL 0.12f

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "Sokoban");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(FRAMES);
    SetExitKey(0);

    InitAudioDevice();
    Music music_menu = LoadMusicStream("../assets/menu.mp3");
    Music music_game = LoadMusicStream("../assets/game.mp3");
    PlayMusicStream(music_menu);

    Music *current_music = &music_menu;

    open("sokoban.db");

    Screen screen = SCREEN_LOGIN;
    Difficulty diff = DIFF_EASY;
    Level level = {0};
    Solver solver = {0};
    int quit = 0;
    int user_id = -1;
    char username[64] = {0};

    while (!WindowShouldClose() && !quit)
    {
        if (screen == SCREEN_GAME)
        {
            level.time_elapsed += GetFrameTime();

            if (IsKeyPressed(KEY_ESCAPE))
            {
                if (solver.active) FreeSolver(&solver);
                screen = SCREEN_PAUSE;
            }
            else if (solver.active)
            {
                solver.timer += GetFrameTime();
                if (solver.timer >= SOLVER_STEP_INTERVAL)
                {
                    solver.timer = 0;
                    ApplyMove(&level, solver.moves[solver.current_move]);
                    solver.current_move++;

                    if (CheckWin(&level))
                    {
                        FreeSolver(&solver);
                        save_session(user_id, diff, level.step_count, level.time_elapsed, 1);
                        screen = SCREEN_WIN;
                    }
                    else if (solver.current_move >= solver.num_moves)
                    {
                        FreeSolver(&solver);
                    }
                }

                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
                    IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A) ||
                    IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D) ||
                    IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_R))
                {
                    FreeSolver(&solver);
                }
            }
            else
            {
                bool mod = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
                           IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                if (mod && IsKeyPressed(KEY_B))
                {
                    RestartLevel(&level);
                    {
                        static const char *diff_names[] = {"easy", "medium", "hard"};
                        static FILE *solver_log = NULL;
                        struct timespec _t0, _t1;
                        clock_gettime(CLOCK_MONOTONIC, &_t0);
                        SolveLevel(&level, &solver);
                        clock_gettime(CLOCK_MONOTONIC, &_t1);
                        double _ms = (_t1.tv_sec - _t0.tv_sec) * 1000.0 +
                                     (_t1.tv_nsec - _t0.tv_nsec) / 1e6;
                        if (!solver_log) solver_log = fopen("../tests/test_solver.txt", "a");
                        if (solver_log)
                        {
                            fprintf(solver_log, "%s;%d;%.2f\n",
                                    diff_names[diff], level.num_boxes, _ms);
                            fflush(solver_log);
                        }
                    }
                }

                HandleInput(&level);
                if (CheckWin(&level))
                {
                    save_session(user_id, diff, level.step_count, level.time_elapsed, 1);
                    screen = SCREEN_WIN;
                }
            }
        }

        static Screen prev_screen = SCREEN_LOGIN;
        if (screen != prev_screen)
        {
            if (screen == SCREEN_GAME)
            {
                StopMusicStream(music_menu);
                PlayMusicStream(music_game);
                current_music = &music_game;
            }
            else if (prev_screen == SCREEN_GAME)
            {
                StopMusicStream(music_game);
                PlayMusicStream(music_menu);
                current_music = &music_menu;
            }
            prev_screen = screen;
        }
        UpdateMusicStream(*current_music);

        BeginDrawing();
        switch (screen)
        {
        case SCREEN_LOGIN:
            DrawLogin(&screen, &user_id, username);
            break;
        case SCREEN_MENU:
            DrawMenu(&screen, &quit, username);
            break;
        case SCREEN_DIFFICULTY:
            DrawDifficultySelect(&screen, &diff, &level);
            break;
        case SCREEN_SETTINGS:
            DrawSettings(&screen);
            break;
        case SCREEN_GAME:
            RenderLevel(&level);
            if (solver.active)
            {
                const char *stxt = TextFormat("AI SOLVING  %d/%d", solver.current_move, solver.num_moves);
                int stw = MeasureText(stxt, 20);
                DrawText(stxt, GetScreenWidth() / 2 - stw / 2, GetScreenHeight() - 36, 20, 
                         CLITERAL(Color){140, 200, 120, 255});
            }
            break;
        case SCREEN_PAUSE:
            RenderLevel(&level);
            DrawPause(&screen, &level, user_id, diff);
            break;
        case SCREEN_WIN:
            RenderLevel(&level);
            DrawWin(&screen, &level, diff);
            break;
        case SCREEN_RULES:
            DrawRules(&screen);
            break;
        case SCREEN_HISTORY:
            DrawHistory(&screen, user_id);
            break;
        case SCREEN_STATS:
            DrawStats(&screen, user_id);
            break;
        }
        EndDrawing();
    }

    if (solver.active) FreeSolver(&solver);
    FreeUndoStack(&level);
    close();

    UnloadMusicStream(music_menu);
    UnloadMusicStream(music_game);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}