/*
 * solver.c — автоматический решатель уровней Sokoban.
 *
 * Алгоритм: A* (A-star) по пространству состояний игры.
 *
 * Состояние (PackedState) — это позиция игрока + отсортированный массив
 * позиций всех ящиков. Два состояния равны, если игрок стоит в том же
 * месте и все ящики расставлены одинаково.
 *
 * Позиция кодируется одним uint16_t: pos = y * MAX_FIELD + x.
 *
 * Структуры данных:
 *   NodePool  — плоский массив всех порождённых узлов A*.
 *               Каждый узел хранит состояние, индекс родителя,
 *               направление хода и значения g, f.
 *   MinHeap   — бинарная куча (min-heap) по f; хранит индексы в NodePool.
 *               Это «открытый список» (open list) A*.
 *   HashSet   — хеш-таблица с открытой адресацией; хранит PackedState.
 *               Это «закрытый список» (closed list) A* — уже посещённые
 *               состояния, чтобы не обрабатывать их повторно.
 *
 * Эвристика h(n) — сумма Manhattan-расстояний каждого ящика до его
 * ближайшей цели. Эвристика допустима (не завышает реальную стоимость),
 * поэтому A* находит оптимальный путь.
 *
 * Дополнительная оптимизация: обнаружение дедлоков (IsDeadState) —
 * если после толчка ящик попал в позицию, из которой его никогда не
 * вытолкнуть на цель, ветка отсекается без раскрытия.
 */

#include "solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

/* Начальные/максимальные ёмкости динамических структур. */
#define NODES_INIT_CAP  500000
#define NODES_MAX_CAP   100000000
#define HEAP_INIT_CAP   500000
#define HASH_INIT_CAP   1048576   // 2^20, степень двойки

/* Лимит итераций — защита от зависания на неразрешимых уровнях. */
#define MAX_ITERATIONS  100000000

/* Векторы смещений для четырёх направлений: вверх, вниз, влево, вправо. */
static const int SDX[4] = {0, 0, -1, 1};
static const int SDY[4] = {-1, 1, 0, 0};

/* ---------- Вспомогательные функции для работы с ящиками ---------- */

/*
 * SortBoxes — сортировка массива позиций ящиков вставками.
 * Порядок позиций не важен для игры, но важен для сравнения состояний:
 * одни и те же ящики всегда дают одинаковую PackedState независимо от
 * порядка их толчков.
 */
static void SortBoxes(uint16_t *boxes, int n)
{
    for (int i = 1; i < n; i++)
    {
        uint16_t key = boxes[i];
        int j = i - 1;
        while (j >= 0 && boxes[j] > key)
        {
            boxes[j + 1] = boxes[j];
            j--;
        }
        boxes[j + 1] = key;
    }
}

/*
 * IsBoxAt — линейный поиск ящика по позиции pos.
 * Возвращает индекс в массиве boxes или -1, если ящика нет.
 * (Количество ящиков мало — до MAX_BOXES=10 — поэтому O(n) достаточно.)
 */
static int IsBoxAt(const uint16_t *boxes, int n, uint16_t pos)
{
    for (int i = 0; i < n; i++)
        if (boxes[i] == pos) return i;
    return -1;
}

/*
 * IsGoal — проверяет, является ли позиция pos одной из целевых клеток.
 */
static int IsGoal(const uint16_t *goals, int n, uint16_t pos)
{
    for (int i = 0; i < n; i++)
        if (goals[i] == pos) return 1;
    return 0;
}

/* ---------- Эвристика A* ---------- */

/*
 * Heuristic — нижняя оценка оставшейся стоимости пути (h(n)).
 *
 * Для каждого ящика берём Manhattan-расстояние до ближайшей цели и
 * суммируем. Это допустимая эвристика: реальное число ходов не может
 * быть меньше этой суммы, поэтому A* гарантированно найдёт кратчайший
 * путь.
 *
 * Недостаток: не учитывает конкуренцию ящиков за одну цель. Для Sokoban
 * это может привести к тому, что несколько ящиков «претендуют» на одну
 * ближайшую цель — эвристика занижена, но остаётся допустимой.
 */
static int Heuristic(const uint16_t *boxes, const uint16_t *goals, int n)
{
    int h = 0;
    for (int i = 0; i < n; i++)
    {
        int bx = boxes[i] % MAX_FIELD;
        int by = boxes[i] / MAX_FIELD;
        int best = INT_MAX;
        for (int j = 0; j < n; j++)
        {
            int gx = goals[j] % MAX_FIELD;
            int gy = goals[j] / MAX_FIELD;
            int dist = abs(bx - gx) + abs(by - gy);
            if (dist < best) best = dist;
        }
        h += best;
    }
    return h;
}

/* ---------- Обнаружение дедлоков ---------- */

/*
 * IsDeadState — проверяет, является ли расстановка ящиков тупиковой
 * (дедлоком), то есть состоянием, из которого решение уже невозможно.
 *
 * Реализованы два вида дедлоков:
 *
 * 1. Угловой дедлок (corner deadlock):
 *    Ящик упёрся в угол из двух стен. Вытолкнуть его из угла невозможно.
 *    Пример: ящик в клетке, где слева и сверху стены.
 *
 * 2. Блокировка 2×2 (freeze deadlock):
 *    Четыре клетки в квадрате 2×2 заняты стенами или ящиками, и хотя бы
 *    один ящик в этом квадрате не стоит на цели. Ни один ящик в таком
 *    квадрате никогда не сдвинется.
 *
 * Проверка выполняется только для ящиков не на цели — ящик на цели не
 * создаёт дедлок даже в углу.
 *
 * Эти проверки значительно сокращают дерево поиска, отсекая заведомо
 * проигрышные ветки.
 */
static int IsDeadState(const Level *level, const uint16_t *boxes, int nb, const uint16_t *goals)
{
    for (int i = 0; i < nb; i++)
    {
        if (IsGoal(goals, nb, boxes[i])) continue; // ящик уже на цели — пропускаем

        int x = boxes[i] % MAX_FIELD;
        int y = boxes[i] / MAX_FIELD;

        // 1) Угловой дедлок: две смежные стены вокруг ящика
        int w_up    = (level->cells[y - 1][x] == CELL_WALL);
        int w_down  = (level->cells[y + 1][x] == CELL_WALL);
        int w_left  = (level->cells[y][x - 1] == CELL_WALL);
        int w_right = (level->cells[y][x + 1] == CELL_WALL);

        if ((w_up && w_left) || (w_up && w_right) ||
            (w_down && w_left) || (w_down && w_right))
            return 1;

        // 2) Дедлок 2×2: проверяем все четыре квадрата, в которых участвует
        //    текущий ящик (ящик может быть в любом из четырёх углов квадрата)
        int offsets[4][2] = {{0,0}, {-1,0}, {0,-1}, {-1,-1}};
        for (int q = 0; q < 4; q++)
        {
            int bx = x + offsets[q][0]; // левый верхний угол квадрата
            int by = y + offsets[q][1];
            if (bx < 0 || bx + 1 >= level->width || by < 0 || by + 1 >= level->height)
                continue;

            // Четыре клетки квадрата
            uint16_t cells[4] = {
                (uint16_t)(by       * MAX_FIELD + bx),
                (uint16_t)(by       * MAX_FIELD + bx + 1),
                (uint16_t)((by + 1) * MAX_FIELD + bx),
                (uint16_t)((by + 1) * MAX_FIELD + bx + 1)
            };

            int all_blocked = 1, any_box_off_goal = 0;
            for (int c = 0; c < 4; c++)
            {
                int cx = cells[c] % MAX_FIELD;
                int cy = cells[c] / MAX_FIELD;
                int is_wall = (level->cells[cy][cx] == CELL_WALL);
                int is_box  = (IsBoxAt(boxes, nb, cells[c]) != -1);

                if (!is_wall && !is_box) { all_blocked = 0; break; } // есть свободная клетка — не дедлок
                if (is_box && !IsGoal(goals, nb, cells[c]))
                    any_box_off_goal = 1; // ящик вне цели в этом квадрате
            }
            if (all_blocked && any_box_off_goal) return 1; // дедлок 2×2
        }
    }
    return 0;
}

/* ---------- NodePool — пул узлов A* ---------- */

/*
 * Все узлы хранятся в одном динамическом массиве (NodePool).
 * Узел адресуется целочисленным индексом — это безопаснее указателей,
 * так как realloc может переместить блок памяти.
 * Поле parent в AStarNode — тоже индекс, а не указатель.
 */

static NodePool *CreateNodePool(int cap)
{
    NodePool *p = (NodePool *)calloc(1, sizeof(NodePool));
    if (!p) return NULL;
    p->data = (AStarNode *)malloc(sizeof(AStarNode) * cap);
    if (!p->data) { free(p); return NULL; }
    p->capacity = cap;
    return p;
}

static void FreeNodePool(NodePool *p)
{
    if (!p) return;
    free(p->data);
    free(p);
}

/*
 * PoolAdd — добавляет узел в пул, при необходимости удваивая ёмкость.
 * Возвращает индекс нового узла или -1 при ошибке.
 *
 * ВАЖНО: после PoolAdd адрес pool->data может измениться из-за realloc.
 * Поэтому перед вызовом PoolAdd нужно скопировать все нужные данные
 * из pool->data на стек (как это делается в главном цикле).
 */
static int PoolAdd(NodePool *p, const AStarNode *node)
{
    if (p->count >= p->capacity)
    {
        if (p->capacity >= NODES_MAX_CAP) return -1;
        int new_cap = p->capacity * 2;
        if (new_cap > NODES_MAX_CAP) new_cap = NODES_MAX_CAP;
        AStarNode *tmp = (AStarNode *)realloc(p->data, sizeof(AStarNode) * new_cap);
        if (!tmp) return -1;
        p->data = tmp;
        p->capacity = new_cap;
    }
    p->data[p->count] = *node;
    return p->count++;
}

/* ---------- MinHeap — приоритетная очередь (open list) ---------- */

/*
 * Бинарная куча хранит индексы узлов из NodePool, упорядоченные по f.
 * HeapPop всегда возвращает узел с наименьшим f — следующий кандидат
 * для раскрытия в A*.
 */

static MinHeap *CreateHeap(int cap)
{
    MinHeap *h = (MinHeap *)calloc(1, sizeof(MinHeap));
    if (!h) return NULL;
    h->idx = (int *)malloc(sizeof(int) * cap);
    if (!h->idx) { free(h); return NULL; }
    h->capacity = cap;
    return h;
}

static void FreeHeap(MinHeap *h)
{
    if (!h) return;
    free(h->idx);
    free(h);
}

/*
 * HeapSiftUp — восстанавливает свойство кучи снизу вверх после вставки.
 * Новый элемент в позиции pos «всплывает» к корню, пока его f < f родителя.
 */
static void HeapSiftUp(MinHeap *h, const NodePool *pool, int pos)
{
    while (pos > 0)
    {
        int parent = (pos - 1) / 2;
        if (pool->data[h->idx[parent]].f <= pool->data[h->idx[pos]].f) break;
        int tmp = h->idx[parent];
        h->idx[parent] = h->idx[pos];
        h->idx[pos] = tmp;
        pos = parent;
    }
}

/*
 * HeapSiftDown — восстанавливает свойство кучи сверху вниз после извлечения
 * минимума. Корень заменяется последним элементом, затем «тонет» вниз.
 */
static void HeapSiftDown(MinHeap *h, const NodePool *pool, int pos)
{
    while (1)
    {
        int best = pos;
        int left = 2 * pos + 1;
        int right = 2 * pos + 2;
        if (left < h->size && pool->data[h->idx[left]].f < pool->data[h->idx[best]].f)
            best = left;
        if (right < h->size && pool->data[h->idx[right]].f < pool->data[h->idx[best]].f)
            best = right;
        if (best == pos) break;
        int tmp = h->idx[pos];
        h->idx[pos] = h->idx[best];
        h->idx[best] = tmp;
        pos = best;
    }
}

/* HeapPush — добавляет индекс узла в очередь и восстанавливает порядок. */
static int HeapPush(MinHeap *h, const NodePool *pool, int node_idx)
{
    if (h->size >= h->capacity)
    {
        int new_cap = h->capacity * 2;
        int *tmp = (int *)realloc(h->idx, sizeof(int) * new_cap);
        if (!tmp) return 0;
        h->idx = tmp;
        h->capacity = new_cap;
    }
    h->idx[h->size] = node_idx;
    HeapSiftUp(h, pool, h->size);
    h->size++;
    return 1;
}

/*
 * HeapPop — извлекает индекс узла с наименьшим f.
 * Корень (минимум) сохраняется, на его место ставится последний элемент,
 * затем выполняется sift-down.
 */
static int HeapPop(MinHeap *h, const NodePool *pool)
{
    if (h->size <= 0) return -1;
    int result = h->idx[0];
    h->size--;
    if (h->size > 0)
    {
        h->idx[0] = h->idx[h->size];
        HeapSiftDown(h, pool, 0);
    }
    return result;
}

/* ---------- HashSet — закрытый список посещённых состояний ---------- */

/*
 * Хеш-таблица с открытой адресацией (линейное зондирование).
 * Ёмкость всегда степень двойки, поэтому вместо деления используется
 * побитовое AND с mask = capacity - 1.
 *
 * Назначение: прежде чем добавлять новое состояние в open list,
 * проверяем, не было ли оно уже раскрыто. Если было — пропускаем.
 * Это предотвращает повторное раскрытие одного состояния и делает
 * алгоритм корректным (каждое состояние обрабатывается не более одного раза).
 */

/*
 * HashPacked — хеш FNV-1a для PackedState.
 * Смешиваем позицию игрока и все позиции ящиков.
 * FNV-1a обеспечивает хорошее распределение для небольших структур.
 */
static uint32_t HashPacked(const PackedState *ps, int nb)
{
    uint32_t h = 2166136261u; // FNV offset basis
    h ^= ps->player;
    h *= 16777619u;           // FNV prime
    for (int i = 0; i < nb; i++)
    {
        h ^= ps->boxes[i];
        h *= 16777619u;
    }
    return h;
}

/* PackedEqual — побайтовое сравнение двух состояний. */
static int PackedEqual(const PackedState *a, const PackedState *b, int nb)
{
    if (a->player != b->player) return 0;
    for (int i = 0; i < nb; i++)
        if (a->boxes[i] != b->boxes[i]) return 0;
    return 1;
}

static HashSet *CreateHashSet(int capacity)
{
    HashSet *hs = (HashSet *)calloc(1, sizeof(HashSet));
    if (!hs) return NULL;
    hs->states = (PackedState *)malloc(sizeof(PackedState) * capacity);
    hs->occupied = (char *)calloc(capacity, sizeof(char));
    hs->capacity = capacity;
    hs->mask = capacity - 1;
    if (!hs->states || !hs->occupied)
    {
        free(hs->states);
        free(hs->occupied);
        free(hs);
        return NULL;
    }
    return hs;
}

static void FreeHashSet(HashSet *hs)
{
    if (!hs) return;
    free(hs->states);
    free(hs->occupied);
    free(hs);
}

/*
 * HashSetGrow — удваивает ёмкость таблицы и перехеширует все записи.
 * Вызывается автоматически при заполнении >50% (load factor 0.5),
 * чтобы сохранить скорость линейного зондирования.
 */
static int HashSetGrow(HashSet *hs, int nb)
{
    int new_cap = hs->capacity * 2;
    PackedState *new_states = (PackedState *)malloc(sizeof(PackedState) * new_cap);
    char *new_occ = (char *)calloc(new_cap, sizeof(char));
    if (!new_states || !new_occ)
    {
        free(new_states);
        free(new_occ);
        return 0;
    }
    int new_mask = new_cap - 1;
    // Перенос всех существующих записей в новую таблицу
    for (int i = 0; i < hs->capacity; i++)
    {
        if (!hs->occupied[i]) continue;
        uint32_t idx = HashPacked(&hs->states[i], nb) & new_mask;
        while (new_occ[idx])
        {
            idx = (idx + 1) & new_mask; // линейное зондирование
        }
        new_states[idx] = hs->states[i];
        new_occ[idx] = 1;
    }

    free(hs->states);
    free(hs->occupied);
    hs->states = new_states;
    hs->occupied = new_occ;
    hs->capacity = new_cap;
    hs->mask = new_mask;
    return 1;
}

/*
 * HashSetInsert — пытается вставить состояние ps в таблицу.
 * Возвращает 1, если состояние новое (вставлено).
 * Возвращает 0, если состояние уже присутствует (дубликат).
 *
 * Линейное зондирование: если ячейка занята другим состоянием,
 * переходим к следующей по кругу (idx+1) & mask.
 */
static int HashSetInsert(HashSet *hs, const PackedState *ps, int nb)
{
    if (hs->count * 2 >= hs->capacity) // load factor > 0.5
    {
        if (!HashSetGrow(hs, nb)) return 0;
    }

    uint32_t idx = HashPacked(ps, nb) & hs->mask;
    while (hs->occupied[idx])
    {
        if (PackedEqual(&hs->states[idx], ps, nb)) return 0; // уже есть
        idx = (idx + 1) & hs->mask;
    }
    hs->states[idx] = *ps;
    hs->occupied[idx] = 1;
    hs->count++;
    return 1;
}

/* ---------- Главная функция: A* поиск решения ---------- */

/*
 * SolveLevel — запускает A* поиск от начального состояния уровня.
 *
 * Возвращает true и заполняет solver->moves последовательностью
 * направлений (индексы 0..3 = вверх/вниз/влево/вправо), если решение
 * найдено. Иначе возвращает false.
 *
 * Общая схема A*:
 *   1. Создать начальный узел, поместить в open list (MinHeap).
 *   2. Пока open list не пуст:
 *      a. Извлечь узел cur с наименьшим f = g + h.
 *      b. Если cur — целевое состояние (все ящики на целях) — путь найден.
 *      c. Для каждого из 4 направлений попробовать сделать шаг:
 *         - Если новая клетка — стена, пропустить.
 *         - Если новая клетка — ящик, попробовать толкнуть его дальше;
 *           если ящик упирается в стену или другой ящик — пропустить.
 *         - Если после толчка возник дедлок — пропустить.
 *         - Если новое состояние уже в closed list — пропустить.
 *         - Иначе создать дочерний узел и добавить в open list.
 *   3. Если путь найден, восстановить его, идя по цепочке parent.
 */
bool SolveLevel(const Level *level, Solver *solver)
{
    int nb = level->num_boxes;
    bool success = false;

    // Инициализация трёх структур данных
    NodePool *pool  = CreateNodePool(NODES_INIT_CAP);
    MinHeap  *open  = CreateHeap(HEAP_INIT_CAP);
    HashSet  *closed = CreateHashSet(HASH_INIT_CAP);

    if (!pool || !open || !closed)
        goto cleanup;

    // Кодируем цели в uint16_t и сортируем для сравнения с состояниями
    uint16_t goals[MAX_BOXES];
    for (int i = 0; i < nb; i++)
        goals[i] = (uint16_t)(level->goals[i].y * MAX_FIELD + level->goals[i].x);
    SortBoxes(goals, nb);

    // Создаём корневой узел (начальное состояние)
    {
        AStarNode root;
        root.state.player = (uint16_t)(level->player.y * MAX_FIELD + level->player.x);
        for (int i = 0; i < nb; i++)
            root.state.boxes[i] = (uint16_t)(level->boxes[i].y * MAX_FIELD + level->boxes[i].x);
        SortBoxes(root.state.boxes, nb); // нормализуем порядок ящиков
        root.parent = -1;      // корень не имеет родителя
        root.direction = -1;   // корень не имеет направления
        root.g = 0;            // стоимость пути от старта = 0
        root.f = Heuristic(root.state.boxes, goals, nb); // f = g + h = 0 + h

        int root_idx = PoolAdd(pool, &root);
        if (root_idx < 0) goto cleanup;
        HeapPush(open, pool, root_idx);
        HashSetInsert(closed, &root.state, nb); // сразу помечаем как посещённый
    }

    int found = -1;      // индекс найденного целевого узла (-1 = не найден)
    int iterations = 0;

    // Главный цикл A*
    while (open->size > 0 && iterations < MAX_ITERATIONS)
    {
        iterations++;

        // Извлекаем узел с наименьшим f из open list
        int cur_idx = HeapPop(open, pool);
        if (cur_idx < 0) break;

        /*
         * Копируем данные текущего узла на стек.
         * Это критически важно: PoolAdd внутри цикла может вызвать realloc,
         * после чего pool->data укажет на новый адрес, а старые указатели
         * на элементы массива станут невалидными. Работая с локальными
         * копиями cur_state и cur_g, мы избегаем use-after-realloc.
         */
        PackedState cur_state = pool->data[cur_idx].state;
        int cur_g = pool->data[cur_idx].g;

        // Проверка победы: все ящики совпадают с отсортированными целями
        int won = 1;
        for (int i = 0; i < nb; i++)
            if (cur_state.boxes[i] != goals[i]) { won = 0; break; }
        if (won) { found = cur_idx; break; }

        // Текущая позиция игрока
        int px = cur_state.player % MAX_FIELD;
        int py = cur_state.player / MAX_FIELD;

        // Раскрытие узла: пробуем все 4 направления
        for (int d = 0; d < 4; d++)
        {
            int nx = px + SDX[d]; // куда шагает игрок
            int ny = py + SDY[d];

            // Проверяем границы и стены
            if (nx < 0 || nx >= level->width || ny < 0 || ny >= level->height) continue;
            if (level->cells[ny][nx] == CELL_WALL) continue;

            uint16_t npos = (uint16_t)(ny * MAX_FIELD + nx); // новая позиция игрока

            // Копируем ящики из локальной копии состояния (а не из pool->data!)
            uint16_t new_boxes[MAX_BOXES];
            memcpy(new_boxes, cur_state.boxes, sizeof(uint16_t) * nb);

            int box_idx = IsBoxAt(new_boxes, nb, npos);

            if (box_idx != -1) // на пути ящик — пытаемся его толкнуть
            {
                int bnx = nx + SDX[d]; // куда полетит ящик
                int bny = ny + SDY[d];
                if (bnx < 0 || bnx >= level->width || bny < 0 || bny >= level->height) continue;
                if (level->cells[bny][bnx] == CELL_WALL) continue; // ящик упёрся в стену
                uint16_t bpos = (uint16_t)(bny * MAX_FIELD + bnx);
                if (IsBoxAt(new_boxes, nb, bpos) != -1) continue; // ящик упёрся в другой ящик

                new_boxes[box_idx] = bpos; // перемещаем ящик
            }

            // Отсекаем дедлоки: если после толчка возник тупик — пропускаем
            if (box_idx != -1 && IsDeadState(level, new_boxes, nb, goals))
                continue;

            // Нормализуем порядок ящиков для однозначного представления состояния
            SortBoxes(new_boxes, nb);

            // Формируем новое состояние
            PackedState ns;
            ns.player = npos;
            memcpy(ns.boxes, new_boxes, sizeof(uint16_t) * nb);

            // Проверяем, посещали ли мы это состояние раньше
            if (!HashSetInsert(closed, &ns, nb))
                continue; // уже в closed list — пропускаем

            // Создаём дочерний узел: g увеличивается на 1 (один ход),
            // f = g + h(нового состояния)
            AStarNode child;
            child.state = ns;
            child.parent = cur_idx;  // ссылка на родителя для восстановления пути
            child.direction = d;     // направление, которым был сделан этот ход
            child.g = cur_g + 1;
            child.f = child.g + Heuristic(new_boxes, goals, nb);

            int child_idx = PoolAdd(pool, &child);
            if (child_idx < 0) goto done; // закончилась память

            if (!HeapPush(open, pool, child_idx))
                goto done;
        }
    }

done:
    if (found >= 0)
    {
        /*
         * Восстановление пути от финального узла до корня.
         *
         * Идём по цепочке parent-указателей, считаем длину пути,
         * затем заполняем массив moves в обратном порядке (от старта к финишу).
         */

        // Первый проход: считаем длину пути
        int path_len = 0;
        int idx = found;
        while (idx > 0) // корень имеет parent = -1, его не считаем
        {
            path_len++;
            idx = pool->data[idx].parent;
        }

        solver->moves = (int *)malloc(sizeof(int) * path_len);
        if (solver->moves)
        {
            solver->num_moves = path_len;
            solver->current_move = 0;
            solver->active = true;
            solver->timer = 0;

            // Второй проход: заполняем moves с конца к началу
            idx = found;
            for (int i = path_len - 1; i >= 0; i--)
            {
                solver->moves[i] = pool->data[idx].direction;
                idx = pool->data[idx].parent;
            }
            success = true;
        }
    }

    printf("[solver] iterations=%d  pool=%d  hash=%d  found=%s\n",
           iterations, pool->count, closed->count,
           found >= 0 ? "YES" : "NO");

cleanup:
    FreeNodePool(pool);
    FreeHeap(open);
    FreeHashSet(closed);
    return success;
}

/* FreeSolver — освобождает память, выделенную под массив ходов. */
void FreeSolver(Solver *solver)
{
    if (solver->moves)
    {
        free(solver->moves);
        solver->moves = NULL;
    }
    solver->num_moves = 0;
    solver->current_move = 0;
    solver->active = false;
}
