#include "raylib.h"
#include "types.h"
#include "level.h"
#include "render.h"
#include "game.h"
#include "ui.h"
#include "db.h"

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "Sokoban");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(FRAMES);
    SetExitKey(0);

    InitAudioDevice();
    Music music_menu = LoadMusicStream("assets/menu.mp3");
    Music music_game = LoadMusicStream("assets/game.mp3");
    PlayMusicStream(music_menu);

    Music *current_music = &music_menu;

    open("sokoban.db");

    Screen screen = SCREEN_LOGIN;
    Difficulty diff = DIFF_EASY;
    Level level = {0};
    int quit = 0;
    int user_id = -1;
    char username[64] = {0};

    while (!WindowShouldClose() && !quit)
    {
        if (screen == SCREEN_GAME)
        {
            level.time_elapsed += GetFrameTime();
            if (IsKeyPressed(KEY_ESCAPE))
                screen = SCREEN_PAUSE;
            else
            {
                HandleInput(&level);
                if (CheckWin(&level))
                {
                    save_session(user_id, diff,
                                 level.step_count, level.time_elapsed, 1);
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
            break;
        case SCREEN_PAUSE:
            RenderLevel(&level);
            DrawPause(&screen, &level, user_id, diff); // передаём для записи при выходе
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

    close();

    UnloadMusicStream(music_menu);
    UnloadMusicStream(music_game);
    CloseAudioDevice();

    CloseWindow();
    return 0;
}
