#include "workbench_pack.h"
#include "workbench_file.h"
#include "component_registry.h"
#include "netlist.h"
#include "spice_process.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>

/* Place every terminal of every catalog device on a switch terminal, then separate them.
 * Both discovery directions and the actual solver node map must agree. */
static void every_catalog_terminal(void)
{
    for (size_t type = 0; type < component_registry_count(); ++type)
    {
        const ComponentInfo *info = component_info((PartType)type);
        if (!info)
            continue;
        Workbench bench = {0};
        assert(bench_add(&bench, info->type, 500, 300) == 0);
        assert(bench_add(&bench, PART_SWITCH, 2000, 2000) == 1);
        for (int pin = 0; pin < info->pin_count; ++pin)
        {
            float x, y, sx, sy;
            bench_terminal_position(&bench, (Pin){0, pin}, &x, &y);
            bench_pin_position(&bench.parts[1], 0, &sx, &sy);
            assert(bench_move_part(&bench, 1, bench.parts[1].x + x - sx, bench.parts[1].y + y - sy));
            Pin contact;
            assert(bench_find_contact(&bench, 0, pin, &contact) && contact.component == 1);
            assert(bench_find_contact(&bench, 1, 0, &contact) && contact.component == 0);
            assert(netlist_same_net(&bench, (Pin){0, pin}, (Pin){1, 0}));
            assert(bench_move_part(&bench, 1, bench.parts[1].x + 3, bench.parts[1].y + 3));
            assert(!bench_find_contact(&bench, 1, 0, &contact));
            assert(!netlist_same_net(&bench, (Pin){0, pin}, (Pin){1, 0}));
        }
        bench_snapshot_clear(&bench);
    }
}

static void chip_ports_and_persistence(void)
{
    Workbench bench = {0};
    BenchSelection selection = {0};
    char message[256];
    assert(bench_add(&bench, PART_SWITCH, 500, 300) == 0);
    selection.parts[0] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_add(&bench, PART_SWITCH, 2000, 2000) == 1);
    for (int turn = 0; turn < 4; ++turn)
    {
        for (int pin = 0; pin < 2; ++pin)
        {
            float x, y, sx, sy;
            bench_terminal_position(&bench, (Pin){0, pin}, &x, &y);
            bench_pin_position(&bench.parts[1], 0, &sx, &sy);
            assert(bench_move_part(&bench, 1, bench.parts[1].x + x - sx, bench.parts[1].y + y - sy));
            assert(netlist_same_net(&bench, (Pin){0, pin}, (Pin){1, 0}));
            Workbench loaded = {0};
            FILE *file = tmpfile();
            assert(file && workbench_write(file, &bench));
            rewind(file);
            assert(workbench_read(file, &loaded));
            fclose(file);
            assert(loaded.groups[0].rotation == turn);
            assert(netlist_same_net(&loaded, (Pin){0, pin}, (Pin){1, 0}));
            bench_snapshot_clear(&loaded);
            assert(bench_move_part(&bench, 1, 2000, 2000));
        }
        assert(bench_rotate_part(&bench, 0));
        assert(bench.parts[0].rotation == 0);
    }
    assert(bench.groups[0].rotation == 0);
    assert(bench_group_width(&bench, 1) == 98);
    assert(bench_group_resize(&bench, 1, 76, bench_group_height(&bench, 1)));
    assert(bench_group_width(&bench, 1) == 76);
    assert(bench_rotate_part_45(&bench, 0));
    assert(bench.groups[0].rotation_diagonal == 1);
    {
        Workbench loaded = {0};
        FILE *file = tmpfile();
        assert(file && workbench_write(file, &bench));
        rewind(file);
        assert(workbench_read(file, &loaded));
        fclose(file);
        assert(loaded.groups[0].rotation_diagonal == 1);
        assert(bench_group_width(&loaded, 1) == 76);
        bench_snapshot_clear(&loaded);
    }
    /* Hidden internal geometry cannot become an external contact. */
    float x, y, sx, sy;
    bench_pin_position(&bench.parts[0], 0, &x, &y);
    bench_pin_position(&bench.parts[1], 0, &sx, &sy);
    assert(bench_move_part(&bench, 1, bench.parts[1].x + x - sx, bench.parts[1].y + y - sy));
    assert(!netlist_same_net(&bench, (Pin){0, 0}, (Pin){1, 0}));
    bench_snapshot_clear(&bench);
}

static void diagonal_part_roundtrip(void)
{
    Workbench bench = {0}, loaded = {0};
    assert(bench_add(&bench, PART_RESISTOR, 500, 300) == 0);
    assert(bench_rotate_part_45(&bench, 0));
    float x, y;
    bench_pin_position(&bench.parts[0], 0, &x, &y);
    assert(fabsf(x - (500 - 49 * .70710678f)) < .001f);
    assert(fabsf(y - (300 - 49 * .70710678f)) < .001f);
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.parts[0].rotation_diagonal == 1);
    bench_pin_position(&loaded.parts[0], 0, &x, &y);
    assert(fabsf(x - (500 - 49 * .70710678f)) < .001f);
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&bench);
}

static void chip_to_chip(void)
{
    Workbench bench = {0};
    char message[256];
    assert(bench_add(&bench, PART_SWITCH, 500, 300) == 0);
    BenchSelection selection = {0};
    selection.parts[0] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_add(&bench, PART_SWITCH, 900, 500) == 1);
    selection = (BenchSelection){0};
    selection.parts[1] = 1;
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    for (int turn = 0; turn < 4; ++turn)
    {
        float x, y, sx, sy;
        bench_terminal_position(&bench, (Pin){0, 0}, &x, &y);
        bench_terminal_position(&bench, (Pin){1, 1}, &sx, &sy);
        bench_selection_move(&bench, &selection, x - sx, y - sy);
        assert(netlist_same_net(&bench, (Pin){0, 0}, (Pin){1, 1}));
        bench_selection_move(&bench, &selection, 3, 3);
        assert(!netlist_same_net(&bench, (Pin){0, 0}, (Pin){1, 1}));
        assert(bench_rotate_part(&bench, 0));
    }
    bench_snapshot_clear(&bench);
}

static void switched_chip_circuit(const char *solver)
{
    if (!solver)
        return;
    Workbench bench = {0};
    const PartType types[] = {PART_SOURCE, PART_SWITCH, PART_SWITCH, PART_RESISTOR, PART_LED};
    for (int i = 0; i < 5; ++i)
        assert(bench_add(&bench, types[i], 100 + 300 * i, 300) == i);
    assert(bench_set_on(&bench, 1, 1) && bench_set_on(&bench, 2, 1));
    BenchSelection selection = {0};
    selection.parts[2] = 1;
    char message[256];
    assert(bench_pack_selection(&bench, &selection, message, sizeof(message)));
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){3, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){3, 1}, (Pin){4, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){4, 1}, (Pin){0, 1}) == 1);
    for (int rotation = 0; rotation < 4; ++rotation)
    {
        float x, y, sx, sy;
        bench_terminal_position(&bench, (Pin){2, 0}, &x, &y);
        bench_terminal_position(&bench, (Pin){1, 1}, &sx, &sy);
        assert(bench_move_part(&bench, 1, bench.parts[1].x + x - sx, bench.parts[1].y + y - sy));
        double current, values[WORKBENCH_MAX_COMPONENTS];
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(values[4] > .001);
        assert(bench_set_on(&bench, 1, 0));
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(fabs(values[4]) < 1e-6);
        assert(bench_set_on(&bench, 1, 1));
        assert(bench_set_on(&bench, 2, 0));
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(fabs(values[4]) < 1e-6);
        assert(bench_set_on(&bench, 2, 1));
        assert(bench_move_part(&bench, 1, bench.parts[1].x + 3, bench.parts[1].y + 3));
        assert(spice_dc_analyze(&bench, solver, &current, values, message, sizeof(message)));
        assert(fabs(values[4]) < 1e-6);
        assert(bench_rotate_part(&bench, 2));
    }
    bench_snapshot_clear(&bench);
}

/* "Ligo um fio direto em outro fio": splitting a bent wire keeps both halves, its color and the electrical node;
 * the junction survives save/reopen and disappears once no wire reaches it. */
static void wire_junctions(void)
{
    Workbench bench = {0}, reopened = {0};
    WorkbenchSettings settings = workbench_settings_default();
    assert(bench_add(&bench, PART_SOURCE, 300, 300) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 700, 300) == 1);
    assert(bench_add(&bench, PART_RESISTOR, 500, 500) == 2);
    assert(bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    const WirePoint knees[2] = {{400, 200}, {600, 200}};
    assert(bench_set_wire_route(&bench, 0, knees, 2));
    assert(bench_set_wire_color(&bench, 0, 2));
    int junction = bench_split_wire(&bench, 0, 1, (WirePoint){500, 200});
    assert(junction == 3 && bench.parts[junction].type == PART_JUNCTION && bench.wire_count == 2);
    assert(bench.wires[0].knee_count == 1 && bench.wires[1].knee_count == 1);
    assert(bench.wires[0].color == 2 && bench.wires[1].color == 2);
    assert(netlist_same_net(&bench, (Pin){0, 0}, (Pin){1, 0}));
    assert(bench_toggle_wire(&bench, (Pin){2, 0}, (Pin){junction, 0}) == 1);
    assert(netlist_same_net(&bench, (Pin){2, 0}, (Pin){1, 0}));
    assert(!netlist_same_net(&bench, (Pin){2, 1}, (Pin){1, 0}));
    /* Opposite cases: invalid slot or wire refused; a junction still wired is kept. */
    assert(bench_split_wire(&bench, 0, 5, (WirePoint){0, 0}) < 0 && bench_split_wire(&bench, 99, 0, (WirePoint){0, 0}) < 0);
    assert(bench_prune_junctions(&bench) == 0);

    char temporary_directory[MAX_PATH];
    char temporary_file[MAX_PATH];
    DWORD directory_length = GetTempPathA(sizeof(temporary_directory), temporary_directory);
    assert(directory_length > 0 && directory_length < sizeof(temporary_directory));
    assert(GetTempFileNameA(temporary_directory, "bjt", 0, temporary_file));
    assert(DeleteFileA(temporary_file));
    assert(workbench_save_project(temporary_file, &bench, &settings));
    assert(workbench_load_project(temporary_file, &reopened, &settings));
    assert(DeleteFileA(temporary_file));
    assert(reopened.parts[junction].type == PART_JUNCTION && reopened.wire_count == 3);
    assert(netlist_same_net(&reopened, (Pin){2, 0}, (Pin){0, 0}));

    for (int i = bench.wire_count - 1; i >= 0; --i)
        bench_remove_wire(&bench, i);
    assert(bench_prune_junctions(&bench) == 1 && bench.part_count == 3);
    bench_snapshot_clear(&reopened);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    wire_junctions();
    every_catalog_terminal();
    chip_ports_and_persistence();
    diagonal_part_roundtrip();
    chip_to_chip();
    switched_chip_circuit(argc > 1 ? argv[1] : NULL);
    puts("Every catalog terminal, chip rotation, direct contacts and save/reopen passed.");
    return 0;
}
