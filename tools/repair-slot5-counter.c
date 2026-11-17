#include "workbench_file.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    Workbench bench = {0};
    WorkbenchSettings settings;
    assert(workbench_load_project("saves/slot-5.elec", &bench, &settings));
    const int wires[] = {53, 59, 65};
    const int junctions[] = {47, 52, 54};
    const int inverted_outputs[] = {10, 23, 34};
    const int clocks[] = {21, 32, 43};
    for (int i = 0; i < 3; ++i)
    {
        Wire *wire = &bench.wires[wires[i]];
        assert(wire->a.component == clocks[i] && wire->a.pin == 0);
        assert(wire->b.component == junctions[i] && wire->b.pin == 0);
        wire->b = (Pin){inverted_outputs[i], 0};
    }
    assert(workbench_save_project("saves/slot-5.elec", &bench, &settings));
    bench_snapshot_clear(&bench);
    puts("slot 5: next-stage clocks connected to Q_bar; wire routes and colors retained");
}
