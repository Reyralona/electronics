#include "component_registry.h"
#include "sim_state.h"
#include "ui_text.h"
#include "spice_trace.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int sim_state_has_result(const SimState *sim, SimResultKind kind)
{
    return sim->result_kind == kind;
}

void sim_state_set_result(SimState *sim, SimResultKind kind, int success, uint64_t revision)
{
    sim->display_pending = 0;
    sim->result_kind = success ? kind : SIM_RESULT_NONE;
    sim->result_revision = revision;
    if (!success || kind != SIM_RESULT_DC)
        sim->dc_wire_valid = 0;
}

void sim_state_clear_result(SimState *sim)
{
    sim->display_pending = 0;
    sim->result_kind = SIM_RESULT_NONE;
    sim->dc_wire_valid = 0;
}

void sim_state_discard_result(SimState *sim, SimResultKind kind)
{
    if (sim->result_kind == kind)
        sim_state_clear_result(sim);
}

Pin sim_state_probe(const Workbench *bench, int selected, int probe_pin)
{
    int i;
    if (selected >= 0 && selected < bench->part_count && !bench_is_board(bench->parts[selected].type) &&
        probe_pin >= 0 && probe_pin < bench_pin_count(bench->parts[selected].type))
        return (Pin){selected, probe_pin};
    for (i = 0; i < bench->part_count; ++i)
        if (component_electrical_type(bench->parts[i].type) == PART_SOURCE || bench->parts[i].type == PART_CLOCK)
            return (Pin){i, 0};
    for (i = 0; i < bench->part_count; ++i)
        if (!bench_is_board(bench->parts[i].type))
            return (Pin){i, 0};
    return (Pin){0, 0};
}

const char *sim_state_invalidate(SimState *sim, const Workbench *bench)
{
    const char *message = NULL;
    /* Electrical results belong only to the exact assembly that produced them. */
    int keep = sim->auto_enabled && (sim->burn_revision == bench->revision ||
               (sim->sim_running && sim->display_pending));
    if (!keep && sim->burnt_count && sim->burn_revision != bench->revision)
    {
        memset(sim->burnt, 0, sizeof(sim->burnt));
        sim->burnt_count = 0;
    }
    const char *paused_text = tr(TXT_APP_CHANGED_WHILE_PAUSED);
    if (!keep && sim_state_has_result(sim, SIM_RESULT_TRACE) && sim->result_revision != bench->revision)
    {
        sim_state_discard_result(sim, SIM_RESULT_TRACE);
        if (!sim->auto_enabled)
            message = tr(TXT_APP_CURVE_DISCARDED);
        else if (!sim->sim_running)
            message = paused_text;
    }
    if (!keep && sim_state_has_result(sim, SIM_RESULT_DC) && sim->result_revision != bench->revision)
    {
        sim_state_discard_result(sim, SIM_RESULT_DC);
        if (!sim->auto_enabled)
            message = tr(TXT_APP_CHANGED_PRESS_P);
        else if (!sim->sim_running)
            message = paused_text;
    }
    return message;
}

const char *sim_state_advance(SimState *sim, uint64_t now)
{
    if (sim_state_has_result(sim, SIM_RESULT_TRACE) && sim->sim_running)
    {
        int finished = 0, index = spice_trace_replay_index(&sim->trace, sim->trace_play_start_index,
                                                           now - sim->trace_play_started, &finished);
        if (index < 0)
        {
            sim->sim_running = 0;
            return tr(TXT_APP_CURVE_PLAY_FAILED);
        }
        else
        {
            sim->trace_index = index;
            if (finished && sim->trace_periodic)
            {
                sim->trace_index = 0;
                sim->trace_play_start_index = 0;
                sim->trace_play_started = now;
            }
            else if (finished && !sim->trace_automatic)
            {
                sim->sim_running = 0;
                return tr(TXT_APP_CURVE_END);
            }
            /* An automatic, non-repeating curve (RC charging, for example) holds its final state. */
        }
    }
    return NULL;
}

static TextId trace_format(const SimState *sim)
{
    if (sim->trace_current == 2)
        return TXT_APP_TRACE_WIRE_CURRENT;
    if (sim->trace_current)
        return TXT_APP_TRACE_PART_CURRENT;
    return TXT_APP_TRACE_VOLTAGE;
}

const char *sim_state_trace_text(SimState *sim)
{
    int index = sim->trace_index;
    snprintf(sim->trace_status, sizeof(sim->trace_status), tr(trace_format(sim)), sim->trace.samples[index].time,
             sim->trace.samples[index].value, index + 1, sim->trace.count);
    return sim->trace_status;
}

void sim_state_leds(const SimState *sim, const Workbench *bench, int leds[])
{
    int i;
    if (sim_state_has_result(sim, SIM_RESULT_TRACE))
    {
        for (i = 0; i < bench->part_count; ++i)
            leds[i] = bench->parts[i].type == PART_LED && ((sim->trace.leds_on[sim->trace_index] >> i) & 1);
    }
    else if (sim_state_has_result(sim, SIM_RESULT_DC))
    {
        for (i = 0; i < bench->part_count; ++i)
            leds[i] = bench->parts[i].type == PART_LED && sim->dc_led_currents[i] >= 1e-6;
    }
    else if (sim->auto_enabled && !sim->auto_failed)
    {
        for (i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
            leds[i] = sim->sim_running ? 0 : -1;
    }
    else if (!bench_boolean_supported(bench))
    {
        for (i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
            leds[i] = -1;
    }
    else if (sim->sim_running)
        bench_simulate(bench, leds);
    for (i = 0; i < bench->part_count; ++i)
        if (sim->burnt[i] && bench->parts[i].type == PART_LED)
            leds[i] = 2;
}

void sim_state_accept_elements(SimState *sim, const Workbench *bench,
                               const double elements[WORKBENCH_MAX_COMPONENTS][COMPONENT_MAX_ELEMENTS], double time)
{
    double elapsed = time > sim->element_time ? time - sim->element_time : 0;
    memcpy(sim->dc_elements, elements, sizeof(sim->dc_elements));
    sim->element_time = time;
    for (int i = 0; i < bench->part_count && i < WORKBENCH_MAX_COMPONENTS; ++i)
    {
        PartType type = bench->parts[i].type;
        if (type == PART_DC_MOTOR || type == PART_VIBRATION_MOTOR || type == PART_ENCODER_MOTOR)
            sim->motor_angle[i] = fmod(sim->motor_angle[i] + elements[i][0] * elapsed, 6.283185307179586);
        if (type != PART_OSCILLOSCOPE)
            continue;
        if (sim->scope_count[i] == COMPONENT_SCOPE_HISTORY)
            memmove(sim->scope_history[i], sim->scope_history[i] + 1,
                    (COMPONENT_SCOPE_HISTORY - 1) * sizeof(sim->scope_history[i][0]));
        else
            ++sim->scope_count[i];
        sim->scope_history[i][sim->scope_count[i] - 1] = (float)elements[i][0];
    }
}

void sim_live_graph_open(SimLiveGraph *graph, SimLiveProbe probe, const Workbench *bench)
{
    memset(graph, 0, sizeof(*graph));
    graph->active = 1;
    graph->valid = 1;
    graph->probe = probe;
    graph->part_count = bench->part_count;
    graph->wire_count = bench->wire_count;
}

void sim_live_graph_close(SimLiveGraph *graph)
{
    memset(graph, 0, sizeof(*graph));
}

void sim_live_graph_sample(const SimLiveGraph *graph, int index, double *time, double *value)
{
    int slot = (graph->start + index) % SIM_LIVE_GRAPH_CAPACITY;
    *time = graph->time[slot];
    *value = graph->value[slot];
}

static void live_graph_push(SimLiveGraph *graph, double time, double value)
{
    int slot = (graph->start + graph->count) % SIM_LIVE_GRAPH_CAPACITY;
    graph->time[slot] = time;
    graph->value[slot] = value;
    if (graph->count < SIM_LIVE_GRAPH_CAPACITY)
        graph->count++;
    else
        graph->start = (graph->start + 1) % SIM_LIVE_GRAPH_CAPACITY;
}

void sim_live_graph_append(SimLiveGraph *graph, const SimLiveSamples *samples)
{
    if (!graph->active)
        return;
    graph->valid = samples->valid;
    if (!samples->valid || samples->count <= 0)
        return;
    double newest = 0, ignored;
    if (graph->count)
        sim_live_graph_sample(graph, graph->count - 1, &newest, &ignored);
    const double restart_tolerance = samples->interval * .5;
    if (graph->count && samples->start < newest - restart_tolerance)
    {
        graph->start = 0;
        graph->count = 0;
    }
    for (int i = 0; i < samples->count; ++i)
        live_graph_push(graph, samples->start + i * samples->interval, samples->values[i]);
}

int sim_live_graph_stale(const SimLiveGraph *graph, const Workbench *bench)
{
    return graph->active && (graph->part_count != bench->part_count || graph->wire_count != bench->wire_count);
}
