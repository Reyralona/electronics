#include "workbench_file.h"
#include "spice_live.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    Workbench bench = {0};
    assert(workbench_load(argv[1], &bench));
    printf("slot: %d parts, %d wires\n", bench.part_count, bench.wire_count);
    for (int instrument = 0; instrument <= 1; ++instrument)
    {
        SpiceLive *live = spice_live_create();
        assert(live);
        SimAutoOptions options = {0};
        options.instrument_wires = instrument;
        options.step = 1e-5;
        options.stop = .005;
        options.limits = burn_limits_default();
        SimAutoResult result;
        ULONGLONG start = GetTickCount64();
        for (int frame = 0; frame < 6; ++frame)
        {
            int ok = spice_live_step(live, &bench, argv[2], &options, .005 / 300, &result);
            if (!ok)
                fprintf(stderr, "%s\n", result.message);
            assert(ok);
        }
        printf("instrument=%d elapsed=%llu ms time=%.6g s\n", instrument,
               (unsigned long long)(GetTickCount64() - start), spice_live_time(live));
        spice_live_destroy(live);
    }
    bench_snapshot_clear(&bench);
}
