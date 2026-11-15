#include "workbench_history.h"

static void clear_stack(Workbench *stack, int *count)
{
    while (*count)
        bench_snapshot_clear(&stack[--*count]);
}

void bench_history_clear(BenchHistory *history)
{
    if (!history)
        return;
    clear_stack(history->undo, &history->undo_count);
    clear_stack(history->redo, &history->redo_count);
}

static int push(Workbench *stack, int *count, const Workbench *bench)
{
    Workbench copy = {0};
    if (!bench_snapshot_clone(&copy, bench))
        return 0;
    if (*count == BENCH_HISTORY_LIMIT)
    {
        bench_snapshot_clear(&stack[0]);
        for (int i = 1; i < *count; ++i)
            bench_snapshot_move(&stack[i - 1], &stack[i]);
        --*count;
    }
    bench_snapshot_move(&stack[(*count)++], &copy);
    return 1;
}

int bench_history_record(BenchHistory *history, const Workbench *before)
{
    if (!history || !before || !push(history->undo, &history->undo_count, before))
        return 0;
    clear_stack(history->redo, &history->redo_count);
    return 1;
}

static int restore(Workbench *source, int *source_count, Workbench *opposite, int *opposite_count,
                   Workbench *bench)
{
    if (!*source_count)
        return 0;
    Workbench next = {0};
    if (!bench_snapshot_clone(&next, &source[*source_count - 1]))
        return 0;
    if (!push(opposite, opposite_count, bench))
    {
        bench_snapshot_clear(&next);
        return 0;
    }
    uint64_t revision = bench->revision + 1;
    bench_snapshot_move(bench, &next);
    bench->revision = revision;
    bench_snapshot_clear(&source[--*source_count]);
    return 1;
}

int bench_history_undo(BenchHistory *history, Workbench *bench)
{
    if (!history || !bench)
        return 0;
    return restore(history->undo, &history->undo_count, history->redo, &history->redo_count, bench);
}

int bench_history_redo(BenchHistory *history, Workbench *bench)
{
    if (!history || !bench)
        return 0;
    return restore(history->redo, &history->redo_count, history->undo, &history->undo_count, bench);
}
