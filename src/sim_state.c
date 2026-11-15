#include "sim_state.h"
#include "spice_trace.h"
#include <string.h>

Pin sim_state_probe(const Workbench *bench, int selected, int probe_pin)
{
    int i;
    if (selected >= 0 && selected < bench->part_count && bench->parts[selected].type != PART_BREADBOARD &&
        probe_pin < bench_pin_count(bench->parts[selected].type))
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
    int keep = sim->auto_enabled && memcmp(&sim->result_shape, bench, sizeof(*bench)) == 0;
    if (!keep && sim->burnt_count && memcmp(&sim->result_shape, bench, sizeof(*bench)) != 0)
    {
        memset(sim->burnt, 0, sizeof(sim->burnt));
        sim->burnt_count = 0;
    }
    const char *paused_text = "Pausado: a montagem mudou; aperte play para calcular.";
    if (!keep && sim->trace_valid && memcmp(&sim->trace_snapshot, bench, sizeof(*bench)) != 0)
    {
        sim->trace_valid = 0;
        if (!sim->auto_enabled)
            message = "Montagem alterada; transitorio descartado.";
        else if (!sim->sim_running)
            message = paused_text;
    }
    if (!keep && sim->dc_valid && memcmp(&sim->dc_snapshot, bench, sizeof(*bench)) != 0)
    {
        sim->dc_valid = 0;
        if (!sim->auto_enabled)
            message = "Montagem alterada; pressione P para recalcular.";
        else if (!sim->sim_running)
            message = paused_text;
    }
    return message;
}

const char *sim_state_advance(SimState *sim, uint64_t now)
{
    if (sim->trace_valid && sim->sim_running)
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
    if (sim->trace_valid)
    {
        for (i = 0; i < bench->part_count; ++i)
            leds[i] = bench->parts[i].type == PART_LED && ((sim->trace.leds_on[sim->trace_index] >> i) & 1);
    }
    else if (sim->dc_valid)
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
