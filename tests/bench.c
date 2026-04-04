#include "../src/level.h"
#include "../src/solver.h"
#include "../src/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int n = 100;
    if (argc >= 2) n = atoi(argv[1]);

    srand((unsigned)time(NULL));

    FILE *f = fopen("../tests/bench_results.csv", "w");
    if (!f) { fprintf(stderr, "cannot open bench_results.csv\n"); return 1; }

    fprintf(f, "difficulty;num_boxes;gen_ms;solve_ms;solved\n");

    const char *diff_names[] = {"easy", "medium", "hard"};

    for (int d = 0; d < 3; d++)
    {
        for (int i = 0; i < n; i++)
        {
            struct timespec t0, t1;

            clock_gettime(CLOCK_MONOTONIC, &t0);
            Level level = GenerateLevel((Difficulty)d);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double gen_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                            (t1.tv_nsec - t0.tv_nsec) / 1e6;

            Solver solver = {0};
            clock_gettime(CLOCK_MONOTONIC, &t0);
            int solved = SolveLevel(&level, &solver);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double solve_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                              (t1.tv_nsec - t0.tv_nsec) / 1e6;

            fprintf(f, "%s;%d;%.2f;%.2f;%d\n",
                    diff_names[d], level.num_boxes, gen_ms, solve_ms, solved);
            fflush(f);

            if (solved) FreeSolver(&solver);
            FreeUndoStack(&level);

            printf("\r[%s] %d/%d  ", diff_names[d], i + 1, n);
            fflush(stdout);
        }
        printf("\n");
    }

    fclose(f);
    printf("Done -> bench_results.csv\n");
    return 0;
}
