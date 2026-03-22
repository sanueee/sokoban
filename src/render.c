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

#define C_HAT (Color){240, 225, 180, 255}
#define C_HAT_SH (Color){190, 170, 125, 255}
#define C_GI (Color){230, 240, 220, 255}
#define C_HAKAMA (Color){80, 120, 90, 255}
#define C_EYE (Color){200, 240, 180, 255}
#define C_BLACK (Color){30, 42, 28, 255}
#define C_STEEL (Color){190, 210, 195, 255}

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

// dir: 0 - Down, 1 - Up, 2 - Left, 3 - Right
static void DrawPlayerTile(int x, int y, int t, float walk_phase, int dir, bool is_pushing)
{
    // Параметры анимации
    int frame = (int)(walk_phase * 10) % 4; // 4 кадра анимации
    int bob = (frame % 2 == 0) ? 0 : 1;     // Легкое покачивание при ходьбе

    // Если игрок стоит (walk_phase == 0), делаем эффект дыхания
    if (walk_phase == 0)
        bob = (int)(GetTime() * 2) % 2;

    // Центрирование и базовые размеры
    int ox = x + t / 10;
    int oy = y + bob;
    int sw = t - (t / 5); // ширина персонажа

    // 1. Отрисовка НОГ (Хакама)
    // При движении вверх/вниз ноги меняются местами. При толкании — упор.
    if (is_pushing)
    {
        DrawRectangle(ox + 2, oy + t * 2 / 3, sw - 4, t / 3, C_HAKAMA); // Широкая стойка
    }
    else
    {
        int leg_off = (frame % 2 == 0) ? 4 : -4;
        DrawRectangle(ox + leg_off, oy + t * 2 / 3, sw / 3, t / 3, C_HAKAMA);           // Левая нога
        DrawRectangle(ox + leg_off * leg_off, oy + t * 2 / 3, sw / 3, t / 3, C_HAKAMA); // Правая нога
    }

    // 2. Отрисовка ТУЛОВИЩА (Белое кимоно)
    int body_h = t / 2;
    int push_offset = (is_pushing && (dir == 2 || dir == 3)) ? (dir == 3 ? 4 : -4) : 0;
    DrawRectangle(ox + 4 + push_offset, oy + t / 3, sw - 8, body_h, C_GI);

    // 3. КАТАНА
    // При движении ВВЕРХ (dir == 1) рисуем за спиной по диагонали
    if (dir == 1)
    {
        DrawRectangle(ox + 2, oy + t / 3, sw - 4, 3, C_STEEL); // Горизонтально за спиной
    }
    else if (dir == 3)
    { // Направо - видна сбоку
        DrawRectangle(ox + sw - 2, oy + t / 2, 6, 2, C_STEEL);
    }
    else if (dir == 2)
    { // Налево
        DrawRectangle(ox - 4, oy + t / 2, 6, 2, C_STEEL);
    }

    // 4. ГОЛОВА (Тень под шляпой)
    DrawRectangle(ox + 6, oy + t / 6, sw - 12, t / 4, C_BLACK);

    // ГЛАЗА (только если не смотрим вверх)
    if (dir != 1)
    {
        int eye_blink = (GetTime() > 4.0 && (int)GetTime() % 5 == 0) ? 0 : 2; // Иногда моргает
        if (eye_blink > 0)
        {
            DrawRectangle(ox + 8, oy + t / 4, 2, 2, C_EYE);
            DrawRectangle(ox + sw - 10, oy + t / 4, 2, 2, C_EYE);
        }
    }

    // 5. ШЛЯПА (Сугегаса) — визитная карточка
    // Рисуем тремя слоями для создания конической формы
    DrawRectangle(ox, oy + t / 6, sw, 4, C_HAT_SH);       // Нижний край (тень)
    DrawRectangle(ox + 2, oy + t / 10, sw - 4, 4, C_HAT); // Средняя часть
    DrawRectangle(ox + sw / 2 - 2, oy, 6, 4, C_HAT);      // Верхушка
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

    // todo
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

    // 2. Отрисовка целей
    for (int i = 0; i < level->num_boxes; i++)
    {
        DrawGoalTile(ox + level->goals[i].x * tile, oy + level->goals[i].y * tile, tile);
    }

    // 3. Отрисовка ящиков
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

    // 4. Отрисовка ИГРОКА (Обновлено)
    // Если игрок стоит, фаза 0. Если идет — берем время.
    float walk_phase = 0;
    if (level->character.is_moving)
    {
        walk_phase = (float)GetTime() * 8.0f;
    }

    DrawPlayerTile(
        ox + level->player.x * tile,
        oy + level->player.y * tile,
        tile,
        walk_phase,
        level->character.dir,
        level->character.is_pushing);

    DrawRectangle(0, 0, sw, hud_h, C_HUD_BG);
    DrawRectangle(0, hud_h - 1, sw, 1, C_HUD_LINE);

    DrawText(TextFormat("Steps: %d", level->step_count), 14, 15, 20, C_HUD_TEXT);

    int total_s = (int)level->time_elapsed;
    DrawText(TextFormat("Time: %02d:%02d", total_s / 60, total_s % 60), sw / 2 - 52, 15, 20, C_HUD_TEXT);

    const char *diff_names[] = {"Easy", "Medium", "Hard"};
    const char *diff_str = diff_names[level->difficulty];
    int diff_w = MeasureText(diff_str, 20);
    DrawText(diff_str, sw - diff_w - 14, 15, 20, C_HUD_DIM);
}
