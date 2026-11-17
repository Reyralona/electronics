#include "workbench_pack.h"
#include "workbench_file.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    Workbench bench = {0}, loaded = {0};
    BenchSelection all = {0};
    char message[256];
    bench_add(&bench, PART_AND, 400, 200);
    bench_add(&bench, PART_AND, 600, 400);
    bench_add(&bench, PART_JUNCTION, 200, 300);
    assert(bench_set_port_name(&bench, (Pin){2, 0}, "CLOCK"));
    assert(bench_toggle_wire(&bench, (Pin){2, 0}, (Pin){0, 0}));
    assert(bench_toggle_wire(&bench, (Pin){2, 0}, (Pin){1, 0}));
    for (int i = 0; i < bench.part_count; ++i)
        all.parts[i] = 1;
    int group = bench_pack_selection(&bench, &all, message, sizeof(message));
    assert(group && bench_group_ports(&bench, group) == 1);
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    assert(bench_group_ports(&loaded, group) == 1);
    fclose(file);
    bench_snapshot_clear(&bench);
    bench_snapshot_clear(&loaded);
    puts("Manual CLOCK port shared by two gates: passed");
}
