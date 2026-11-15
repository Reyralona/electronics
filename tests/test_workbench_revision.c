#include "workbench.h"
#include "workbench_file.h"
#include "workbench_history.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

static void test_part_edits(void)
{
    Workbench bench = {0};
    assert(bench_add(&bench, PART_SOURCE, 100, 200) == 0);
    assert(bench.revision == 1);
    assert(bench_set_value(&bench, 0, 5));
    assert(bench_set_on(&bench, 0, 1));
    assert(bench_move_part(&bench, 0, 100, 200));
    assert(bench.revision == 1);
    assert(bench_set_value(&bench, 0, 6));
    assert(bench.revision == 2);
    assert(bench_set_on(&bench, 0, 0));
    assert(bench.revision == 3);
    assert(bench_move_part(&bench, 0, 110, 200));
    assert(bench.revision == 4);
    assert(bench_rotate_part(&bench, 0));
    assert(bench.revision == 5);
    assert(!bench_move_part(&bench, 0, NAN, 200));
    assert(!bench_set_value(&bench, 0, -1));
    assert(!bench_set_on(&bench, 0, 2));
    bench_remove(&bench, -1);
    assert(bench.revision == 5);
    bench_remove(&bench, 0);
    assert(bench.revision == 6);
}

static void test_wire_edits(void)
{
    Workbench bench = {0};
    WirePoint route[] = {{100, 50}, {200, 50}};
    assert(bench_add(&bench, PART_SOURCE, 100, 200) == 0);
    assert(bench_add(&bench, PART_LED, 300, 200) == 1);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench.revision == 3);
    assert(bench_set_wire_route(&bench, 0, route, 2));
    assert(bench.revision == 4);
    assert(bench_set_wire_route(&bench, 0, route, 2));
    assert(bench.revision == 4);
    assert(bench_set_wire_color(&bench, 0, 3));
    assert(bench.revision == 5);
    assert(bench_move_knee(&bench, 0, 1, (WirePoint){210, 60}));
    assert(bench.revision == 6);
    assert(bench_remove_knee(&bench, 0, 0));
    assert(bench.revision == 7 && bench.wires[0].knees[0].x == 210);
    assert(!bench_set_wire_route(&bench, 0, NULL, 1));
    assert(!bench_set_wire_route(&bench, 0, (WirePoint[]){{NAN, 0}}, 1));
    assert(!bench_set_wire_color(&bench, 0, WIRE_COLOR_COUNT));
    assert(!bench_move_knee(&bench, 0, -1, route[0]));
    assert(!bench_remove_knee(&bench, 0, 10));
    assert(bench.revision == 7 && bench.wires[0].knee_count == 1);
    assert(bench_set_wire_route(&bench, 0, NULL, 0));
    assert(bench.revision == 8);
    assert(bench_remove_wire(&bench, 0));
    assert(bench.revision == 9);
    assert(!bench_remove_wire(&bench, 0));
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 2);
    assert(bench.revision == 11);
}

static void test_replacement_and_history(void)
{
    Workbench bench = {0}, other = {0};
    BenchHistory *history = calloc(1, sizeof(*history));
    assert(history);
    assert(bench_add(&bench, PART_SOURCE, 100, 200) == 0);
    bench_history_record(history, &bench);
    assert(bench_set_value(&bench, 0, 6));
    uint64_t edited = bench.revision;
    assert(bench_history_undo(history, &bench));
    assert(bench.parts[0].value == 5 && bench.revision > edited);
    uint64_t undone = bench.revision;
    assert(bench_history_redo(history, &bench));
    assert(bench.parts[0].value == 6 && bench.revision > undone);
    uint64_t redone = bench.revision;
    assert(bench_add(&other, PART_LED, 300, 200) == 0);
    bench_replace(&bench, &other);
    assert(bench.parts[0].type == PART_LED && bench.revision > redone);
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &other));
    rewind(file);
    uint64_t replaced = bench.revision;
    assert(workbench_read(file, &bench));
    assert(bench.revision > replaced);
    fclose(file);
    free(history);
}

static void test_snap(void)
{
    Workbench bench = {0};
    assert(bench_add(&bench, PART_BREADBOARD, 500, 400) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 486, 345) == 1);
    uint64_t before = bench.revision;
    assert(bench_snap_to_breadboard(&bench, 1));
    assert(bench.revision == before);
    assert(bench_move_part(&bench, 1, 487, 345));
    assert(bench_snap_to_breadboard(&bench, 1));
    assert(bench.revision == before + 2 && bench.parts[1].x == 486);
}

int main(void)
{
    test_part_edits();
    test_wire_edits();
    test_replacement_and_history();
    test_snap();
    return 0;
}
