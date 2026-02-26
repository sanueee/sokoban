#include "render.h"

#define COLOR_WALL      (Color){30, 30, 30, 255}
#define COLOR_FLOOR     (Color){200, 200, 200, 255}
#define COLOR_PLAYER    (Color){255, 50, 50, 255}
#define COLOR_BOX       (Color){180, 120, 40, 255}
#define COLOR_BOX_ON_GOAL (Color){50, 200, 100, 255}
#define COLOR_GOAL      (Color){255, 200, 0, 255}

void RenderLevel(const Level *level)
{
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // место под управление игрой, таймер, статистику
    int hud_height = 50;

    int available_w = screen_w;
    int available_h = screen_h - hud_height;

    int tile = available_w / level->width;
    if (available_h / level->height < tile)
        tile = available_h / level->height;

    // центрируем 
    int offset_x = (available_w - tile * level->width) / 2;
    int offset_y = hud_height + (available_h - tile * level->height) / 2;

    for (int y = 0; y < level->height; y++)
    {
        for (int x = 0; x < level->width; x++)
        {
            Color color = (level->cells[y][x] == CELL_WALL) ? COLOR_WALL : COLOR_FLOOR;
            DrawRectangle( offset_x + x * tile, offset_y + y * tile, tile - 1 , tile - 1, color);
        }
    }

    for (int i = 0; i < level->num_boxes; i++) {
        int x = level->goals[i].x;
        int y = level->goals[i].y;
        DrawRectangle(offset_x + x * tile + tile/4, offset_y + y * tile + tile/4,
                      tile/2, tile/2, COLOR_GOAL);
    }

    for (int i = 0; i < level->num_boxes; i++) {
        int x = level->boxes[i].x;
        int y = level->boxes[i].y;

        // Проверить стоит ли ящик на цели
        Color color = COLOR_BOX;
        for (int j = 0; j < level->num_boxes; j++) {
            if (level->goals[j].x == x && level->goals[j].y == y) {
                color = COLOR_BOX_ON_GOAL;
                break;
            }
        }
        DrawRectangle(offset_x + x * tile + 2, offset_y + y * tile + 2,
                      tile - 4, tile - 4, color);
    }

    // Отрисовка игрока
    int px = level->player.x;
    int py = level->player.y;
    DrawRectangle(offset_x + px * tile + 4, offset_y + py * tile + 4,
                  tile - 8, tile - 8, COLOR_PLAYER);
}