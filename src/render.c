#include "render.h"

#define C_BG CLITERAL(Color){35, 45, 35, 255}

#define C_WALL CLITERAL(Color){72, 90, 65, 255}
#define C_WALL_HI CLITERAL(Color){110, 130, 100, 255}
#define C_WALL_SH CLITERAL(Color){40, 55, 38, 255}
#define C_WALL_LINE CLITERAL(Color){55, 72, 50, 255}

#define C_FLOOR CLITERAL(Color){52, 68, 48, 255}
#define C_FLOOR_DOT CLITERAL(Color){75, 95, 68, 255}

#define C_GOAL CLITERAL(Color){160, 210, 140, 255}
#define C_GOAL_DIM CLITERAL(Color){90, 130, 75, 255}

// box
#define C_BOX CLITERAL(Color){220, 200, 165, 255}
#define C_BOX_HI CLITERAL(Color){245, 230, 200, 255}
#define C_BOX_SH CLITERAL(Color){150, 130, 100, 255}
#define C_BOX_CROSS CLITERAL(Color){180, 160, 125, 255}

// box on goal
#define C_BOX_ON CLITERAL(Color){140, 195, 120, 255}
#define C_BOX_ON_HI CLITERAL(Color){180, 230, 160, 255}
#define C_BOX_ON_SH CLITERAL(Color){75, 130, 60, 255}
#define C_BOX_ON_CROSS CLITERAL(Color){95, 160, 80, 255}

#define C_PLAYER_SKIN (Color){212, 197, 169, 255}
#define C_PLAYER_BODY (Color){ 74, 124,  89, 255}
#define C_PLAYER_LEGS (Color){ 58,  99,  73, 255}
#define C_PLAYER_EYE  (Color){ 26,  46,  26, 255}

#define C_HUD_BG CLITERAL(Color){25, 35, 25, 255}
#define C_HUD_LINE CLITERAL(Color){70, 95, 62, 255}
#define C_HUD_TEXT CLITERAL(Color){210, 230, 195, 255}
#define C_HUD_DIM CLITERAL(Color){120, 155, 108, 255}

static void DrawDiamond(int cx, int cy, int r, Color col)
{ // draws the goal
    for (int i = 0; i <= r; i++)
    {
        int w = i * 2 + 1;
        DrawRectangle(cx - i, cy - r + i, w, 1, col); // верхняя половина + центр
    }
    for (int i = 0; i < r; i++)
    {
        int w = i * 2 + 1;
        DrawRectangle(cx - i, cy + r - i, w, 1, col); // нижняя половина
    }
}

static void DrawWallTile(int x, int y, int t)
{ // 3D effect
    DrawRectangle(x, y, t, t, C_WALL);

    // cветлые верх, лево
    DrawRectangle(x, y, t, 2, C_WALL_HI);
    DrawRectangle(x, y, 2, t, C_WALL_HI);

    // тёмные низ, право
    DrawRectangle(x, y + t - 2, t, 2, C_WALL_SH);
    DrawRectangle(x + t - 2, y, 2, t, C_WALL_SH);

    if (t >= 14)
        DrawRectangle(x + 3, y + t / 2, t - 6, 1, C_WALL_LINE);
}

static void DrawFloorTile(int x, int y, int t)
{ // draws the floor
    DrawRectangle(x, y, t, t, C_FLOOR);
    if (t >= 10)
    {
        DrawRectangle(x, y, 1, 1, C_FLOOR_DOT);
        DrawRectangle(x + t - 1, y, 1, 1, C_FLOOR_DOT);
        DrawRectangle(x, y + t - 1, 1, 1, C_FLOOR_DOT);
        DrawRectangle(x + t - 1, y + t - 1, 1, 1, C_FLOOR_DOT);
    }
}

static void DrawGoalTile(int x, int y, int t)
{ // draws floor with goal
    DrawFloorTile(x, y, t);

    int cx = x + t / 2;
    int cy = y + t / 2;
    int r = t / 5;
    if (r < 2)
        r = 2;

    DrawDiamond(cx, cy, r + 2, C_GOAL_DIM);
    DrawDiamond(cx, cy, r, C_GOAL);
}

static void DrawBoxTile(int x, int y, int t, int on_goal)
{ // draws the box
    Color base = on_goal ? C_BOX_ON : C_BOX;
    Color hi = on_goal ? C_BOX_ON_HI : C_BOX_HI;
    Color sh = on_goal ? C_BOX_ON_SH : C_BOX_SH;
    Color cross = on_goal ? C_BOX_ON_CROSS : C_BOX_CROSS;

    int pad = 3;
    int bx = x + pad, by = y + pad;
    int bw = t - pad * 2, bh = t - pad * 2;

    DrawRectangle(bx, by, bw, bh, base);

    DrawRectangle(bx, by, bw, 2, hi);
    DrawRectangle(bx, by, 2, bh, hi);

    DrawRectangle(bx, by + bh - 2, bw, 2, sh);
    DrawRectangle(bx + bw - 2, by, 2, bh, sh);

    int cx = x + t / 2;
    int cy = y + t / 2;
    int arm = t / 4;
    DrawRectangle(cx - 1, cy - arm, 2, arm * 2, cross);
    DrawRectangle(cx - arm, cy - 1, arm * 2, 2, cross);

    if (on_goal)
        DrawRectangleLines(x + 1, y + 1, t - 2, t - 2, C_BOX_ON_HI);
}

static void DrawPlayerTile(int x, int y, int t)
{ // draws the player
    int cx = x + t / 2;

    int head_size = t / 3;
    int head_x = cx - head_size / 2;
    int head_y = y + t / 10;
    DrawRectangle(head_x, head_y, head_size, head_size, C_PLAYER_SKIN);

    DrawRectangle(head_x + head_size / 5.5, head_y + head_size / 3, 2, 2, C_PLAYER_EYE);
    DrawRectangle(head_x + head_size * 4 / 5.5, head_y + head_size / 3, 2, 2, C_PLAYER_EYE);

    int body_w = t * 2 / 5;
    int body_h = t / 3;
    int body_x = cx - body_w / 2;
    int body_y = head_y + head_size + 1;
    DrawRectangle(body_x, body_y, body_w, body_h, C_PLAYER_BODY);

    int leg_w = body_w / 3;
    int leg_h = t / 5;
    int leg_y = body_y + body_h;
    DrawRectangle(body_x + 1, leg_y, leg_w, leg_h, C_PLAYER_LEGS);
    DrawRectangle(body_x + body_w - leg_w - 1, leg_y, leg_w, leg_h, C_PLAYER_LEGS);
}

void RenderLevel(const Level *level)
{ // main func
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    ClearBackground(C_BG);

    int hud_h = 50;
    int avail_w = sw;
    int avail_h = sh - hud_h;
    int tile = avail_w / level->width;
    if (avail_h / level->height < tile)
        tile = avail_h / level->height;

    // центрируем поле
    int ox = (avail_w - tile * level->width) / 2;
    int oy = hud_h + (avail_h - tile * level->height) / 2;

    // клетки
    for (int y = 0; y < level->height; y++)
    {
        for (int x = 0; x < level->width; x++)
        {
            int px = ox + x * tile;
            int py = oy + y * tile;
            if (level->cells[y][x] == CELL_WALL)
                DrawWallTile(px, py, tile);
            else
                DrawFloorTile(px, py, tile);
        }
    }

    // целевые
    for (int i = 0; i < level->num_boxes; i++)
    {
        DrawGoalTile(ox + level->goals[i].x * tile, oy + level->goals[i].y * tile, tile);
    }

    // коробки
    for (int i = 0; i < level->num_boxes; i++)
    {
        int bx = level->boxes[i].x;
        int by = level->boxes[i].y;
        bool on_goal = false;
        for (int j = 0; j < level->num_boxes; j++)
        {
            if (level->goals[j].x == bx && level->goals[j].y == by)
            {
                on_goal = true;
                break;
            }
        }
        DrawBoxTile(ox + bx * tile, oy + by * tile, tile, on_goal);
    }

    // игрок
    DrawPlayerTile(ox + level->player.x * tile, oy + level->player.y * tile, tile);

    // hud
    DrawRectangle(0, 0, sw, hud_h, C_HUD_BG);
    DrawRectangle(0, hud_h - 1, sw, 1, C_HUD_LINE);
    DrawText(TextFormat("Steps: %d", level->step_count), 14, 15, 20, C_HUD_TEXT);
    int total_s = (int)level->time_elapsed;
    DrawText(TextFormat("Time: %02d:%02d", total_s / 60, total_s % 60), sw / 2 - 52, 15, 20, C_HUD_TEXT);

    // процент ящиков на целях
    int on_goal = 0;
    for (int i = 0; i < level->num_boxes; i++)
        for (int j = 0; j < level->num_boxes; j++)
            if (level->boxes[i].x == level->goals[j].x &&
                level->boxes[i].y == level->goals[j].y)
            { on_goal++; break; }
    int pct = level->num_boxes > 0 ? (on_goal * 100 / level->num_boxes) : 0;
    const char *pct_str = TextFormat("%d/%d  %d%%", on_goal, level->num_boxes, pct);
    int pct_w = MeasureText(pct_str, 20);
    DrawText(pct_str, sw / 2 - 52 - pct_w - 40, 15, 20, on_goal == level->num_boxes ? C_HUD_TEXT : C_HUD_DIM);

    const char *diff_names[] = {"Easy", "Medium", "Hard"};
    const char *diff_str = diff_names[level->difficulty];
    int diff_w = MeasureText(diff_str, 20);
    DrawText(diff_str, sw - diff_w - 14, 15, 20, C_HUD_DIM);
}
