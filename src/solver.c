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
static void SortBoxes(CellPos *boxes, int n)
{
    for (int i = 1; i < n; i++)
    {
        CellPos key = boxes[i];
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
static int IsBoxAt(const CellPos *boxes, int n, CellPos pos)
{
    for (int i = 0; i < n; i++)
        if (boxes[i] == pos) return i;
    return -1;
}

/*
 * IsGoal — проверяет, является ли позиция pos одной из целевых клеток.
 */
static int IsGoal(const CellPos *goals, int n, CellPos pos)
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
static int Heuristic(const CellPos *boxes, const CellPos *goals, int n)
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
static int IsDeadState(const Level *level, const CellPos *boxes, int nb, const CellPos *goals)
{
    for (int i = 0; i < nb; i++)
    {
        if (IsGoal(goals, nb, boxes[i])) continue; // ящик уже на цели — пропускаем

        int x = boxes[i] % MAX_FIELD;
        int y = boxes[i] / MAX_FIELD;

        // 1) угловой дедлок: две смежные стены вокруг ящика
        int w_up    = (level->cells[y - 1][x] == CELL_WALL);
        int w_down  = (level->cells[y + 1][x] == CELL_WALL);
        int w_left  = (level->cells[y][x - 1] == CELL_WALL);
        int w_right = (level->cells[y][x + 1] == CELL_WALL);

        if ((w_up && w_left) || (w_up && w_right) ||
            (w_down && w_left) || (w_down && w_right))
            return 1;

        // 2) дедлок 2×2: проверяем все четыре квадрата, в которых участвует
        //    текущий ящик (ящик может быть в любом из четырёх углов квадрата)
        int offsets[4][2] = {{0,0}, {-1,0}, {0,-1}, {-1,-1}};
        for (int q = 0; q < 4; q++)
        {
            int bx = x + offsets[q][0]; // левый верхний угол квадрата
            int by = y + offsets[q][1];
            if (bx < 0 || bx + 1 >= level->width || by < 0 || by + 1 >= level->height)
                continue;

            // четыре клетки квадрата
            CellPos cells[4] = {
                (CellPos)(by       * MAX_FIELD + bx),
                (CellPos)(by       * MAX_FIELD + bx + 1),
                (CellPos)((by + 1) * MAX_FIELD + bx),
                (CellPos)((by + 1) * MAX_FIELD + bx + 1)
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
    // перенос всех существующих записей в новую таблицу
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

/*
 * ComputeReachable — обход поля в ширину (BFS) от клетки start_pos.
 *
 * Зачем эта функция вызывается в двух режимах:
 *   1) reachable != NULL (при раскрытии узла): заполняет карту достижимости,
 *      чтобы за O(1) понимать, с каких сторон игрок может подойти к ящикам.
 *   2) reachable == NULL (при создании дочернего узла): карта не нужна,
 *      нужен только возвращаемый минимум — каноническая позиция игрока.
 *
 * Значения в массиве vis / reachable:
 *   0 — клетка ещё не посещена (или недостижима игроком)
 *   1 — клетка достижима игроком
 *   2 — в клетке стоит ящик (препятствие для ходьбы и маркер для O(1)-проверки)
 *
 * Возвращает минимальный индекс клетки (CellPos), куда может дойти игрок.
 * Все позиции игрока внутри одной связной области дают один и тот же минимум,
 * поэтому дублирующие состояния ходьбы схлопываются в HashSet в одно.
 */
static CellPos ComputeReachable(const Level *level, const CellPos *boxes, int nb,
                                CellPos start_pos, uint8_t *reachable)
{
    // временный буфер на стеке используется, если вызывающий передал reachable == NULL
    uint8_t local_vis[MAX_FIELD * MAX_FIELD];
    uint8_t *vis = reachable ? reachable : local_vis;
    memset(vis, 0, sizeof(uint8_t) * MAX_FIELD * MAX_FIELD);

    // помечаем все ящики двойкой заранее: так и BFS в них не пойдёт,
    // и в SolveLevel можно за O(1) проверить, свободна ли клетка push_to от ящиков
    for (int i = 0; i < nb; i++)
        vis[boxes[i]] = 2;

    // head — откуда читаем, tail — куда дописываем
    CellPos queue[MAX_FIELD * MAX_FIELD];
    int head = 0, tail = 0;

    // стартуем
    vis[start_pos] = 1;
    queue[tail++] = start_pos;
    CellPos min_pos = start_pos;

    // пока в очереди есть непосещённые соседи — расширяем достижимую область
    while (head < tail)
    {
        CellPos cur = queue[head++];
        int cx = cur % MAX_FIELD;
        int cy = cur / MAX_FIELD;

        // пробуем шагнуть в 4 стороны
        for (int d = 0; d < 4; d++)
        {
            int nx = cx + SDX[d];
            int ny = cy + SDY[d];
            if (nx < 0 || nx >= level->width || ny < 0 || ny >= level->height) continue;
            if (level->cells[ny][nx] == CELL_WALL) continue;

            CellPos npos = (CellPos)(ny * MAX_FIELD + nx);
            // пропускаем, если уже посещали (1) или там стоит ящик (2)
            if (vis[npos]) continue;

            vis[npos] = 1;
            if (npos < min_pos) min_pos = npos; // обновляем канонический минимум
            queue[tail++] = npos;               // добавляем клетку в хвост очереди
        }
    }
    return min_pos;
}

/*
 * FindWalkPath — поиск кратчайшего маршрута ходьбы игрока от клетки from до to
 * без перемещения ящиков
 *
 * Записывает направления шагов (0..3) в буфер out_dirs (в прямом порядке: от from к to)
 * и возвращает количество шагов (или -1, если путь не найден).
 */
static int FindWalkPath(const Level *level, const CellPos *boxes, int nb,
                        CellPos from, CellPos to, int *out_dirs)
{
    // игрок уже стоит на нужной клетке
    if (from == to) return 0;

    uint8_t vis[MAX_FIELD * MAX_FIELD];       // 1 — клетка занята ящиком или уже посещена
    CellPos parent_pos[MAX_FIELD * MAX_FIELD];// из какой клетки пришли в данную (для отката пути)
    int8_t parent_dir[MAX_FIELD * MAX_FIELD]; // каким направлением (0..3) пришли в данную клетку
    CellPos queue[MAX_FIELD * MAX_FIELD];     // очередь BFS

    memset(vis, 0, sizeof(vis));
    // помечаем текущие ящики как непроходимые препятствия
    for (int i = 0; i < nb; i++)
        vis[boxes[i]] = 1;

    int head = 0, tail = 0;
    vis[from] = 1;
    queue[tail++] = from;

    // флаг: дошли ли мы до целевой клетки to
    int reached = 0;
    while (head < tail)
    {
        CellPos cur = queue[head++];
        int cx = cur % MAX_FIELD;
        int cy = cur / MAX_FIELD;

        for (int d = 0; d < 4; d++)
        {
            int nx = cx + SDX[d];
            int ny = cy + SDY[d];
            if (nx < 0 || nx >= level->width || ny < 0 || ny >= level->height) continue;
            if (level->cells[ny][nx] == CELL_WALL) continue;

            CellPos npos = (CellPos)(ny * MAX_FIELD + nx);
            if (vis[npos]) continue;

            vis[npos] = 1;
            parent_pos[npos] = cur;      // запоминаем предка, чтобы потом восстановить маршрут
            parent_dir[npos] = (int8_t)d;// запоминаем направление шага
            if (npos == to) { reached = 1; break; } // цель достигнута — дальше искать не нужно
            queue[tail++] = npos;
        }
        if (reached) break;
    }
    if (!reached) return -1;

    // восстанавливаем маршрут с конца (от to к from)
    int len = 0;
    CellPos p = to;
    while (p != from)
    {
        out_dirs[len++] = parent_dir[p];
        p = parent_pos[p];
    }

    // разворачиваем массив шагов, чтобы они шли в прямом порядке (от from к to)
    for (int i = 0; i < len / 2; i++)
    {
        int tmp = out_dirs[i];
        out_dirs[i] = out_dirs[len - 1 - i];
        out_dirs[len - 1 - i] = tmp;
    }
    return len;
}

/*
 * SolveLevel — запускает Push-based A* поиск с канонизацией игрока.
 *
 * Главная идея:
 *   Один переход (ребро графа) = один толчок ящика.
 *   Позиция игрока в PackedState хранится в каноническом виде — как минимальный
 *   индекс клетки в связной области игрока. Благодаря этому все шаги пустой ходьбы
 *   по комнате схлопываются в одно состояние.
 *
 * Возвращает true и заполняет solver->moves полной последовательностью
 * направлений (ходьба + толчки), если решение найдено. Иначе false.
 */
bool SolveLevel(const Level *level, Solver *solver)
{
    int nb = level->num_boxes;
    bool success = false;

    NodePool *pool   = CreateNodePool(NODES_INIT_CAP); // хранилище всех созданных узлов
    MinHeap  *open   = CreateHeap(HEAP_INIT_CAP);      // открытый список (приоритет по min f)
    HashSet  *closed = CreateHashSet(HASH_INIT_CAP);   // закрытый список (отсечка дубликатов)

    if (!pool || !open || !closed)
        goto cleanup;

    // кодируем целевые клетки в CellPos и сортируем
    CellPos goals[MAX_BOXES];
    for (int i = 0; i < nb; i++)
        goals[i] = (CellPos)(level->goals[i].y * MAX_FIELD + level->goals[i].x);
    SortBoxes(goals, nb);

    {
        AStarNode root;
        for (int i = 0; i < nb; i++)
            root.state.boxes[i] = (CellPos)(level->boxes[i].y * MAX_FIELD + level->boxes[i].x);
        SortBoxes(root.state.boxes, nb);

        CellPos start_player = (CellPos)(level->player.y * MAX_FIELD + level->player.x);
        root.state.player = ComputeReachable(level, root.state.boxes, nb, start_player, NULL);
        root.parent = -1;      // у корня нет родителя
        root.direction = -1;   // к корню не вёл никакой толчок
        root.push_box = 0;
        root.g = 0;            // сделано 0 толчков
        root.f = Heuristic(root.state.boxes, goals, nb); // f = 0 + h

        int root_idx = PoolAdd(pool, &root);
        if (root_idx < 0) goto cleanup;
        HeapPush(open, pool, root_idx);
        HashSetInsert(closed, &root.state, nb);
    }

    int found = -1;
    int iterations = 0;

    // главный цикл Push-based A*
    while (open->size > 0 && iterations < MAX_ITERATIONS)
    {
        iterations++;

        // извлекаем самый перспективный узел (с минимальным f = g + h)
        int cur_idx = HeapPop(open, pool);
        if (cur_idx < 0) break;

        PackedState cur_state = pool->data[cur_idx].state;
        int cur_g = pool->data[cur_idx].g;

        // проверка победы
        int won = 1;
        for (int i = 0; i < nb; i++)
            if (cur_state.boxes[i] != goals[i]) { won = 0; break; }
        if (won) { found = cur_idx; break; }

        // один BFS на весь узел: отмечаем достижимые клетки игрока (1) и ящики (2)
        uint8_t reachable[MAX_FIELD * MAX_FIELD];
        ComputeReachable(level, cur_state.boxes, nb, cur_state.player, reachable);

        // перебираем все возможные толчки: каждый ящик bi и каждое из 4 направлений d
        for (int bi = 0; bi < nb; bi++)
        {
            CellPos box_pos = cur_state.boxes[bi]; // текущая клетка ящика до толчка
            int bx = box_pos % MAX_FIELD;
            int by = box_pos / MAX_FIELD;

            for (int d = 0; d < 4; d++)
            {
                int fx = bx - SDX[d]; // push_from: откуда толкает игрок
                int fy = by - SDY[d];
                int tx = bx + SDX[d]; // push_to: куда сдвинется ящик
                int ty = by + SDY[d];

                if (fx < 0 || fx >= level->width || fy < 0 || fy >= level->height) continue;
                if (tx < 0 || tx >= level->width || ty < 0 || ty >= level->height) continue;

                // может ли игрок дойти до клетки за ящиком?
                CellPos from_pos = (CellPos)(fy * MAX_FIELD + fx);
                if (reachable[from_pos] != 1) continue;

                // свободна ли клетка перед ящиком от стен и других ящиков?
                if (level->cells[ty][tx] == CELL_WALL) continue;
                CellPos to_pos = (CellPos)(ty * MAX_FIELD + tx);
                if (reachable[to_pos] == 2) continue;

                // формируем новую расстановку ящиков с передвинутым ящиком bi
                CellPos new_boxes[MAX_BOXES];
                memcpy(new_boxes, cur_state.boxes, sizeof(CellPos) * nb);
                new_boxes[bi] = to_pos;

                if (IsDeadState(level, new_boxes, nb, goals))
                    continue;

                // сортируем ящики, чтобы порядок в массиве не зависел от номера ящика
                SortBoxes(new_boxes, nb);

                // после толчка игрок оказывается на старой клетке ящика (box_pos).
                // находим канонический минимум новой области достижимости игрока.
                PackedState ns;
                ns.player = ComputeReachable(level, new_boxes, nb, box_pos, NULL);
                memcpy(ns.boxes, new_boxes, sizeof(CellPos) * nb);

                // если такая расстановка ящиков + область игрока уже были — пропускаем
                if (!HashSetInsert(closed, &ns, nb))
                    continue;

                // собираем дочерний узел: запоминаем box_pos и направление d для восстановления пути
                AStarNode child;
                child.state = ns;
                child.parent = cur_idx;
                child.direction = d;
                child.push_box = box_pos;
                child.g = cur_g + 1; // +1 толчок
                child.f = child.g + Heuristic(new_boxes, goals, nb);

                int child_idx = PoolAdd(pool, &child);
                if (child_idx < 0) goto done; // кончилась память пула

                if (!HeapPush(open, pool, child_idx))
                    goto done;
            }
        }
    }

done:
    if (found >= 0)
    {
        /*
         * Восстановление полного маршрута для игры (solver->moves).
         * Поэтому сначала выписываем цепочку толчков от старта к победе, а затем
         * проигрываем уровень с самого начала и перед каждым толчком дописываем
         * короткий путь ходьбы (FindWalkPath).
         */

        // шаг 1: считаем длину цепочки толчков, поднимаясь от found к корню
        int num_pushes = 0;
        int idx = found;
        while (idx > 0)
        {
            num_pushes++;
            idx = pool->data[idx].parent;
        }

        // временные массивы для толчков в прямом порядке (от первого к последнему)
        CellPos *push_boxes = (CellPos *)malloc(sizeof(CellPos) * (num_pushes > 0 ? num_pushes : 1));
        int *push_dirs      = (int *)malloc(sizeof(int) * (num_pushes > 0 ? num_pushes : 1));

        if (push_boxes && push_dirs)
        {
            // заполняем массивы с конца к началу, идя по parent-ссылкам от цели к корню
            idx = found;
            for (int i = num_pushes - 1; i >= 0; i--)
            {
                push_boxes[i] = pool->data[idx].push_box;
                push_dirs[i]  = pool->data[idx].direction;
                idx = pool->data[idx].parent;
            }

            // шаг 2: виртуальная симуляция уровня от исходного положения игрока и ящиков
            CellPos sim_player = (CellPos)(level->player.y * MAX_FIELD + level->player.x);
            CellPos sim_boxes[MAX_BOXES];
            for (int i = 0; i < nb; i++)
                sim_boxes[i] = (CellPos)(level->boxes[i].y * MAX_FIELD + level->boxes[i].x);

            // итоговый буфер ходов (динамический, т.к. заранее неизвестно, сколько шагов ходьбы добавится)
            int cap = num_pushes * 8 + 16;
            int *moves = (int *)malloc(sizeof(int) * cap);
            int total_moves = 0;                 // сколько шагов уже записано в moves
            int walk_dirs[MAX_FIELD * MAX_FIELD];// буфер для одного участка ходьбы от FindWalkPath
            int ok = (moves != NULL);            // флаг успешности всех аллокаций и поисков пути

            // проходим по каждому толчку p в прямом порядке
            for (int p = 0; p < num_pushes && ok; p++)
            {
                CellPos bpos = push_boxes[p]; // где стоит ящик перед p-м толчком
                int dir = push_dirs[p];       // в какую сторону его нужно толкнуть
                int bx = bpos % MAX_FIELD;
                int by = bpos / MAX_FIELD;

                // push_from — куда игрок должен подойти, push_to — куда встанет ящик после толчка
                CellPos push_from = (CellPos)((by - SDY[dir]) * MAX_FIELD + (bx - SDX[dir]));
                CellPos push_to   = (CellPos)((by + SDY[dir]) * MAX_FIELD + (bx + SDX[dir]));

                // ищем кратчайший путь ходьбы от текущей позиции sim_player до push_from
                int wlen = FindWalkPath(level, sim_boxes, nb, sim_player, push_from, walk_dirs);
                if (wlen < 0) { ok = 0; break; }

                // при нехватке места в итоговом массиве moves — расширяем буфер (×2)
                if (total_moves + wlen + 1 > cap)
                {
                    int new_cap = cap * 2;
                    while (new_cap < total_moves + wlen + 1) new_cap *= 2;
                    int *tmp = (int *)realloc(moves, sizeof(int) * new_cap);
                    if (!tmp) { ok = 0; break; }
                    moves = tmp;
                    cap = new_cap;
                }

                // дописываем сначала шаги подхода к ящику, затем сам шаг толчка
                for (int w = 0; w < wlen; w++)
                    moves[total_moves++] = walk_dirs[w];
                moves[total_moves++] = dir;

                // обновляем состояние виртуальной симуляции: ящик уехал в push_to, игрок встал в bpos
                int bi = IsBoxAt(sim_boxes, nb, bpos);
                if (bi < 0) { ok = 0; break; }
                sim_boxes[bi] = push_to;
                sim_player = bpos;
            }

            if (ok)
            {
                solver->moves = moves;
                solver->num_moves = total_moves;
                solver->current_move = 0;
                solver->active = true;
                solver->timer = 0;
                success = true;
            }
            else
            {
                free(moves);
            }
        }

        free(push_boxes);
        free(push_dirs);
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
