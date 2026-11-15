#include "sim_state.h"
#include <assert.h>
#include <string.h>

static void test_invalidation(void)
{
    static SimState sim;
    Workbench bench = {0};
    assert(bench_add(&bench, PART_LED, 300, 200) == 0);
    sim.auto_enabled = 1;
    sim.dc_valid = 1;
    sim.dc_snapshot = bench;
    sim.result_shape = bench;
    sim.burnt_count = 1;
    sim.burnt[0] = 1;
    assert(sim_state_invalidate(&sim, &bench) == NULL);
    assert(sim.dc_valid && sim.burnt_count == 1);
    bench.parts[0].value += 1;
    const char *message = sim_state_invalidate(&sim, &bench);
    assert(message && strstr(message, "Pausado"));
    assert(!sim.dc_valid && !sim.burnt_count && !sim.burnt[0]);

    sim.trace_valid = 1;
    sim.trace_snapshot = bench;
    assert(sim_state_invalidate(&sim, &bench) == NULL);
    bench.parts[0].x += 10;
    sim.sim_running = 1;
    assert(sim_state_invalidate(&sim, &bench) == NULL);
    assert(!sim.trace_valid);
}

static void test_playback(void)
{
    static SimState sim;
    sim.trace_valid = 1;
    sim.sim_running = 1;
    sim.trace.count = 3;
    sim.trace.samples[0] = (SpiceSample){0, 0};
    sim.trace.samples[1] = (SpiceSample){.001, 2};
    sim.trace.samples[2] = (SpiceSample){.01, 5};
    sim.trace_play_started = 100;
    assert(sim_state_advance(&sim, 600) == NULL);
    assert(sim.trace_index == 1 && sim.sim_running);
    assert(sim_state_advance(&sim, 5100) != NULL);
    assert(sim.trace_index == 2 && !sim.sim_running);

    sim.sim_running = 1;
    sim.trace_periodic = 1;
    assert(sim_state_advance(&sim, 5100) == NULL);
    assert(sim.trace_index == 0 && sim.trace_play_started == 5100 && sim.sim_running);
    sim.trace_periodic = 0;
    sim.trace_automatic = 1;
    assert(sim_state_advance(&sim, 10100) == NULL);
    assert(sim.trace_index == 2 && sim.sim_running);
    sim.trace.count = 0;
    assert(sim_state_advance(&sim, 10101) != NULL);
    assert(!sim.sim_running);
}

static void test_leds(void)
{
    static SimState sim;
    Workbench bench = {0};
    int leds[WORKBENCH_MAX_COMPONENTS] = {0};
    assert(bench_add(&bench, PART_LED, 300, 200) == 0);
    sim.auto_enabled = 1;
    sim_state_leds(&sim, &bench, leds);
    assert(leds[0] == -1);
    sim.dc_valid = 1;
    sim.dc_led_currents[0] = .003;
    sim_state_leds(&sim, &bench, leds);
    assert(leds[0] == 1);
    sim.trace_valid = 1;
    sim.trace.leds_on[0] = 0;
    sim_state_leds(&sim, &bench, leds);
    assert(leds[0] == 0);
    sim.burnt[0] = 1;
    sim_state_leds(&sim, &bench, leds);
    assert(leds[0] == 2);
}

static void test_probe_selection(void)
{
    Workbench bench = {0};
    assert(bench_add(&bench, PART_SOURCE, 300, 200) == 0);
    assert(bench_add(&bench, PART_LED, 500, 200) == 1);
    Pin probe = sim_state_probe(&bench, 1, 1);
    assert(probe.component == 1 && probe.pin == 1);
    probe = sim_state_probe(&bench, 1, -1);
    assert(probe.component == 0 && probe.pin == 0);
    probe = sim_state_probe(&bench, 1, 100);
    assert(probe.component == 0 && probe.pin == 0);
    probe = sim_state_probe(&bench, 100, 0);
    assert(probe.component == 0 && probe.pin == 0);
}

int main(void)
{
    test_invalidation();
    test_playback();
    test_leds();
    test_probe_selection();
    return 0;
}
