#include "workbench_file.h"
#include "spice_live.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    Workbench bench = {0};
    assert(workbench_load("saves/slot-5.elec", &bench));
    SpiceLive *live = spice_live_create();
    assert(live);
    SimAutoOptions options = {0};
    options.step = 1e-5;
    options.stop = .005;
    options.limits = burn_limits_default();
    SimAutoResult *result = calloc(1, sizeof(*result));
    assert(result);
    for (int edge = 0; edge < 16; ++edge)
    {
        assert(spice_live_step(live, &bench, argv[1], &options, .00025, result));
        assert(spice_live_step(live, &bench, argv[1], &options, .00025, result));
        assert(spice_live_step(live, &bench, argv[1], &options, .00025, result));
        assert(spice_live_step(live, &bench, argv[1], &options, .00025, result));
        int value = 0;
        const int output_parts[] = {9, 22, 33, 44};
        for (int bit = 0; bit < 4; ++bit)
        {
            double voltage;
            char message[256];
            assert(spice_live_voltage(live, &bench, (Pin){output_parts[bit], 0}, &voltage, message, sizeof(message)));
            value |= (voltage > 2.5) << bit;
        }
        printf("edge %d: %d\n", edge + 1, value);
        assert(value == (edge + 1) % 16);
    }
    free(result);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}
