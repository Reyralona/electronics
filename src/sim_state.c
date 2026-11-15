#include "sim_state.h"
#include "spice_trace.h"
#include <string.h>

int sim_state_has_result(const SimState *sim, SimResultKind kind)
{
    return sim->result_kind == kind;
}

void sim_state_set_result(SimState *sim, SimResultKind kind, int success, uint64_t revision)
{
    sim->result_kind = success ? kind : SIM_RESULT_NONE;
    sim->result_revision = revision;
    if (!success || kind != SIM_RESULT_DC)
        sim->dc_wire_valid = 0;
}

void sim_state_clear_result(SimState *sim)
{
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
    if (selected >= 0 && selected < bench->part_count && bench->parts[selected].type != PART_BREADBOARD &&
        probe_pin >= 0 && probe_pin < bench_pin_count(bench->parts[selected].type))
        return (Pin){selected, probe_pin};
    for (i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type == PART_SOURCE || bench->parts[i].type == PART_CLOCK)
            return (Pin){i, 0};
    for (i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].type != PART_BREADBOARD)
            return (Pin){i, 0};
    return (Pin){0, 0};
}

const char *sim_state_invalidate(SimState *sim, const Workbench *bench)
{
    const char *message = NULL;
    /* Electrical results belong only to the exact assembly that produced them. */
    int keep = sim->auto_enabled && sim->burn_revision == bench->revision;
    if (!keep && sim->burnt_count && sim->burn_revision != bench->revision)
    {
        memset(sim->burnt, 0, sizeof(sim->burnt));
        sim->burnt_count = 0;
    }
    const char *paused_text = "Pausado: a montagem mudou; aperte play para calcular.";
    if (!keep && sim_state_has_result(sim, SIM_RESULT_TRACE) && sim->result_revision != bench->revision)
    {
        sim_state_discard_result(sim, SIM_RESULT_TRACE);
        if (!sim->auto_enabled)
            message = "Montagem alterada; transitorio descartado.";
        else if (!sim->sim_running)
            message = paused_text;
    }
    if (!keep && sim_state_has_result(sim, SIM_RESULT_DC) && sim->result_revision != bench->revision)
    {
        sim_state_discard_result(sim, SIM_RESULT_DC);
        if (!sim->auto_enabled)
            message = "Montagem alterada; pressione P para recalcular.";
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
            return "Nao foi possivel reproduzir a curva.";
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
                return "Fim da curva; Espaco reinicia, B volta.";
            }
            /* An automatic, non-repeating curve (RC charging, for example) holds its final state. */
        }
    }
    return NULL;
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
