#include "workbench.h"
#include "workbench_file.h"
#include "workbench_history.h"
#include "workbench_selection.h"
#include "spice_process.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char **argv)
{
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
