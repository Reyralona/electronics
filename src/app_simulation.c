#include "app_simulation.h"
#include "spice_trace.h"
#include <stdio.h>
#include <string.h>
#define AUTO_DEBOUNCE_MS 120

static const char *burn_message(char *buffer, size_t capacity, const Workbench *bench, const unsigned char burnt[],
                                const unsigned char reason[], const double value[])
{
    int i;
    for (i = 0; i < bench->part_count; ++i)
    {
        if (!burnt[i])
            continue;
        if (reason[i] == BURN_SHORT)
            snprintf(buffer, capacity, "%s em curto-circuito queimou. Corrija a ligacao e ela volta a funcionar.",
                     bench->parts[i].type == PART_CLOCK ? "Clock" : "Fonte");
        else if (reason[i] == BURN_OVERPOWER)
            snprintf(buffer, capacity,
                     "Resistor queimou: %.3g W (limite %.3g W). Corrija o circuito e ele volta a funcionar.", value[i],
                     burn_limits_default().resistor_max_power);
        else
            snprintf(buffer, capacity,
                     "LED queimou: %.3g mA (limite %.3g mA). Use um resistor em serie; ele volta a funcionar.",
                     value[i] * 1000.0, burn_limits_default().led_max_current * 1000.0);
        return buffer;
    }
    return NULL;
}

static void accept_automatic_result(AppState *app)
{
    const SimAutoResult *r = &app->dc_job.auto_result;
    app->sim.result_shape = app->dc_job.snapshot;
    memcpy(app->sim.burnt, r->burnt, sizeof(app->sim.burnt));
    memcpy(app->sim.burn_reason, r->burn_reason, sizeof(app->sim.burn_reason));
    memcpy(app->sim.burn_value, r->burn_value, sizeof(app->sim.burn_value));
    app->sim.burnt_count = r->burnt_count;
    app->sim.auto_failed = !app->dc_job.success;
    if (app->sim.auto_failed)
    {
        app->sim.dc_valid = 0;
        app->sim.dc_wire_valid = 0;
        app->sim.trace_valid = 0;
        snprintf(app->sim.auto_status, sizeof(app->sim.auto_status), "Nao foi possivel calcular: %s", r->message);
        app->status = app->sim.auto_status;
    }
    else
    {
        const char *burn_text;
        if (r->has_trace)
        {
            if (!app->sim.trace_valid || !app->sim.trace_automatic)
                app->show_trace = 0;
            app->sim.trace = app->dc_job.trace;
            app->sim.trace_valid = 1;
            app->sim.trace_snapshot = app->dc_job.snapshot;
            app->sim.trace_index = 0;
            app->sim.trace_pin = app->dc_job.probe.pin;
            app->sim.trace_current = 0;
            app->sim.trace_wire = -1;
            app->sim.trace_periodic = r->periodic;
            app->sim.trace_automatic = 1;
            app->sim.trace_play_start_index = 0;
            app->sim.trace_play_started = SDL_GetTicks();
            app->sim.dc_valid = 0;
            app->sim.dc_wire_valid = 0;
        }
        else
        {
            app->sim.trace_valid = 0;
            app->sim.trace_automatic = 0;
            app->sim.dc_valid = 1;
            app->sim.dc_snapshot = app->dc_job.snapshot;
            app->sim.dc_current = r->source_current;
            memcpy(app->sim.dc_led_currents, r->currents, sizeof(app->sim.dc_led_currents));
            app->sim.dc_wire_valid = r->measures_wires;
            if (app->sim.dc_wire_valid)
                memcpy(app->sim.dc_wire_currents, r->wire_currents, sizeof(app->sim.dc_wire_currents));
        }
        burn_text = burn_message(app->sim.auto_status, sizeof(app->sim.auto_status), &app->bench, app->sim.burnt,
                                 app->sim.burn_reason, app->sim.burn_value);
        if (burn_text)
            app->status = burn_text;
        else
        {
            snprintf(app->sim.auto_status, sizeof(app->sim.auto_status), "%s", r->message);
            app->status = app->sim.auto_status;
        }
    }
}

static void accept_transient_result(AppState *app)
{
    app->sim.trace_automatic = 0;
    app->sim.trace_periodic = sim_auto_is_periodic(&app->dc_job.snapshot);
    app->sim.trace_valid = app->dc_job.success;
    app->sim.trace_index = 0;
    app->sim.trace_pin = app->dc_job.probe.pin;
    app->sim.trace_current = app->dc_job.transient == 3 ? 2 : app->dc_job.transient == 2 ? 1 : 0;
    app->sim.trace_wire = app->sim.trace_current == 2 ? app->dc_job.probe.component : -1;
    app->sim.trace_snapshot = app->dc_job.snapshot;
    if (app->sim.trace_valid)
    {
        app->sim.trace = app->dc_job.trace;
        app->show_trace = 1;
        app->sim.sim_running = 0;
        snprintf(app->sim.trace_status, sizeof(app->sim.trace_status),
                 app->sim.trace_current == 2 ? "Corrente fio A -> B: t=%.6g s; I=%.6g A (1/%d)."
                 : app->sim.trace_current    ? "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (1/%d)."
                                             : "Tensao: t=%.6g s; V=%.6g V (1/%d).",
                 app->sim.trace.samples[0].time, app->sim.trace.samples[0].value, app->sim.trace.count);
        app->status = app->sim.trace_status;
    }
    else
    {
        memcpy(app->sim.dc_status, app->dc_job.message, sizeof(app->sim.dc_status));
        app->status = app->sim.dc_status;
    }
}

static void poll_result(AppState *app)
{
    if (spice_job_poll(&app->dc_job))
    {
        if (memcmp(&app->dc_job.snapshot, &app->bench, sizeof(app->bench)) != 0)
        {
            /* An automatic result for an older assembly is simply dropped; a newer analysis follows. */
            if (!app->dc_job.automatic)
            {
                app->sim.dc_valid = 0;
                app->status = "Montagem mudou durante analise; resultado descartado.";
            }
        }
        else if (app->dc_job.automatic)
        {
            accept_automatic_result(app);
        }
        else if (app->dc_job.transient)
        {
            accept_transient_result(app);
        }
        else
        {
            app->sim.dc_valid = app->dc_job.success;
            app->sim.dc_wire_valid = app->dc_job.success && app->dc_job.measures_wires;
            app->sim.dc_snapshot = app->dc_job.snapshot;
            app->sim.dc_current = app->dc_job.current;
            memcpy(app->sim.dc_led_currents, app->dc_job.led_currents, sizeof(app->sim.dc_led_currents));
            if (app->sim.dc_wire_valid)
                memcpy(app->sim.dc_wire_currents, app->dc_job.wire_currents, sizeof(app->sim.dc_wire_currents));
            memcpy(app->sim.dc_status, app->dc_job.message, sizeof(app->sim.dc_status));
            app->status = app->sim.dc_status;
        }
    }
}

static void schedule_analysis(AppState *app)
{
    /* Automatic analysis: once the assembly has been still for a moment, recalculate whatever changed. */
    if (memcmp(&app->sim.frame_bench, &app->bench, sizeof(app->bench)) != 0)
    {
        app->sim.frame_bench = app->bench;
        app->sim.bench_changed_at = SDL_GetTicks();
    }
    if (app->sim.auto_enabled && app->sim.sim_running && !app->dc_job.thread &&
        SDL_GetTicks() - app->sim.bench_changed_at >= AUTO_DEBOUNCE_MS)
    {
        int dirty =
            !app->sim.auto_requested_valid || memcmp(&app->sim.auto_requested, &app->bench, sizeof(app->bench)) != 0;
        dirty = dirty || app->sim.auto_show_current != app->show_current || app->sim.auto_step != app->transient_step ||
                app->sim.auto_stop != app->transient_stop;
        if (dirty)
        {
            SimAutoOptions options = {app->show_current, sim_state_probe(&app->bench, app->selected, app->probe_pin),
                                      app->transient_step, app->transient_stop, burn_limits_default()};
            app->sim.auto_requested = app->bench;
            app->sim.auto_requested_valid = 1;
            app->sim.auto_show_current = app->show_current;
            app->sim.auto_step = app->transient_step;
            app->sim.auto_stop = app->transient_stop;
            if (!spice_job_start_auto(&app->dc_job, &app->bench, app->ngspice, &options))
            {
                app->sim.auto_failed = 1;
                app->status = "Falha ao iniciar o calculo automatico.";
            }
        }
    }
}

static void invalidate_results(AppState *app)
{
    {
        /* Electrical results belong only to the exact assembly that produced them. */
        int keep = app->sim.auto_enabled && memcmp(&app->sim.result_shape, &app->bench, sizeof(app->bench)) == 0;
        if (!keep && app->sim.burnt_count && memcmp(&app->sim.result_shape, &app->bench, sizeof(app->bench)) != 0)
        {
            memset(app->sim.burnt, 0, sizeof(app->sim.burnt));
            app->sim.burnt_count = 0;
        }
        const char *paused_text = "Pausado: a montagem mudou; aperte play para calcular.";
        if (!keep && app->sim.trace_valid && memcmp(&app->sim.trace_snapshot, &app->bench, sizeof(app->bench)) != 0)
        {
            app->sim.trace_valid = 0;
            if (!app->sim.auto_enabled)
                app->status = "Montagem alterada; transitorio descartado.";
            else if (!app->sim.sim_running)
                app->status = paused_text;
        }
        if (!keep && app->sim.dc_valid && memcmp(&app->sim.dc_snapshot, &app->bench, sizeof(app->bench)) != 0)
        {
            app->sim.dc_valid = 0;
            if (!app->sim.auto_enabled)
                app->status = "Montagem alterada; pressione P para recalcular.";
            else if (!app->sim.sim_running)
                app->status = paused_text;
        }
    }
}

static void advance_playback(AppState *app)
{
    if (app->sim.trace_valid && app->sim.sim_running)
    {
        int finished = 0, index = spice_trace_replay_index(&app->sim.trace, app->sim.trace_play_start_index,
                                                           SDL_GetTicks() - app->sim.trace_play_started, &finished);
        if (index < 0)
        {
            app->sim.sim_running = 0;
            app->status = "Nao foi possivel reproduzir a curva.";
        }
        else
        {
            app->sim.trace_index = index;
            if (finished && app->sim.trace_periodic)
            {
                app->sim.trace_index = 0;
                app->sim.trace_play_start_index = 0;
                app->sim.trace_play_started = SDL_GetTicks();
            }
            else if (finished && !app->sim.trace_automatic)
            {
                app->sim.sim_running = 0;
                app->status = "Fim da curva; Espaco reinicia, B volta.";
            }
            /* An automatic, non-repeating curve (RC charging, for example) holds its final state. */
        }
    }
}

static void update_leds(AppState *app)
{
    int i;
    if (app->sim.trace_valid)
    {
        for (i = 0; i < app->bench.part_count; ++i)
            app->leds[i] =
                app->bench.parts[i].type == PART_LED && ((app->sim.trace.leds_on[app->sim.trace_index] >> i) & 1);
    }
    else if (app->sim.dc_valid)
    {
        for (i = 0; i < app->bench.part_count; ++i)
            app->leds[i] = app->bench.parts[i].type == PART_LED && app->sim.dc_led_currents[i] >= 1e-6;
    }
    else if (app->sim.auto_enabled && !app->sim.auto_failed)
    {
        for (i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
            app->leds[i] = app->sim.sim_running ? 0 : -1;
    }
    else if (!bench_boolean_supported(&app->bench))
    {
        for (i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
            app->leds[i] = -1;
    }
    else if (app->sim.sim_running)
        bench_simulate(&app->bench, app->leds);
    for (i = 0; i < app->bench.part_count; ++i)
        if (app->sim.burnt[i] && app->bench.parts[i].type == PART_LED)
            app->leds[i] = 2;
}

void app_simulation_update(AppState *app)
{
    poll_result(app);
    schedule_analysis(app);
    invalidate_results(app);
    advance_playback(app);
    update_leds(app);
}
