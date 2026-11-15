#ifndef WORKBENCH_HISTORY_H
#define WORKBENCH_HISTORY_H
#include "workbench.h"
#define BENCH_HISTORY_LIMIT 24
typedef struct {
    Workbench undo[BENCH_HISTORY_LIMIT], redo[BENCH_HISTORY_LIMIT];
    int undo_count, redo_count;
} BenchHistory;
void bench_history_clear(BenchHistory *history);
int bench_history_record(BenchHistory *history, const Workbench *before);
int bench_history_undo(BenchHistory *history, Workbench *bench);
int bench_history_redo(BenchHistory *history, Workbench *bench);
#endif
