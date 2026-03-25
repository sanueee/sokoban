#ifndef SOLVER_H
#define SOLVER_H

#include "types.h"

bool SolveLevel(const Level *level, Solver *solver);
void FreeSolver(Solver *solver);

#endif