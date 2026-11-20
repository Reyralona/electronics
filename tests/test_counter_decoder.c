#include "spice_live.h"
#include "workbench_file.h"
#include "workbench_pack.h"
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

static Pin named_port(const Workbench *rig, int group, char name)
{
    Pin pin;
    for (int port = 0; bench_group_port(rig, group, port, &pin); ++port)
    {
        char text[BENCH_PORT_NAME_CAPACITY];
        if (bench_port_name(&rig->parts[pin.component], pin.pin, text, sizeof(text)) && text[0] == name && !text[1])
            return pin;
    }
    assert(!"missing named decoder terminal");
    return (Pin){-1, -1};
}

static void check_copied_decoder(const char *solver)
{
    static Workbench rig;
    static BenchClipboard clipboard;
    BenchSelection selected = {0};
    selected.parts[bench_group_first(&bench, 1)] = 1;
    assert(bench_selection_copy(&bench, &selected, &clipboard));
    for (int source = 0; source < 5; ++source)
    {
        assert(bench_add(&rig, PART_SOURCE, 100 + source * 100, 100) == source);
        if (source)
            assert(bench_toggle_wire(&rig, (Pin){source,1}, (Pin){0,1}) == 1);
    }
    assert(bench_selection_paste(&rig, &selected, &clipboard, 700, 400));
    int original = rig.parts[5].group, offset = rig.part_count;
    assert(bench_selection_paste(&rig, &selected, &clipboard, 1200, 400));
    int copied = rig.parts[offset].group;
    assert(bench_mirror_part(&rig, offset));
    assert(bench_rotate_part(&rig, offset));
    int loads[2][7];
    for (int instance = 0; instance < 2; ++instance)
    {
        int group = instance ? copied : original;
        for (int bit = 0; bit < 4; ++bit)
            assert(bench_toggle_wire(&rig, named_port(&rig, group, "ABCD"[bit]), (Pin){bit+1,0}) == 1);
        for (int segment = 0; segment < 7; ++segment)
        {
            int load = bench_add(&rig, PART_RESISTOR, 100 + segment * 100, 900 + instance * 100);
            assert(load >= 0);
            loads[instance][segment] = load;
            assert(bench_toggle_wire(&rig, named_port(&rig, group, "abcdefg"[segment]), (Pin){load,0}) == 1);
            assert(bench_toggle_wire(&rig, (Pin){load,1}, (Pin){0,1}) == 1);
        }
    }
    SpiceLive *live = spice_live_create();
    assert(live);
    SimAutoOptions options = {0};
    options.step = .001;
    options.limits = burn_limits_default();
    for (int value = 0; value < 16; ++value)
    {
        for (int bit = 0; bit < 4; ++bit)
            assert(bench_set_value(&rig, bit + 1, (value >> bit & 1) ? 5 : 1e-12));
        int success = spice_live_step(live, &rig, solver, &options, .002, &result);
        if (!success) fprintf(stderr, "copied decoder input=%d: %s\n", value, result.message);
        assert(success);
        for (int segment = 0; segment < 7; ++segment)
            assert(fabs(result.currents[loads[0][segment]] - result.currents[loads[1][segment]]) < 1e-6);
    }
    assert(bench_set_on(&rig, 0, 0));
    assert(spice_live_step(live, &rig, solver, &options, .002, &result));
    for (int instance = 0; instance < 2; ++instance)
        for (int segment = 0; segment < 7; ++segment)
            assert(fabs(result.currents[loads[instance][segment]]) < 1e-6);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig);
    bench_snapshot_clear(&clipboard.bench);
    puts("Copied/mirrored/rotated decoder: same outputs for all 16 inputs, high impedance when unpowered.");
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
    check_copied_decoder(argv[2]);
    bench_snapshot_clear(&bench);
    return 0;
}
