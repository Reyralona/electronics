#include "workbench_pack.h"
#include "workbench_group_edit.h"
#include "workbench_group_name.h"
#include "workbench_file.h"
#include "spice_process.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Human flow: create a chip, compose it into two more chips, save/reopen,
 * enter each level, edit/apply, copy, ungroup one level, then delete.
 * Opposite: merely entering or discarding never changes the outer circuit. */
static void test_electrical_layers(const char *solver)
{
    Workbench bench = {0}, draft = {0};
    BenchSelection selection = {0};
    char message[256];
    assert(bench_add(&bench, PART_SOURCE, 100, 300) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 400, 300) == 1);
    assert(bench_add(&bench, PART_RESISTOR, 700, 300) == 2);
    selection.parts[1] = 1;
    int left = bench_pack_selection(&bench, &selection, message, sizeof(message));
    bench_selection_clear(&selection);
    selection.parts[2] = 1;
    int right = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(left && right);
    /* Separate packages whose hidden original parts occupy the same coordinates. */
    bench.groups[left - 1].x = 400;
    bench.groups[right - 1].x = 700;
    bench.parts[2].x = bench.parts[1].x;
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){2, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1}) == 1);
    selection.parts[1] = 1;
    int outer = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(outer && bench_group_ports(&bench, outer) == 2);
    Pin contact;
    assert(!bench_find_contact(&bench, 1, 0, &contact));
    if (solver)
    {
        double current, currents[WORKBENCH_MAX_COMPONENTS];
        assert(spice_dc_analyze(&bench, solver, &current, currents, message, sizeof(message)));
        assert(fabs(fabs(currents[1]) - .0025) < .000001);
        assert(fabs(fabs(currents[2]) - .0025) < .000001);
    }
    assert(bench_group_extract(&bench, outer, &draft));
    assert(draft.parts[0].group && draft.parts[1].group && draft.parts[0].group != draft.parts[1].group);
    bench_snapshot_clear(&draft);
    bench_snapshot_clear(&bench);
}

static void test_depth_and_invalid_parent(void)
{
    Workbench bench = {0}, loaded = {0};
    BenchSelection selection = {0};
    char message[256];
    assert(bench_add(&bench, PART_NOT, 400, 200) == 0);
    selection.parts[0] = 1;
    for (int level = 0; level < 20; ++level)
        assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench.parts[0].group == 20);
    bench.groups[0].parent = 1;
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(bench_add(&loaded, PART_LED, 100, 100) == 0);
    uint64_t revision = loaded.revision;
    assert(!workbench_read(file, &loaded));
    assert(loaded.part_count == 1 && loaded.parts[0].type == PART_LED && loaded.revision == revision);
    fclose(file);
    bench.groups[0].parent = 2;
    for (int level = 20; level > 0; --level)
    {
        assert(bench.parts[0].group == level);
        assert(bench_unpack_selection(&bench, &selection));
    }
    assert(!bench.parts[0].group && !bench.parts[0].layers);
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        assert(!bench.groups[g].active);
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&bench);
}

static void test_touching_children(void)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    char message[256];
    assert(bench_add(&bench, PART_RESISTOR, 400, 200) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 700, 200) == 1);
    selection.parts[0] = 1;
    int first = bench_pack_selection(&bench, &selection, message, sizeof(message));
    selection.parts[0] = 0;
    selection.parts[1] = 1;
    int second = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(first && second);
    bench.groups[second - 1].x = bench.groups[first - 1].x + 120;
    Pin contact;
    assert(bench_find_contact(&bench, 0, 1, &contact) && contact.component == 1 && contact.pin == 0);
    selection.parts[0] = 1;
    int outer = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(outer && bench.wire_count == 1 && bench_group_ports(&bench, outer) == 2);
    assert(bench_unpack_selection(&bench, &selection));
    assert(bench_find_contact(&bench, 0, 1, &contact));
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    test_electrical_layers(argc > 1 ? argv[1] : NULL);
    test_depth_and_invalid_parent();
    test_touching_children();
    Workbench bench = {0}, draft = {0}, inner = {0}, reopened = {0};
    BenchSelection selection = {0};
    char message[256];
    assert(bench_add(&bench, PART_AND, 400, 200) == 0);
    selection.parts[0] = 1;
    int first = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(first && bench_group_set_name(&bench, first, "AND interna"));
    assert(bench_set_port_name(&bench, (Pin){0, 0}, "A"));
    int second = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(second && second != first);
    assert(bench_group_set_name(&bench, second, "Segundo nivel"));
    int third = bench_pack_selection(&bench, &selection, message, sizeof(message));
    assert(third && third != second);
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &reopened));
    fclose(file);
    uint64_t revision = reopened.revision;
    assert(bench_group_extract(&reopened, third, &draft));
    assert(reopened.revision == revision && reopened.parts[0].group == third);
    assert(draft.parts[0].group == second);
    assert(!strcmp(draft.groups[second - 1].name, "Segundo nivel"));
    assert(bench_group_extract(&draft, second, &inner));
    assert(inner.parts[0].group == first);
    assert(!strcmp(inner.groups[first - 1].name, "AND interna"));
    char name[32];
    assert(bench_port_name(&inner.parts[0], 0, name, sizeof(name)) && !strcmp(name, "A"));
    assert(bench_group_set_name(&inner, first, "Renomeado"));
    assert(bench_group_apply(&draft, second, draft.revision, &inner, "Segundo nivel", message, sizeof(message)));
    assert(bench_group_apply(&reopened, third, reopened.revision, &draft, "Terceiro", message, sizeof(message)));
    BenchClipboard clipboard = {0};
    assert(bench_selection_copy(&reopened, &selection, &clipboard));
    assert(bench_selection_paste(&reopened, &selection, &clipboard, 900, 400));
    assert(reopened.part_count == 2);
    assert(bench_unpack_selection(&reopened, &selection));
    int copied_second = reopened.parts[1].group;
    assert(copied_second && copied_second != second && copied_second != third);
    assert(bench_group_extract(&reopened, copied_second, &draft));
    assert(!strcmp(draft.groups[draft.parts[0].group - 1].name, "Renomeado"));
    bench_selection_delete(&reopened, &selection);
    assert(reopened.part_count == 1 && reopened.parts[0].group == third);
    bench_snapshot_clear(&clipboard.bench);
    bench_snapshot_clear(&bench);
    bench_snapshot_clear(&draft);
    bench_snapshot_clear(&inner);
    bench_snapshot_clear(&reopened);
    puts("recursive chips: create, nest, save, reopen, inspect, apply, copy, ungroup, delete OK");
    return 0;
}
