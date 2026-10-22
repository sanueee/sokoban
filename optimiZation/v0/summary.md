# v0 — профиль исходной версии

Условия: `conditions.txt`. Код: коммит `acbeb73` + переименование `open`/`close` → `db_open`/`db_close`
(без него игра не запускается на Linux; на генератор и решатель не влияет).

## Файлы

| Файл | Что это |
|---|---|
| `time.csv` | этап 1: время, `(ulimit -v 4000000; ./sokoban_bench 20)`, снял пользователь |
| `cg_easy_medium.out` | этап 2: callgrind, `./sokoban_bench 3`, дамп после easy+medium (`callgrind_control --dump`), hard не профилировался |
| `cg_easy_medium.log` | вывод решателя в этом прогоне (какие уровни попали) |
| `annotate_self.txt`, `annotate_inclusive.txt` | `callgrind_annotate` self / `--inclusive=yes` |
| `annotate_lines.txt` | разбивка по строкам исходника (`--auto=yes`) |
| `massif_easy_medium.out/.txt` | этап 4: massif, снимок после easy+medium (`vgdb all_snapshots`), `ms_print` |

Почему без hard: исходный `bench` всегда гоняет все три сложности, а hard под valgrind идёт часами
и упирается в память. Профиль сохранялся в момент, когда закончился medium, и процесс останавливался.

## Время (time.csv)

| | Уникальных уровней ≈ | Генерация, мс | Решение, мс | Решено |
|---|---|---|---|---|
| easy | 1 | 0,7–1,6 | 2,5–4,8 | 20/20 |
| medium | 2 | 51–60 | 0,25–8,6 | 20/20 |
| hard | ~14 | 3–2439 | 64–27 555 | 18/20 |

- Из-за `srand(time(NULL))` в `GenerateLevel` уровни внутри одной секунды одинаковые.
- 2 нерешённых hard (~26–27 с) — нехватка памяти под `ulimit` 4 ГБ, а не нерешаемость.

## callgrind: всего 946 534 662 Ir (6 уровней easy+medium)

**Inclusive (с вызванными):**

| Функция | Ir | % |
|---|---|---|
| `GenerateLevel` | 600 328 711 | **63,4** |
| ↳ `rand` (6 756 772 вызова) | 317 350 319 | **33,5** |
| ↳ `CarveMaze` (679 вызовов) | 64 554 369 | 6,8 |
| `SolveLevel` | 344 886 795 | **36,4** |
| ↳ `Heuristic` (150 855 вызовов) | 120 298 812 | **12,7** |
| ↳ `HashSetInsert` (308 312 вызовов) | 30 738 135 | 3,3 |
| ↳ `SortBoxes` (308 318 вызовов) | 25 692 524 | 2,7 |
| ↳ `memcpy` | 7 807 024 | 0,8 |
| ↳ `HeapPush` | 6 144 032 | 0,7 |
| ↳ `calloc`/`malloc` (по 24 вызова) | ~300 000 | 0,03 |

**Self (собственное):** `GenerateLevel` 29,2 % · `random_r` 18,7 % · `SolveLevel` 16,3 % ·
`Heuristic` 12,7 % · `random` 11,5 % · `HashSetInsert` 3,3 % · `rand` 2,9 % · `SortBoxes` 2,7 %.

**Встроено компилятором (-O2), отдельно не видно:**
- в `GenerateLevel`: `ReverseSolve`, `FloodFillWithBoxes`, `HasBox`, `PlaceGoalsAndBoxes`, `HasDeadlock`, `IsOnGoal`, `GenerateMaze`;
- в `SolveLevel`: `IsDeadState`, `IsBoxAt`, `IsGoal`, `HeapPop`/`HeapSiftDown`, `PoolAdd`, `HashPacked`.

Их цена — в разбивке по строкам (`annotate_lines.txt`). Самые дорогие строки:

| % | Где | Строка |
|---|---|---|
| 2×16,7 | level.c, `ReverseSolve` | два `rand()`: выбор ящика и направления |
| 4,3 + 3,6 | level.c, `ReverseSolve` | проверки границ `pull_*` / `back_*` |
| 2,5 | level.c, `FloodFillWithBoxes` | `visited \|\| WALL \|\| HasBox` |
| 2,5 | level.c, `ReverseSolve` | `if (!visited[pull_y][pull_x])` |
| 4,9 + 3,5 + 2,4 | solver.c, `Heuristic` | `abs()+abs()`, `% MAX_FIELD`, внутренний цикл |
| 1,8 + 1,1 | solver.c, `HeapSiftDown` | `pool->data[h->idx[…]].f` |

## massif: пик 43,99 МБ

Пик достигается на **первом же** вызове решателя, даже на уровне в 853 итерации:
`NodePool` 20 000 000 Б (500 000 × 40) + `MinHeap` 2 000 000 Б + `HashSet` ~22 000 000 Б.
То есть ~44 МБ выделяется на каждый вызов независимо от размера уровня.

## Выводы для обсуждения

1. **Гипотеза «тормозит только решатель» неверна** для easy/medium: 63 % — генерация.
2. Генерация: ~97 перегенераций лабиринта на уровень (679 `CarveMaze` на ~7 уровней) и
   **6,7 млн вызовов `rand`** — почти всё в цикле попыток `ReverseSolve` (до 100 случайных попыток на ход).
3. Решатель: главный — `Heuristic` (~800 Ir на вызов: k² пар, `%` и `/`).
4. `calloc` 44 МБ по инструкциям почти бесплатен (0,03 %): ядро выдаёт нулевые страницы лениво.
   Время на это (page faults) callgrind не видит — он считает инструкции программы, не работу ядра.
5. На hard картина, скорее всего, другая (там решатель на миллионы узлов) — профиль hard нужен отдельно,
   на фиксированных уровнях.
