#include "ui.h"
#include "raylib.h"
#include "level.h"
#include <math.h>
#include <string.h>

#define C_BG CLITERAL(Color){35, 45, 35, 255}
#define C_PANEL CLITERAL(Color){45, 58, 42, 240}
#define C_ACCENT CLITERAL(Color){140, 200, 120, 255}
#define C_GREEN CLITERAL(Color){160, 220, 140, 255}
#define C_TEXT CLITERAL(Color){220, 235, 205, 255}
#define C_DIM CLITERAL(Color){120, 150, 105, 255}
#define C_BTN CLITERAL(Color){55, 72, 50, 255}
#define C_BTN_HOV CLITERAL(Color){80, 105, 72, 255}
#define C_BORDER CLITERAL(Color){80, 108, 70, 255}
#define C_BORDER_HOV CLITERAL(Color){140, 200, 120, 255}

static int Button(const char *label, int x, int y, int w, int h)
{
    Rectangle r = {(float)x, (float)y, (float)w, (float)h};
    int hov = CheckCollisionPointRec(GetMousePosition(), r);

    DrawRectangleRec(r, hov ? C_BTN_HOV : C_BTN);
    DrawRectangleLinesEx(r, 1.5f, hov ? C_BORDER_HOV : C_BORDER);

    int fs = h / 2;
    DrawText(label, x + (w - MeasureText(label, fs)) / 2,
             y + (h - fs) / 2, fs, hov ? C_TEXT : C_DIM);

    return hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

static Rectangle Overlay(int w, int h)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, CLITERAL(Color){0, 0, 0, 160});
    Rectangle r = {(float)(sw / 2 - w / 2), (float)(sh / 2 - h / 2), (float)w, (float)h};
    DrawRectangleRec(r, C_PANEL);
    DrawRectangleLinesEx(r, 2.0f, C_BORDER);
    return r;
}

void DrawMenu(Screen *screen, int *quit, const char *username)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    int fs = 18;
    int name_w = MeasureText(username, fs);
    int btn_w = 70, btn_h = 30, margin = 16;
    int total_w = name_w + 12 + btn_w;
    int nx = sw - total_w - margin;
    int ny = margin;

    DrawCircle(nx - 14, ny + btn_h / 2, 10, C_ACCENT);
    DrawText(TextFormat("%c", username[0]), nx - 20, ny + btn_h / 2 - 9, 18, C_BG);

    DrawText(username, nx, ny + (btn_h - fs) / 2, fs, C_TEXT);

    if (Button("logout", nx + name_w + 12, ny, btn_w, btn_h))
    {
        *screen = SCREEN_LOGIN;
    }

    float t = (float)GetTime();
    float p = 0.8f + 0.2f * sinf(t * 2.4f);

    int tfs = sw / 7;
    if (tfs > 120)
        tfs = 120;
    const char *title = "SOKOBAN";
    DrawText(title, (sw - MeasureText(title, tfs)) / 2, sh / 6, tfs, C_ACCENT);

    // const char *sub = "push all boxes onto their goals";
    // int sfs = 18;
    // DrawText(sub, (sw - MeasureText(sub, sfs)) / 2, sh / 6 + tfs + 10, sfs, C_DIM);

    int bw = 260, bh = 52, gap = 14, bx = sw / 2 - bw / 2, by = sh / 2;

    if (Button("PLAY", bx, by, bw, bh))
        *screen = SCREEN_DIFFICULTY;
    if (Button("RULES", bx, by + bh + gap, bw, bh))
        *screen = SCREEN_RULES;
    if (Button("STATS", bx, by + 2 * (bh + gap), bw, bh))
        *screen = SCREEN_STATS;
    if (Button("SETTINGS", bx, by + 3 * (bh + gap), bw, bh))
        *screen = SCREEN_SETTINGS;
    if (Button("EXIT", bx, by + 4 * (bh + gap), bw, bh))
        *quit = 1;

    DrawText("v1.0", 10, sh - 22, 14, C_DIM);
}

void DrawDifficultySelect(Screen *screen, Difficulty *diff, Level *level)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    int tfs = 48;
    const char *title = "SELECT DIFFICULTY";
    DrawText(title, (sw - MeasureText(title, tfs)) / 2, sh / 5, tfs, C_TEXT);

    const char *labels[] = {"EASY", "MEDIUM", "HARD"};
    const char *descs[] = {
        "9-12 tiles  |  3-4 boxes  |  ~30 moves",
        "13-16 tiles |  5-6 boxes  |  ~60 moves",
        "17-20 tiles |  7-9 boxes  |  ~100 moves",
    };
    Color dots[] = {
        {55, 185, 90, 255},
        {230, 170, 40, 255},
        {210, 60, 60, 255},
    };

    int bw = 260, bh = 56, gap = 18;
    int bx = sw / 2 - bw / 2;
    int by = sh / 2 - (bh * 3 + gap * 2) / 2;

    for (int i = 0; i < 3; i++)
    {
        int y = by + i * (bh + gap);
        DrawRectangle(bx - 22, y + bh / 2 - 6, 12, 12, dots[i]);
        if (Button(labels[i], bx, y, bw, bh))
        {
            *diff = (Difficulty)i;
            *level = GenerateLevel(*diff);
            *screen = SCREEN_GAME;
        }
        DrawText(descs[i], bx + bw + 20, y + (bh - 18) / 2, 18, C_DIM);
    }

    if (Button("BACK", bx, by + 3 * (bh + gap) + 16, bw, bh - 12))
        *screen = SCREEN_MENU;
}

static int s_fullscreen = 0;
static int s_fps_target = 60;

void DrawSettings(Screen *screen)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    int tfs = 48;
    const char *title = "SETTINGS";
    DrawText(title, (sw - MeasureText(title, tfs)) / 2, sh / 6, tfs, C_ACCENT);

    int bw = 320, bh = 52, gap = 16, bx = sw / 2 - bw / 2, by = sh / 3;

    const char *fs_label = s_fullscreen ? "FULLSCREEN: ON" : "FULLSCREEN: OFF";
    if (Button(fs_label, bx, by, bw, bh))
    {
        ToggleFullscreen();
    }

    const char *fps_label = (s_fps_target == 60) ? "FPS LIMIT: 60" : "FPS LIMIT: 30";
    if (Button(fps_label, bx, by + bh + gap, bw, bh))
    {
        s_fps_target = (s_fps_target == 60) ? 30 : 60;
        SetTargetFPS(s_fps_target);
    }

    if (Button("BACK", bx, sh - 100, bw, bh))
        *screen = SCREEN_MENU;
}

void DrawPause(Screen *screen, Level *level, int user_id, Difficulty diff)
{
    Rectangle p = Overlay(340, 310);
    int px = (int)p.x, py = (int)p.y, pw = 340;

    const char *t = "PAUSED";
    DrawText(t, px + (pw - MeasureText(t, 38)) / 2, py + 24, 38, C_ACCENT);

    int bw = 240, bh = 48, gap = 14, bx = px + (pw - bw) / 2, by = py + 90;

    if (Button("RESUME", bx, by, bw, bh))
        *screen = SCREEN_GAME;
    if (Button("RESTART", bx, by + bh + gap, bw, bh))
    {
        RestartLevel(level);
        *screen = SCREEN_GAME;
    }
    if (Button("MAIN MENU", bx, by + 2 * (bh + gap), bw, bh))
    {
        save_session(user_id, diff, level->step_count, level->time_elapsed, 0);
        *screen = SCREEN_MENU;
    }
}

void DrawWin(Screen *screen, Level *level, Difficulty diff)
{
    Rectangle p = Overlay(420, 400);
    int px = (int)p.x, py = (int)p.y, pw = 420;

    DrawRectangleLinesEx(p, 2.5f, C_GREEN);

    const char *t = "YOU WIN!";
    DrawText(t, px + (pw - MeasureText(t, 52)) / 2, py + 20, 52, C_GREEN);

    const char *dnames[] = {"Easy", "Medium", "Hard"};
    int total_s = (int)level->time_elapsed;
    int sx = px + 50, sy = py + 100, sfs = 20;

    DrawText(TextFormat("Difficulty :  %s", dnames[diff]), sx, sy, sfs, C_TEXT);
    DrawText(TextFormat("Steps      :  %d", level->step_count), sx, sy + 34, sfs, C_TEXT);
    DrawText(TextFormat("Time       :  %02d:%02d", total_s / 60, total_s % 60), sx, sy + 68, sfs, C_TEXT);

    int bw = 280, bh = 48, gap = 14, bx = px + (pw - bw) / 2, by = py + 220;

    if (Button("PLAY AGAIN", bx, by, bw, bh))
    {
        *level = GenerateLevel(diff);
        *screen = SCREEN_GAME;
    }
    if (Button("CHANGE DIFF", bx, by + bh + gap, bw, bh))
        *screen = SCREEN_DIFFICULTY;
    if (Button("MAIN MENU", bx, by + 2 * (bh + gap), bw, bh))
        *screen = SCREEN_MENU;
}

void DrawRules(Screen *screen)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    DrawText("HOW TO PLAY", (sw - MeasureText("HOW TO PLAY", 48)) / 2, 50, 48, C_ACCENT);

    struct
    {
        const char *text;
        Color col;
    } lines[] = {
        {"Goal", C_ACCENT},
        {"Push ALL boxes onto the purple diamond goals.", C_TEXT},
        {"", C_TEXT},
        {"Controls", C_ACCENT},
        {"WASD / Arrows   move", C_TEXT},
        {"Z               undo last move", C_TEXT},
        {"R               restart level", C_TEXT},
        {"ESC             pause", C_TEXT},
        {"", C_TEXT},
        {"Tips", C_ACCENT},
        {"Boxes can only be PUSHED, not pulled.", C_TEXT},
        {"Boxes stuck in corners can't be moved — plan ahead!", C_TEXT},
    };

    int fs = 22, y = 130, x = sw / 2 - 340;
    for (int i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++)
    {
        if (lines[i].text[0] == '\0')
        {
            y += fs / 2;
            continue;
        }
        DrawText(lines[i].text, x, y, fs, lines[i].col);
        y += fs + 10;
    }

    int bw = 200, bh = 46;
    if (Button("BACK", sw / 2 - bw / 2, sh - 76, bw, bh))
        *screen = SCREEN_MENU;
}

void DrawStats(Screen *screen, int user_id)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    DrawText("STATISTICS", (sw - MeasureText("STATISTICS", 48)) / 2, 50, 48, C_ACCENT);

    Session sessions[64];
    int count = get_sessions(user_id, sessions, 64);

    int total = count;
    int wins = 0;
    int total_steps = 0;
    int total_time = 0;
    for (int i = 0; i < count; i++)
    {
        if (sessions[i].completed)
            wins++;
        total_steps += sessions[i].steps;
        total_time += sessions[i].time;
    }

    int sx = sw / 2 - 300, sy = 140, fs = 22, gap = 38;
    DrawText(TextFormat("Games played :  %d", total), sx, sy, fs, C_TEXT);
    DrawText(TextFormat("Wins         :  %d", wins), sx, sy + gap, fs, C_TEXT);
    DrawText(TextFormat("Losses       :  %d", total - wins), sx, sy + gap * 2, fs, C_TEXT);
    DrawText(TextFormat("Total steps  :  %d", total_steps), sx, sy + gap * 3, fs, C_TEXT);
    DrawText(TextFormat("Total time   :  %02d:%02d",
                        total_time / 60, total_time % 60),
             sx, sy + gap * 4, fs, C_TEXT);

    if (total > 0)
    {
        DrawText(TextFormat("Win rate     :  %d%%",
                            wins * 100 / total),
                 sx, sy + gap * 5, fs, C_ACCENT);
    }

    DrawRectangle(sx, sy + gap * 6, 500, 1, C_BORDER);

    // последние 5 игр
    DrawText("Recent games:", sx, sy + gap * 6 + 16, fs, C_DIM);
    int max_show = count < 5 ? count : 5;
    const char *dnames[] = {"Easy", "Medium", "Hard"};
    for (int i = 0; i < max_show; i++)
    {
        int y = sy + gap * 7 + i * 30;
        Color rc = sessions[i].completed ? C_GREEN : C_DIM;
        DrawText(TextFormat("%s  |  %d steps  |  %02d:%02d  |  %s  |  %s",
                            dnames[sessions[i].difficulty],
                            sessions[i].steps,
                            sessions[i].time / 60, sessions[i].time % 60,
                            sessions[i].completed ? "WIN" : "quit",
                            sessions[i].played_at),
                 sx, y, 18, rc);
    }

    if (count == 0)
        DrawText("No sessions yet.", (sw - MeasureText("No sessions yet.", 22)) / 2,
                 sh / 2, 22, C_DIM);

    int bw = 200, bh = 46;
    if (Button("BACK", sw / 2 - bw / 2, sh - 76, bw, bh))
        *screen = SCREEN_MENU;
}

void DrawHistory(Screen *screen, int user_id)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    DrawText("HISTORY", (sw - MeasureText("HISTORY", 48)) / 2, 50, 48, C_ACCENT);

    Session sessions[64];
    int count = get_sessions(user_id, sessions, 64);

    const char *dnames[] = {"Easy", "Medium", "Hard"};
    int tx = sw / 2 - 400, ty = 130, fs = 20;

    // заголовки
    DrawText("Difficulty", tx, ty, fs, C_DIM);
    DrawText("Steps", tx + 180, ty, fs, C_DIM);
    DrawText("Time", tx + 300, ty, fs, C_DIM);
    DrawText("Result", tx + 420, ty, fs, C_DIM);
    DrawText("Date", tx + 540, ty, fs, C_DIM);
    DrawRectangle(tx, ty + 28, 700, 1, C_BORDER);

    if (count == 0)
    {
        DrawText("No sessions yet.", (sw - MeasureText("No sessions yet.", 22)) / 2,
                 sh / 2, 22, C_DIM);
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            int y = ty + 44 + i * 34;
            Color rc = sessions[i].completed ? C_GREEN : C_DIM;

            DrawText(dnames[sessions[i].difficulty], tx, y, fs, C_TEXT);
            DrawText(TextFormat("%d", sessions[i].steps), tx + 180, y, fs, C_TEXT);
            DrawText(TextFormat("%02d:%02d",
                                sessions[i].time / 60,
                                sessions[i].time % 60),
                     tx + 300, y, fs, C_TEXT);
            DrawText(sessions[i].completed ? "WIN" : "quit", tx + 420, y, fs, rc);
            DrawText(sessions[i].played_at, tx + 540, y, fs, C_DIM);
        }
    }

    int bw = 200, bh = 46;
    if (Button("BACK", sw / 2 - bw / 2, sh - 76, bw, bh))
        *screen = SCREEN_MENU;
}

void DrawLogin(Screen *screen, int *user_id, char *username)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    ClearBackground(C_BG);

    int tfs = 48;
    const char *title = "WHO ARE YOU?";
    DrawText(title, (sw - MeasureText(title, tfs)) / 2, sh / 7, tfs, C_ACCENT);

    static char input[64] = {0};
    static int input_len = 0;

    int ch;
    while ((ch = GetCharPressed()) != 0)
    {
        if (input_len < 63 && ch >= 32)
        {
            input[input_len++] = (char)ch;
            input[input_len] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && input_len > 0)
        input[--input_len] = '\0';

    int fw = 360, fh = 52, fx = sw / 2 - fw / 2, fy = sh / 3;
    bool focused = CheckCollisionPointRec(GetMousePosition(),
                                          (Rectangle){(float)fx, (float)fy, (float)fw, (float)fh});

    DrawRectangle(fx, fy, fw, fh, C_BTN);
    DrawRectangleLinesEx((Rectangle){(float)fx, (float)fy, (float)fw, (float)fh},
                         2.0f, focused ? C_BORDER_HOV : C_BORDER);

    int txt_fs = 24;
    DrawText(input, fx + 12, fy + (fh - txt_fs) / 2, txt_fs, C_TEXT);
    if ((int)(GetTime() * 2) % 2 == 0)
    {
        int cursor_x = fx + 12 + MeasureText(input, txt_fs);
        DrawRectangle(cursor_x, fy + 10, 2, fh - 20, C_ACCENT);
    }

    if (input_len == 0)
        DrawText("Enter your name...", fx + 12, fy + (fh - txt_fs) / 2, txt_fs, C_DIM);

    int bw = 360, bh = 50, bx = sw / 2 - bw / 2, by = fy + fh + 18;
    if (input_len > 0 && Button("CONTINUE", bx, by, bw, bh))
    {
        *user_id = create_user(input);
        strncpy(username, input, 63);
        memset(input, 0, sizeof(input));
        input_len = 0;
        *screen = SCREEN_MENU;
    }

    User users[32];
    int count = get_all_users(users, 32);

    if (count > 0)
    {
        int lfs = 20;
        const char *sub = "or pick existing:";
        DrawText(sub, (sw - MeasureText(sub, lfs)) / 2,
                 by + bh + 32, lfs, C_DIM);

        int uw = 260, uh = 44, ugap = 10;
        int ux = sw / 2 - uw / 2, uy = by + bh + 62;

        for (int i = 0; i < count; i++)
        {
            if (Button(users[i].name, ux, uy + i * (uh + ugap), uw, uh))
            {
                *user_id = users[i].id;
                strncpy(username, users[i].name, 63);
                memset(input, 0, sizeof(input));
                input_len = 0;
                *screen = SCREEN_MENU;
            }
        }
    }
}
