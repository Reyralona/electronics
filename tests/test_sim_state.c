#include <math.h>
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

static void live_batch(SimLiveSamples *samples, double start, int count, double first_value)
{
    memset(samples, 0, sizeof(*samples));
    samples->valid = 1;
    samples->start = start;
    samples->interval = .001;
    samples->count = count;
    for (int i = 0; i < count; ++i)
        samples->values[i] = first_value + i;
}

/* Live graph: batches join in order, a restarted session starts over and old samples roll off. */
static void test_live_graph(void)
{
    static SimLiveGraph graph;
    static SimLiveSamples samples;
    Workbench bench = {0};
    assert(bench_add(&bench, PART_SOURCE, 0, 0) >= 0);
    sim_live_graph_open(&graph, (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {0, 0}, .001}, &bench);
    assert(graph.active && graph.count == 0);
    live_batch(&samples, 0, 10, 0);
    sim_live_graph_append(&graph, &samples);
    live_batch(&samples, .01, 10, 10);
    sim_live_graph_append(&graph, &samples);
    double time, value;
    assert(graph.count == 20);
    sim_live_graph_sample(&graph, 19, &time, &value);
    assert(fabs(time - .019) < 1e-12 && value == 19);
    live_batch(&samples, 0, 5, 100);
    sim_live_graph_append(&graph, &samples);
    assert(graph.count == 5);
    sim_live_graph_sample(&graph, 0, &time, &value);
    assert(time == 0 && value == 100);
    for (int batch = 0; batch < 20; ++batch)
    {
        live_batch(&samples, .005 + batch * .256, SIM_LIVE_MAX_SAMPLES, batch * 1000.0);
        sim_live_graph_append(&graph, &samples);
    }
    assert(graph.count == SIM_LIVE_GRAPH_CAPACITY);
    sim_live_graph_sample(&graph, SIM_LIVE_GRAPH_CAPACITY - 1, &time, &value);
    assert(value == 19000 + SIM_LIVE_MAX_SAMPLES - 1);
    samples.valid = 0;
    sim_live_graph_append(&graph, &samples);
    assert(!graph.valid && graph.count == SIM_LIVE_GRAPH_CAPACITY);
    assert(!sim_live_graph_stale(&graph, &bench));
    assert(bench_add(&bench, PART_LED, 100, 0) >= 0);
    assert(sim_live_graph_stale(&graph, &bench));
    sim_live_graph_close(&graph);
    assert(!graph.active && !sim_live_graph_stale(&graph, &bench));
    bench_snapshot_clear(&bench);
}

int main(void)
{
    test_live_graph();
    test_result_transitions();
    test_invalidation();
    test_playback();
    test_leds();
    test_probe_selection();
    return 0;
}
