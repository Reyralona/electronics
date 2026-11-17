#include "workbench_file.h"
#include "spice_live.h"
#include "netlist.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    Workbench bench = {0};
    WorkbenchSettings settings;
    assert(workbench_load_project(argv[1], &bench, &settings));
    assert(bench.part_count == 12 && bench.wire_count == 17);
    printf("loaded: %d parts, %d wires\n", bench.part_count, bench.wire_count);
    SpiceTrace trace = {0};
    char message[256];
    int ok = spice_transient_analyze(&bench, argv[2], (Pin){11, 0}, settings.transient_step,
                                     settings.transient_stop, &trace, message, sizeof(message));
    printf("scope trace: %d, %s, samples=%d\n", ok, message, trace.count);
    assert(ok && trace.count > 100);
    if (ok)
        printf("scope first=%.6g last=%.6g\n", trace.samples[0].value, trace.samples[trace.count - 1].value);
    SpiceTrace meter_trace = {0};
    assert(spice_transient_analyze(&bench, argv[2], (Pin){10, 1}, settings.transient_step,
                                   settings.transient_stop, &meter_trace, message, sizeof(message)));
    assert(meter_trace.count == trace.count);
    assert(fabs(meter_trace.samples[trace.count - 1].value - trace.samples[trace.count - 1].value) < .001);
    SpiceLive *live = spice_live_create();
    assert(live);
    SimAutoOptions options = {0};
    options.probe = (Pin){11, 0};
    options.step = settings.transient_step;
    options.stop = settings.transient_stop;
    options.limits = burn_limits_default();
    SimAutoResult *result = calloc(1, sizeof(*result));
    assert(result);
    assert(netlist_live_node(&bench, 0, (Pin){9, 0}) >= 0);
    for (int i = 0; i < 50; ++i)
    {
        int stepped = spice_live_step(live, &bench, argv[2], &options, .001, result);
        double output = 0;
        int measured = stepped && spice_live_voltage(live, &bench, (Pin){9, 0}, &output, message, sizeof(message));
        if (i < 5 || i == 49)
            printf("live %d: step=%d measured=%d t=%.6g output=%.6g meter=%.6g scope=%.6g %s\n", i,
                   stepped, measured, spice_live_time(live), output, result->elements[10][0],
                   result->elements[11][0], result->message);
        assert(stepped && measured);
        assert(output > 0 && output < 10);
        assert(fabs(output - result->elements[10][0]) < .001);
        assert(fabs(output - result->elements[11][0]) < .001);
    }
    assert(result->elements[10][0] > 5.5);
    spice_live_destroy(live);
    live = spice_live_create();
    assert(live);
    assert(bench_set_value(&bench, 0, 1));
    options.step = .0001;
    options.stop = .05;
    assert(spice_live_step(live, &bench, argv[2], &options, .05, result));
    printf("1 Hz voltage mode: %.6g V\n", result->elements[10][0]);
    bench.parts[10].states = 1;
    assert(spice_live_step(live, &bench, argv[2], &options, .05, result));
    bench.parts[10].states = 2;
    assert(spice_live_step(live, &bench, argv[2], &options, .2, result));
    printf("1 Hz resistance mode: scope %.6g V\n", result->elements[11][0]);
    bench.parts[10].states = 0;
    assert(spice_live_step(live, &bench, argv[2], &options, .001, result));
    printf("1 Hz back to voltage mode: %.6g V\n", result->elements[10][0]);
    assert(result->elements[10][0] < 5);
    assert(fabs(result->elements[10][0] - result->elements[11][0]) < .001);
    bench.parts[10].states = 1;
    assert(spice_live_step(live, &bench, argv[2], &options, .01, result));
    bench.parts[10].states = 2;
    assert(spice_live_step(live, &bench, argv[2], &options, .2, result));
    bench.parts[10].states = 0;
    assert(spice_live_step(live, &bench, argv[2], &options, .001, result));
    printf("1 Hz after second mode cycle: %.6g V\n", result->elements[10][0]);
    assert(result->elements[10][0] < 5);
    if (getenv("BANCADA_BENCH_CW"))
    {
        spice_live_destroy(live);
        live = spice_live_create();
        assert(live);
        bench.parts[10].states = 0;
        ULONGLONG previous = GetTickCount64();
        for (int i = 0; i < 20; ++i)
        {
            assert(spice_live_step(live, &bench, argv[2], &options, .25, result));
            ULONGLONG now = GetTickCount64();
            printf("benchmark t=%.2f s, batch=%llu ms, output=%.4f V\n", spice_live_time(live),
                   (unsigned long long)(now - previous), result->elements[11][0]);
            previous = now;
        }
        Workbench no_load = {0};
        assert(bench_snapshot_clone(&no_load, &bench));
        bench_remove(&no_load, 11);
        bench_remove(&no_load, 10);
        spice_live_destroy(live);
        live = spice_live_create();
        assert(live);
        for (int i = 0; i < 80; ++i)
        {
            assert(spice_live_step(live, &no_load, argv[2], &options, .25, result));
            if (i % 20 == 19)
            {
                double output;
                assert(spice_live_voltage(live, &no_load, (Pin){9, 0}, &output, message, sizeof(message)));
                printf("unloaded t=%.2f s, output=%.4f V\n", spice_live_time(live), output);
            }
        }
        bench_snapshot_clear(&no_load);
    }
    if (getenv("BANCADA_BENCH_CW_SMALL"))
    {
        spice_live_destroy(live);
        live = spice_live_create();
        assert(live);
        bench.parts[10].states = 0;
        options.step = 1e-5;
        ULONGLONG previous = GetTickCount64();
        for (int i = 0; i < 500; ++i)
        {
            assert(spice_live_step(live, &bench, argv[2], &options, .02, result));
            if (i % 50 == 49)
            {
                ULONGLONG now = GetTickCount64();
                printf("small t=%.2f s, wall=%llu ms/50 batches, output=%.4f V\n", spice_live_time(live),
                       (unsigned long long)(now - previous), result->elements[11][0]);
                previous = now;
            }
        }
    }
    free(result);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
    return 0;
}
