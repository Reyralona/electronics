#include "workbench_selection.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    BenchClipboard clipboard = {0};
    assert(bench_add(&bench, PART_SOURCE, 300, 200) == 0);
    assert(bench_add(&bench, PART_LED, 500, 200) == 1);
    assert(bench_add(&bench, PART_RESISTOR, 700, 200) == 2);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){2, 0}) == 1);
    assert(bench_set_wire_route(&bench, 0, (WirePoint[]){{400, 150}}, 1));
    bench_selection_rect(&selection, &bench, (WirePoint){550, 250}, (WirePoint){250, 150});
    assert(bench_selection_count(&selection, &bench) == 2);
    assert(bench_selection_copy(&bench, &selection, &clipboard));
    assert(clipboard.bench.part_count == 2 && clipboard.bench.wire_count == 1);
    assert(bench_selection_paste(&bench, &selection, &clipboard, 900, 400));
    assert(bench.part_count == 5 && bench.wire_count == 3);
    assert(bench.wires[2].a.component == 3 && bench.wires[2].b.component == 4);
    assert(bench.parts[3].x == 900 && bench.parts[4].x == 1100);
    assert(bench.wires[2].knees[0].x == 1000 && bench.wires[2].knees[0].y == 350);
    bench_selection_move(&bench, &selection, 20, 30);
    assert(bench.parts[3].x == 920 && bench.wires[2].knees[0].x == 1020);
    assert(bench.parts[0].x == 300 && bench.wires[0].knees[0].x == 400);
    bench_selection_delete(&bench, &selection);
    assert(bench.part_count == 3 && bench.wire_count == 2);
    assert(bench_selection_count(&selection, &bench) == 0);
    while (bench.part_count < WORKBENCH_MAX_COMPONENTS)
        assert(bench_add(&bench, PART_RESISTOR, 800, 500) >= 0);
    Workbench before = bench;
    assert(!bench_selection_paste(&bench, &selection, &clipboard, 900, 400));
    assert(memcmp(&before, &bench, sizeof(bench)) == 0);
    assert(!bench_selection_copy(&bench, &selection, &clipboard));
    assert(clipboard.valid && clipboard.bench.part_count == 2);
    return 0;
}
