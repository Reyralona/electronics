#include "app_simulation.h"
#include <assert.h>
#include <stdlib.h>

int main(void)
{
    static AppState app;
    app_state_init(&app);
    assert(app.history);
    app.sim.sim_running = 0;
    assert(bench_add(&app.bench, PART_LED, 300, 200) == 0);

    /* The DC display uses only a result for the current assembly. */
    app.sim.dc_snapshot = app.bench;
    app.sim.dc_valid = 1;
    app.sim.dc_led_currents[0] = .003;
    app_simulation_update(&app);
    assert(app.leds[0] == 1 && app.sim.dc_valid);
    app.bench.parts[0].x += 20;
    app_simulation_update(&app);
    assert(!app.sim.dc_valid);

    /* Paused transient inspection displays the selected sample, without a window. */
    app.sim.trace_snapshot = app.bench;
    app.sim.trace_valid = 1;
    app.sim.trace.count = 2;
    app.sim.trace.samples[0] = (SpiceSample){0, 0};
    app.sim.trace.samples[1] = (SpiceSample){.001, 5};
    app.sim.trace.leds_on[0] = 0;
    app.sim.trace.leds_on[1] = 1;
    app.sim.trace_index = 0;
    app_simulation_update(&app);
    assert(app.leds[0] == 0);
    app.sim.trace_index = 1;
    app_simulation_update(&app);
    assert(app.leds[0] == 1);
    app.bench.parts[0].x += 20;
    app_simulation_update(&app);
    assert(!app.sim.trace_valid);
    free(app.history);
    return 0;
}
