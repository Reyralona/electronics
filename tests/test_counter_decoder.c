#include "spice_live.h"
#include "workbench_file.h"
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static Workbench bench;
static SimAutoResult result;
static unsigned display_mask(int part)
{
    unsigned mask = 0;
    for (int segment = 0; segment < 7; ++segment)
        if (result.elements[part][segment] > 1e-6)
            mask |= 1u << segment;
    return mask;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    WorkbenchSettings settings;
    assert(workbench_load_project(argv[1], &bench, &settings));
    int displays[2], count = 0, clock = -1;
    for (int i = 0; i < bench.part_count; ++i)
    {
        if (bench.parts[i].type == PART_SEVEN_SEGMENT)
        {
            assert(count < 2);
            displays[count++] = i;
        }
        if (bench.parts[i].type == PART_CLOCK)
            clock = i;
    }
    assert(count == 2 && clock >= 0 && bench.parts[clock].value == 50);
    /* Pre-optimization fingerprint of the supplied circuit; not a truth-table certification of its custom decoders. */
    const unsigned expected[][2] = {
        {0x5B, 0x3F},
        {0x4F, 0x3F},
        {0x66, 0x3F},
        {0x6D, 0x3F},
        {0x7D, 0x3F},
        {0x07, 0x3F},
        {0x6F, 0x17},
        {0x6F, 0x3F},
        {0x3F, 0x06},
        {0x06, 0x06},
        {0x5B, 0x06},
        {0x4F, 0x06},
        {0x66, 0x06},
        {0x6D, 0x06},
        {0x3F, 0x3F},
        {0x06, 0x3F},
        {0x5B, 0x3F},
        {0x4F, 0x3F},
        {0x66, 0x3F},
        {0x6D, 0x3F},
        {0x7D, 0x3F},
    };
    const double span = .0205;
    for (int instrument = 0; instrument < 2; ++instrument)
    {
        SpiceLive *live = spice_live_create();
        assert(live);
        SimAutoOptions options = {0};
        options.step = sim_auto_live_max_step(&bench);
        options.stop = .005;
        options.instrument_wires = instrument;
        options.limits = burn_limits_default();
        LARGE_INTEGER frequency, started, ended;
        assert(QueryPerformanceFrequency(&frequency));
        for (int sample = 0; sample < 21; ++sample)
        {
            if (sample == 1)
                assert(QueryPerformanceCounter(&started));
            int ok = spice_live_step(live, &bench, argv[2], &options, span, &result);
            if (!ok)
                fprintf(stderr, "%s\n", result.message);
            assert(ok && result.burnt_count == 0);
            unsigned units = display_mask(displays[0]), tens = display_mask(displays[1]);
            if (units != expected[sample][0] || tens != expected[sample][1])
                fprintf(stderr, "sample=%d instrument=%d t=%.9g masks=%02X/%02X\n", sample, instrument,
                        spice_live_time(live), units, tens);
            assert(units == expected[sample][0] && tens == expected[sample][1]);
        }
        assert(QueryPerformanceCounter(&ended));
        double elapsed = (double)(ended.QuadPart - started.QuadPart) / frequency.QuadPart;
        printf("Counter/adder/decoders: 21 display checks passed, instrument=%d, steady %.3fx (%d parts/%d wires).\n",
               instrument, 20 * span / elapsed, bench.part_count, bench.wire_count);
        spice_live_destroy(live);
    }
    bench_snapshot_clear(&bench);
    return 0;
}
