#include "sim_state.h"
#include "ui_text.h"
#include <assert.h>
#include <string.h>

static void test_invalidation(void)
{
    static SimState sim;
    Workbench bench = {0};
    assert(bench_add(&bench, PART_LED, 300, 200) == 0);
    sim.auto_enabled = 1;
    sim_state_set_result(&sim, SIM_RESULT_DC, 1, bench.revision);

    sim.burn_revision = bench.revision;
    sim.burnt_count = 1;
    sim.burnt[0] = 1;
    assert(sim_state_invalidate(&sim, &bench) == NULL);
    assert(sim_state_has_result(&sim, SIM_RESULT_DC) && sim.burnt_count == 1);
    assert(bench_move_part(&bench, 0, 310, 200));
    const char *message = sim_state_invalidate(&sim, &bench);
    assert(message && !strcmp(message, tr(TXT_APP_CHANGED_WHILE_PAUSED)));
    assert(!sim_state_has_result(&sim, SIM_RESULT_DC) && !sim.burnt_count && !sim.burnt[0]);

    sim_state_set_result(&sim, SIM_RESULT_TRACE, 1, bench.revision);

    assert(sim_state_invalidate(&sim, &bench) == NULL);
    assert(bench_move_part(&bench, 0, 320, 200));
    sim.sim_running = 1;
    assert(sim_state_invalidate(&sim, &bench) == NULL);
    assert(!sim_state_has_result(&sim, SIM_RESULT_TRACE));
}

static void test_playback(void)
{
    static SimState sim;
    sim_state_set_result(&sim, SIM_RESULT_TRACE, 1, 0);
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
    sim_state_set_result(&sim, SIM_RESULT_DC, 1, bench.revision);
    sim.dc_led_currents[0] = .003;
    sim_state_leds(&sim, &bench, leds);
    assert(leds[0] == 1);
    sim_state_set_result(&sim, SIM_RESULT_TRACE, 1, bench.revision);
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

static void test_result_transitions(void)
{
    static SimState sim;
    assert(sim_state_has_result(&sim, SIM_RESULT_NONE));
    sim_state_set_result(&sim, SIM_RESULT_DC, 1, 10);
    sim.dc_wire_valid = 1;
    assert(sim_state_has_result(&sim, SIM_RESULT_DC));
    assert(!sim_state_has_result(&sim, SIM_RESULT_TRACE));
    sim_state_set_result(&sim, SIM_RESULT_TRACE, 1, 11);
    assert(sim_state_has_result(&sim, SIM_RESULT_TRACE) && sim.result_revision == 11);
    assert(!sim_state_has_result(&sim, SIM_RESULT_DC) && !sim.dc_wire_valid);
    sim_state_discard_result(&sim, SIM_RESULT_DC);
    assert(sim_state_has_result(&sim, SIM_RESULT_TRACE));
    sim_state_set_result(&sim, SIM_RESULT_TRACE, 0, 12);
    assert(sim_state_has_result(&sim, SIM_RESULT_NONE));
    sim_state_set_result(&sim, SIM_RESULT_DC, 1, 13);
    sim_state_discard_result(&sim, SIM_RESULT_TRACE);
    assert(sim_state_has_result(&sim, SIM_RESULT_DC));
    sim.dc_wire_valid = 1;
    sim_state_discard_result(&sim, SIM_RESULT_DC);
    assert(sim_state_has_result(&sim, SIM_RESULT_NONE) && !sim.dc_wire_valid);
}

int main(void)
{
    test_result_transitions();
    test_invalidation();
    test_playback();
    test_leds();
    test_probe_selection();
    return 0;
}
