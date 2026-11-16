#include "workbench_examples.h"
#include "spice_live.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Every example must run in the continuous session, which is what the app uses when it opens one. */
static const char *executable;
static SimAutoOptions options;

static SpiceLive *open_example(int index, Workbench *bench)
{
    int selected;
    assert(workbench_example_create(index, bench, &selected));
    options = (SimAutoOptions){1, {selected, 0}, 1e-5, .005, burn_limits_default()};
    SpiceLive *live = spice_live_create();
    assert(live);
    return live;
}

static void step(SpiceLive *live, const Workbench *bench, double span, SimAutoResult *result, int index)
{
    int success = spice_live_step(live, bench, executable, &options, span, result);
    if (!success)
        fprintf(stderr, "Exemplo %d (%s): %s\n", index, workbench_example_name(index), result->message);
    assert(success);
}

static void every_example_runs(void)
{
    const double frame_span = .005 / 300;
    const int frames = 12;
    for (int index = 0; index < WORKBENCH_EXAMPLE_COUNT; ++index)
    {
        Workbench bench = {0};
        SimAutoResult result;
        SpiceLive *live = open_example(index, &bench);
        for (int frame = 0; frame < frames; ++frame)
            step(live, &bench, frame_span, &result, index);
        assert(result.burnt_count == 0);
        spice_live_destroy(live);
        bench_snapshot_clear(&bench);
    }
}

static int find_part(const Workbench *bench, PartType type)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == type)
            return i;
    return -1;
}

/* NAND with one input tied high inverts the clock: LED dark while the clock is high, lit after it falls. */
static void nand_follows_clock(void)
{
    const int nand_example = 2;
    const double quarter_period = .00025;
    Workbench bench = {0};
    SimAutoResult result;
    SpiceLive *live = open_example(nand_example, &bench);
    int led = find_part(&bench, PART_LED);
    assert(led >= 0);
    double readings[4];
    for (int quarter = 0; quarter < 4; ++quarter)
    {
        step(live, &bench, quarter_period, &result, nand_example);
        readings[quarter] = result.currents[led];
    }
    assert(readings[0] < 1e-6 && readings[1] < 1e-6);
    assert(readings[2] > .002 && readings[3] > .002);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    every_example_runs();
    nand_follows_clock();
    printf("All %d examples run in the continuous session; NAND example follows the clock.\n",
           WORKBENCH_EXAMPLE_COUNT);
    return 0;
}
