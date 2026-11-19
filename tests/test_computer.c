#include "workbench_computer.h"
#include "spice_live.h"
#include "workbench_view.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

/* The example computer runs its program in the continuous ngspice session: after enough clock periods the
 * output LEDs show 28 + 14 - 7, and the value holds because HLT stopped the clock. */
#define TEST_CLOCK_HZ 1000.0
#define LED_ON_AMPS 1e-3
#define LED_OFF_AMPS 1e-4
#define PERIODS_PER_BATCH 5
#define BATCHES 8

static int output_value(const Workbench *bench, const SimAutoResult *result)
{
    int value = 0;
    for (int bit = 0; bit < COMPUTER_OUTPUT_BITS; ++bit)
    {
        int led = workbench_computer_output_led(bench, bit);
        assert(led >= 0);
        double amps = result->currents[led];
        assert(amps > LED_ON_AMPS || amps < LED_OFF_AMPS);
        value |= (amps > LED_ON_AMPS) << bit;
    }
    return value;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    static Workbench bench;
    int selected = -1;
    assert(workbench_build_computer(&bench, &selected) && selected >= 0);
    printf("Computer: %d parts, %d wires.\n", bench.part_count, bench.wire_count);
    /* Too large for the window at 1:1, so it opens framed; a small bench keeps 1:1. */
    BenchView view;
    bench_view_open_example(&view, &bench);
    assert(view.zoom < 1);
    static Workbench small;
    assert(bench_add(&small, PART_LED, 400, 300) == 0);
    bench_view_open_example(&view, &small);
    assert(view.zoom == 1 && view.pan_x == 0 && view.pan_y == 0);
    bench_snapshot_clear(&small);
    int clock_part = -1;
    for (int i = 0; i < bench.part_count; ++i)
        if (bench.parts[i].type == PART_CLOCK)
            clock_part = i;
    assert(clock_part >= 0 && bench_set_value(&bench, clock_part, TEST_CLOCK_HZ));
    SimAutoOptions options = {1, {selected, 0}, sim_auto_live_max_step(&bench), .005, burn_limits_default(),
                              {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    SpiceLive *live = spice_live_create();
    assert(live);
    static SimAutoResult result;
    int values[BATCHES];
    clock_t start = clock();
    for (int batch = 0; batch < BATCHES; ++batch)
    {
        int success = spice_live_step(live, &bench, argv[1], &options, PERIODS_PER_BATCH / TEST_CLOCK_HZ, &result);
        if (!success)
            fprintf(stderr, "computer: %s\n", result.message);
        assert(success && result.burnt_count == 0);
        values[batch] = output_value(&bench, &result);
        printf("t = %2d ms: output %3d\n", (batch + 1) * PERIODS_PER_BATCH, values[batch]);
    }
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    assert(values[BATCHES - 1] == COMPUTER_PROGRAM_RESULT && values[BATCHES - 2] == COMPUTER_PROGRAM_RESULT);
    printf("Computer: program shows %d and halts; %.0f ms simulated in %.2f s.\n", COMPUTER_PROGRAM_RESULT,
           1000.0 * BATCHES * PERIODS_PER_BATCH / TEST_CLOCK_HZ, seconds);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
    return 0;
}
