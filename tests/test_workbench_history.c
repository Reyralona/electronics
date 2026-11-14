#include <assert.h>
#include <stdlib.h>
#include "workbench_history.h"
int main(void)
{
    BenchHistory *h=calloc(1,sizeof(*h)); Workbench b={0}; int i;
    assert(h);
    bench_history_record(h,&b); assert(bench_add(&b,PART_RESISTOR,10,20)==0);
    bench_history_record(h,&b); assert(bench_set_value(&b,0,2200));
    assert(bench_history_undo(h,&b) && b.parts[0].value==1000);
    assert(bench_history_undo(h,&b) && b.part_count==0);
    assert(bench_history_redo(h,&b) && b.part_count==1);
    bench_history_record(h,&b); b.parts[0].x=50;
    assert(!bench_history_redo(h,&b));
    bench_history_record(h,&b); assert(bench_add(&b,PART_SOURCE,0,0)==1);
    bench_history_record(h,&b); assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
    bench_history_record(h,&b); b.wires[0].knee_count=1; b.wires[0].knees[0]=(WirePoint){25,30};
    bench_history_record(h,&b); bench_remove(&b,1);
    assert(b.part_count==1 && b.wire_count==0);
    assert(bench_history_undo(h,&b) && b.part_count==2 && b.wire_count==1 && b.wires[0].knees[0].x==25);
    assert(bench_history_undo(h,&b) && b.wires[0].knee_count==0);
    assert(bench_history_redo(h,&b) && b.wires[0].knee_count==1);
    for(i=0;i<BENCH_HISTORY_LIMIT+2;++i) { bench_history_record(h,&b); b.parts[0].x++; }
    assert(h->undo_count==BENCH_HISTORY_LIMIT);
    free(h); return 0;
}
