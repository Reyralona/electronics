#ifndef WORKBENCH_SELECTION_H
#define WORKBENCH_SELECTION_H
#include "workbench.h"

typedef struct
{
    unsigned char parts[WORKBENCH_MAX_COMPONENTS];
} BenchSelection;

typedef struct
{
    Workbench bench;
    int valid;
} BenchClipboard;

void bench_selection_clear(BenchSelection *selection);
int bench_selection_count(const BenchSelection *selection, const Workbench *bench);
void bench_selection_rect(BenchSelection *selection, const Workbench *bench, WirePoint a, WirePoint b);
int bench_selection_copy(const Workbench *bench, const BenchSelection *selection, BenchClipboard *clipboard);
int bench_selection_paste(Workbench *bench, BenchSelection *selection, const BenchClipboard *clipboard, float x,
                          float y);
void bench_selection_delete(Workbench *bench, BenchSelection *selection);
void bench_selection_move(Workbench *bench, const BenchSelection *selection, float dx, float dy);
#endif
