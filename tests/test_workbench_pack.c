#include "workbench_pack.h"
#include "workbench_file.h"
#include "spice_process.h"
#include "workbench_examples.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

static void test_breadboard_boundary(const char *solver)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    int selected;
    char message[256];
    double current, values[WORKBENCH_MAX_COMPONENTS], reference = 0;
    assert(workbench_example_create(0, &bench, &selected));
    if (solver)
    {
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        reference = values[3];
    }
    selection.parts[2] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench.wire_count == 5 && bench_group_ports(&bench, 1) == 2);
    Pin contact;
    assert(!bench_find_contact(&bench, 2, 0, &contact));
    if (solver)
    {
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(fabs(values[3] - reference) < reference * .001);
    }
}

static void test_overlapping_instances(const char *solver)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    BenchClipboard clipboard = {0};
    int selected;
    char message[256];
    assert(workbench_example_create(0, &bench, &selected));
    for (int i = 0; i < bench.part_count; ++i)
        selection.parts[i] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_selection_copy(&bench, &selection, &clipboard));
    assert(bench_selection_paste(&bench, &selection, &clipboard, bench.parts[0].x, bench.parts[0].y));
    Pin contact;
    assert(bench_find_contact(&bench, 6, 0, &contact) && contact.component == 5);
    if (solver)
    {
        double current, values[WORKBENCH_MAX_COMPONENTS];
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(values[3] > .003 && fabs(values[7] - values[3]) < values[3] * .001);
    }
    Workbench before = bench;
    assert(!bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(memcmp(&before, &bench, sizeof(bench)) == 0);
}

static void test_invalid_project(void)
{
    const char *projects[] = {
        "workbench 16\n1 0\n4 500 200 0 1000 0 0\ngroups 1\ngroup 1 500 200\n"
        "members 1\nmember 0 1\nports 2\nport 0 0\nport 0 0\n",
        "workbench 16\n1 0\n4 500 200 0 1000 0 0\ngroups 1\ngroup 1 500 200\n"
        "members 0\nports 0\n",
        "workbench 16\n1 0\n4 500 200 0 1000 0 0\ngroups 1\ngroup 1 500 200\n"
        "members 1\nmember 0 1\nports 1\nport 0 2\n",
        "workbench 16\n2 1\n0 300 200 1 5 0 0\n4 500 200 0 1000 0 0\n0 0 1 0 0 0\n"
        "groups 1\ngroup 1 500 200\nmembers 1\nmember 1 1\nports 0\n",
    };
    Workbench bench = {0};
    assert(bench_add(&bench, PART_LED, 300, 200) == 0);
    Workbench before = bench;
    for (size_t i = 0; i < sizeof(projects) / sizeof(projects[0]); ++i)
    {
        FILE *file = tmpfile();
        assert(file);
        fputs(projects[i], file);
        fputs("settings 1 1 1 0 0 0 0 1e-5 .005\n", file);
        rewind(file);
        assert(!workbench_read(file, &bench));
        assert(memcmp(&before, &bench, sizeof(bench)) == 0);
        fclose(file);
    }
}

static void test_rc_trace(const char *solver)
{
    if (!solver)
        return;
    Workbench bench = {0};
    BenchSelection selection = {0};
    static SpiceTrace before, after;
    char message[256];
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, PART_RESISTOR, 500, 200);
    bench_add(&bench, PART_CAPACITOR, 700, 200);
    bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1});
    assert(spice_transient_analyze(&bench, solver, (Pin){2, 0}, 1e-5, .005, &before, message, sizeof(message)));
    selection.parts[1] = selection.parts[2] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(spice_transient_analyze(&bench, solver, (Pin){2, 0}, 1e-5, .005, &after, message, sizeof(message)));
    assert(before.count == after.count);
    for (int i = 0; i < before.count; ++i)
    {
        assert(before.samples[i].time == after.samples[i].time);
        assert(fabs(before.samples[i].value - after.samples[i].value) < 1e-8);
    }
}

static void test_boundary_capacity(void)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    int selected;
    char message[256];
    assert(workbench_example_create(0, &bench, &selected));
    for (int pin = 100; bench.wire_count < WORKBENCH_MAX_WIRES; ++pin)
        assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, pin}) == 1);
    selection.parts[2] = 1;
    Workbench before = bench;
    assert(!bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(memcmp(&before, &bench, sizeof(bench)) == 0 && strstr(message, "128"));
}

static int strip_partner(int hole)
{
    for (int other = 0; other < BREADBOARD_PINS; ++other)
        if (other != hole && bench_board_group_first(other) == bench_board_group_first(hole))
            return other;
    return -1;
}

/* Three AND gates make a 4-input AND: only the four free inputs and the final output become ports,
 * whether the inner links are direct wires or go through a breadboard strip outside the block. */
static void test_four_input_and(void)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    char message[256];
    int board = bench_add(&bench, PART_BREADBOARD, 600, 600);
    int first = bench_add(&bench, PART_AND, 300, 200), second = bench_add(&bench, PART_AND, 300, 320);
    int last = bench_add(&bench, PART_AND, 460, 260), load = bench_add(&bench, PART_RESISTOR, 700, 200);
    int hole = 40, partner = strip_partner(hole);
    assert(partner >= 0);
    assert(bench_toggle_wire(&bench, (Pin){first, 2}, (Pin){board, hole}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){board, partner}, (Pin){last, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){second, 2}, (Pin){last, 1}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){second, 2}, (Pin){load, 0}) == 1);
    selection.parts[first] = selection.parts[second] = selection.parts[last] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_group_ports(&bench, 1) == 6);
    assert(!bench_pin_exposed(&bench.parts[first], 2) && !bench_pin_exposed(&bench.parts[last], 0));
    assert(bench_pin_exposed(&bench.parts[second], 2) && !bench_pin_exposed(&bench.parts[last], 1));
    assert(bench_pin_exposed(&bench.parts[last], 2));
    for (int i = 0; i < bench.wire_count; ++i)
    {
        const Wire *wire = &bench.wires[i];
        assert(wire->a.component != board && wire->b.component != board);
        assert(!bench.parts[wire->a.component].group || bench_pin_exposed(&bench.parts[wire->a.component], wire->a.pin) ||
               bench_group_wire_internal(&bench, wire));
    }
    selection = (BenchSelection){0};
    bench_toggle_wire(&bench, (Pin){second, 2}, (Pin){load, 0});
    Workbench plain = {0};
    first = bench_add(&plain, PART_AND, 300, 200);
    second = bench_add(&plain, PART_AND, 300, 320);
    last = bench_add(&plain, PART_AND, 460, 260);
    assert(bench_toggle_wire(&plain, (Pin){first, 2}, (Pin){last, 0}) == 1);
    assert(bench_toggle_wire(&plain, (Pin){second, 2}, (Pin){last, 1}) == 1);
    selection.parts[first] = selection.parts[second] = selection.parts[last] = 1;
    assert(bench_pack_selection(&plain, &selection, message, sizeof(message)));
    assert(bench_group_ports(&plain, 1) == 5 && strstr(message, "5 terminais"));
    bench_snapshot_clear(&plain);
    bench_snapshot_clear(&bench);
}

static WirePoint terminal(const Workbench *bench, int part, int pin)
{
    WirePoint point;
    bench_terminal_position(bench, (Pin){part, pin}, &point.x, &point.y);
    return point;
}

/* "Quero um 3AND com as 3 entradas a esquerda e a saida a direita": two ANDs packed give 4 terminals,
 * automatically 2 left (A, B) and 2 right (C, Y). Moving C to the left leaves only Y on the right. */
static void test_three_input_and_layout(void)
{
    Workbench bench = {0}, reopened = {0};
    BenchSelection selection = {0};
    WorkbenchSettings settings = workbench_settings_default();
    char message[256];
    int first = bench_add(&bench, PART_AND, 300, 200), last = bench_add(&bench, PART_AND, 460, 240);
    assert(bench_toggle_wire(&bench, (Pin){first, 2}, (Pin){last, 0}) == 1);
    selection.parts[first] = selection.parts[last] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_group_ports(&bench, 1) == 4);
    const BenchGroup *chip = &bench.groups[0];
    assert(terminal(&bench, last, 1).x > chip->x); /* C starts on the right */
    float automatic_height = bench_group_height(&bench, 1);

    int c = bench_group_port_index(&bench, (Pin){last, 1});
    assert(c == 2);
    uint64_t revision = bench.revision;
    assert(bench_group_move_port(&bench, 1, c, BENCH_PORT_LEFT, 2));
    assert(bench.revision != revision);
    WirePoint a = terminal(&bench, first, 0), b = terminal(&bench, first, 1);
    WirePoint moved = terminal(&bench, last, 1), output = terminal(&bench, last, 2);
    assert(a.x < chip->x && b.x < chip->x && moved.x < chip->x);
    assert(a.y < b.y && b.y < moved.y);
    assert(output.x > chip->x && fabsf(output.y - chip->y) < .01f);
    assert(bench_group_height(&bench, 1) > automatic_height);

    /* Same place again changes nothing; invalid side or terminal is refused without changes. */
    revision = bench.revision;
    assert(bench_group_move_port(&bench, 1, c, BENCH_PORT_LEFT, 2));
    assert(bench.revision == revision);
    assert(!bench_group_move_port(&bench, 1, c, 2, 0));
    assert(!bench_group_move_port(&bench, 1, 4, BENCH_PORT_LEFT, 0));
    assert(bench.revision == revision);

    /* Save and reopen: the arrangement is part of the chip. */
    assert(workbench_save_project("build/test-chip-layout.elec", &bench, &settings));
    assert(workbench_load_project("build/test-chip-layout.elec", &reopened, &settings));
    remove("build/test-chip-layout.elec");
    WirePoint reopened_c = terminal(&reopened, last, 1);
    assert(fabsf(reopened_c.x - moved.x) < .01f && fabsf(reopened_c.y - moved.y) < .01f);

    /* A stale arrangement (different terminal count) falls back to the automatic one. */
    reopened.groups[0].port_layout = 3;
    assert(terminal(&reopened, last, 1).x > reopened.groups[0].x);
    bench_snapshot_clear(&reopened);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    test_three_input_and_layout();
    const char *solver = argc > 1 ? argv[1] : NULL;
    test_four_input_and();
    test_breadboard_boundary(solver);
    test_overlapping_instances(solver);
    test_invalid_project();
    test_boundary_capacity();
    test_rc_trace(solver);
    Workbench bench = {0};
    BenchSelection selection = {0};
    BenchClipboard clipboard = {0};
    char message[256];
    double current, values[WORKBENCH_MAX_COMPONENTS], before_current = 0;
    assert(bench_add(&bench, PART_SOURCE, 300, 200) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 500, 200) == 1);
    assert(bench_add(&bench, PART_LED, 700, 200) == 2);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){2, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1}) == 1);
    if (argc > 1)
    {
        assert(spice_dc_analyze(&bench, argv[1], &current, values, message, sizeof(message)));
        before_current = values[2];
    }
    selection.parts[1] = selection.parts[2] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)) == 1);
    assert(bench_group_ports(&bench, 1) == 2 && bench_group_wire_internal(&bench, &bench.wires[1]));
    assert(bench_pin_exposed(&bench.parts[1], 0) && !bench_pin_exposed(&bench.parts[1], 1));
    float x, y;
    bench_terminal_position(&bench, (Pin){1, 0}, &x, &y);
    assert(x == 540 && y == 200);
    if (argc > 1)
    {
        assert(spice_dc_analyze(&bench, argv[1], &current, values, message, sizeof(message)));
        assert(fabs(values[2] - before_current) < 1e-9);
    }
    assert(bench_selection_copy(&bench, &selection, &clipboard));
    assert(bench_selection_paste(&bench, &selection, &clipboard, 900, 400));
    assert(bench.parts[3].group == 2 && bench.parts[4].group == 2);
    assert(bench_group_ports(&bench, 2) == 2);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){3, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){4, 1}, (Pin){0, 1}) == 1);
    assert(bench_set_value(&bench, 3, 2000));
    assert(bench.parts[1].value == 1000);
    if (argc > 1)
    {
        assert(spice_dc_analyze(&bench, argv[1], &current, values, message, sizeof(message)));
        assert(fabs(values[2] - before_current) < before_current * .001 && values[4] < values[2] * .6);
    }
    assert(bench_group_resize(&bench, 2, 180, 120));
    assert(!bench_group_resize(&bench, 2, NAN, 120));
    assert(!bench_group_resize(&bench, 2, 10, 120));
    assert(bench_group_width(&bench, 2) == 180);
    assert(bench_group_height(&bench, 2) == 120);
    FILE *file = tmpfile();
    Workbench loaded = {0};
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.parts[3].group == 2 && bench_group_ports(&loaded, 2) == 2);
    assert(bench_group_width(&loaded, 2) == 180 && bench_group_height(&loaded, 2) == 120);
    assert(bench_unpack_selection(&loaded, &selection));
    assert(!loaded.parts[3].group && !loaded.groups[1].active);
    assert(loaded.parts[1].group == 1 && loaded.groups[0].active);
    Workbench before_move = bench;
    bench_selection_move(&bench, &selection, NAN, 0);
    assert(memcmp(&before_move, &bench, sizeof(bench)) == 0);
    bench_selection_delete(&bench, &selection);
    assert(bench.part_count == 3 && !bench.groups[1].active && bench.groups[0].active);
    return 0;
}
