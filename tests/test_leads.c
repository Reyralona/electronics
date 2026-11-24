#include "workbench.h"
#include "workbench_file.h"
#include "workbench_history.h"
#include "workbench_selection.h"
#include "spice_process.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check_render_pick(void *context, WirePoint point, size_t ignored)
{
    (void)ignored;
    const Part *part = context;
    WirePoint projected;
    size_t slot;
    assert(bench_lead_pick(part, 0, 18, point, .001f, &slot, &projected));
    assert(slot < bench_lead_count(part, 0));
    assert(hypotf(projected.x - point.x, projected.y - point.y) < .002f);
}

int main(int argc, char **argv)
{
    static Workbench fitting;
    assert(bench_add(&fitting, PART_BREADBOARD, 650, 400) == 0);
    assert(bench_add(&fitting, PART_RESISTOR, 650, 385) == 1);
    Pin initial_contact;
    assert(bench_find_contact(&fitting, 1, 0, &initial_contact));
    assert(bench_set_scale(&fitting, 1, .4f));
    assert(bench_find_contact(&fitting, 1, 0, &initial_contact));
    assert(bench_snap_to_breadboard(&fitting, 1));
    for (int pin = 0; pin < 2; ++pin)
    {
        Pin contact;
        assert(bench_find_contact(&fitting, 1, pin, &contact) && contact.component == 0);
        float x, y, after_x, after_y;
        bench_pin_position(&fitting.parts[1], pin, &x, &y);
        assert(bench_set_scale(&fitting, 1, .6f));
        bench_pin_position(&fitting.parts[1], pin, &after_x, &after_y);
        assert(hypotf(x - after_x, y - after_y) < .001f);
    }
    WirePoint loose_tip = bench_lead_point(&fitting.parts[1], 0, 0);
    loose_tip.x += 3;
    assert(bench_lead_move(&fitting, 1, 0, 0, loose_tip));
    assert(!bench_find_contact(&fitting, 1, 0, &initial_contact));
    assert(bench_snap_to_breadboard(&fitting, 1));
    assert(bench_find_contact(&fitting, 1, 0, &initial_contact) && initial_contact.component == 0);
    bench_snapshot_clear(&fitting);
    assert(bench_add(&fitting, PART_BREADBOARD, 650, 400) == 0);
    assert(bench_add(&fitting, PART_DIP_SWITCH, 650, 400) == 1);
    assert(bench_set_scale(&fitting, 1, .5f));
    for (int pin = 0; pin < 8; ++pin)
        assert(bench_find_contact(&fitting, 1, pin, &initial_contact) && initial_contact.component == 0);
    bench_snapshot_clear(&fitting);
    assert(bench_add(&fitting, PART_BREADBOARD_WIDE, 650, 400) == 0);
    assert(bench_pin_count(PART_BREADBOARD_WIDE) == 840);
    assert(bench_board_group_first(720) == 300 && bench_board_group_first(839) == 390);
    assert(bench_board_group_first(569) != bench_board_group_first(570));
    WorkbenchSettings fit_settings = workbench_settings_default();
    assert(workbench_save_project("build/test-wide-board.elec", &fitting, &fit_settings));
    assert(workbench_load_project("build/test-wide-board.elec", &fitting, &fit_settings));
    assert(fitting.parts[0].type == PART_BREADBOARD_WIDE);
    bench_snapshot_clear(&fitting);
    Workbench bench = {0}, saved = {0}, loaded = {0};
    static BenchHistory storage;
    BenchHistory *history = &storage;
    assert(history);
    bench_add(&bench, PART_SOURCE, 300, 300);
    bench_add(&bench, PART_RESISTOR, 500, 300);
    bench_add(&bench, PART_LED, 700, 300);
    WirePoint positive, negative, resistor_end;
    bench_pin_position(&bench.parts[0], 0, &positive.x, &positive.y);
    bench_pin_position(&bench.parts[0], 1, &negative.x, &negative.y);
    bench_pin_position(&bench.parts[1], 1, &resistor_end.x, &resistor_end.y);
    assert(bench_lead_move(&bench, 1, 0, 0, positive));
    assert(bench_lead_move(&bench, 2, 0, 0, resistor_end));
    assert(bench_lead_move(&bench, 2, 1, 0, negative));
    assert(bench_snapshot_clone(&saved, &bench));
    assert(bench_history_record(history, &bench));
    for (int i = 0; i < 40; ++i)
        assert(bench_lead_insert(&bench, 2, 0, (size_t)i, (WirePoint){600, 310 + i}));
    assert(bench_lead_count(&bench.parts[2], 0) == 41);
    bench_lead_walk(&bench.parts[2], 0, 18, check_render_pick, &bench.parts[2]);
    assert(bench_lead_count(&saved.parts[2], 0) == 1);
    assert(bench_history_undo(history, &bench));
    assert(bench_lead_count(&bench.parts[2], 0) == 1);
    assert(bench_history_redo(history, &bench));
    assert(bench_lead_count(&bench.parts[2], 0) == 41);
    assert(bench_lead_remove(&bench, 2, 0, 1));
    assert(!bench_lead_remove(&bench, 2, 0, 39));
    Pin contact;
    assert(bench_find_contact(&bench, 2, 0, &contact));
    assert(contact.component == 1 && contact.pin == 1);
    assert(!bench_lead_move(&bench, 2, 0, 0, (WirePoint){NAN, 0}));
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(bench_lead_count(&loaded.parts[2], 0) == 40);
    assert(bench_find_contact(&loaded, 2, 0, &contact) && contact.component == 1);
    if (argc > 1)
    {
        double current, currents[WORKBENCH_MAX_COMPONENTS];
        char message[256];
        assert(spice_dc_analyze(&loaded, argv[1], &current, currents, message, sizeof(message)));
        assert(currents[2] > .002 && currents[2] < .004);
        WirePoint end = bench_lead_point(&loaded.parts[2], 0, 39);
        end.y += 5;
        assert(bench_lead_move(&loaded, 2, 0, 39, end));
        assert(spice_dc_analyze(&loaded, argv[1], &current, currents, message, sizeof(message)));
        assert(fabs(currents[2]) < 1e-8);
    }
    BenchClipboard clipboard = {0};
    BenchSelection selection = {0};
    selection.parts[2] = 1;
    assert(bench_selection_copy(&bench, &selection, &clipboard));
    assert(bench_selection_paste(&bench, &selection, &clipboard, 900, 500));
    assert(bench_lead_count(&bench.parts[3], 0) == 40);
    assert(bench_lead_move(&bench, 2, 0, 0, (WirePoint){400, 600}));
    assert(bench_lead_point(&bench.parts[3], 0, 0).y != 600);
    bench_snapshot_clear(&clipboard.bench);
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&saved);
    bench_snapshot_clear(&bench);
    bench_history_clear(history);
    return 0;
}
