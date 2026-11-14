#include "workbench_history.h"
#include <string.h>

static void push(Workbench *stack,int *count,const Workbench *bench)
{
    if(*count==BENCH_HISTORY_LIMIT) {
        memmove(stack,stack+1,(BENCH_HISTORY_LIMIT-1)*sizeof(*stack));
        --*count;
    }
    stack[(*count)++]=*bench;
}
void bench_history_record(BenchHistory *history,const Workbench *before)
{
    push(history->undo,&history->undo_count,before);
    history->redo_count=0;
}
int bench_history_undo(BenchHistory *history,Workbench *bench)
{
    if(!history->undo_count) return 0;
    push(history->redo,&history->redo_count,bench);
    *bench=history->undo[--history->undo_count];
    return 1;
}
int bench_history_redo(BenchHistory *history,Workbench *bench)
{
    if(!history->redo_count) return 0;
    push(history->undo,&history->undo_count,bench);
    *bench=history->redo[--history->redo_count];
    return 1;
}
