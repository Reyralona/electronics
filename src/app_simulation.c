#include "app_simulation.h"
#include "spice_trace.h"
#include <stdio.h>
#include <string.h>
#define AUTO_DEBOUNCE_MS 120

/* Without a breadboard, explicit wire endpoints determine connectivity.
 * Moving bodies or wire knees then leaves the electrical system unchanged. */
static int same_electrical_assembly(const Workbench *a, const Workbench *b)
{
    if (a->part_count != b->part_count || a->wire_count != b->wire_count)
        return 0;
    for (int i = 0; i < a->part_count; ++i)
    {
        const Part *p = &a->parts[i];
        const Part *q = &b->parts[i];
        if (p->type == PART_BREADBOARD || p->type != q->type || p->on != q->on ||
            p->value != q->value || p->control != q->control)
            return 0;
        for (int pin = 0; pin < bench_pin_count(p->type); ++pin)
        {
            int after = -1;
            for (;;)
            {
                Pin old_contact = {-1, -1}, new_contact = {-1, -1};
                int old_found = bench_find_contact_after(a, i, pin, after, &old_contact);
                int new_found = bench_find_contact_after(b, i, pin, after, &new_contact);
                if (old_found != new_found || old_contact.component != new_contact.component ||
                    old_contact.pin != new_contact.pin)
                    return 0;
                if (!old_found)
                    break;
                after = old_contact.component * WORKBENCH_MAX_PINS + old_contact.pin;
            }
        }
    }
    for (int i = 0; i < a->wire_count; ++i)
    {
        const Wire *p = &a->wires[i];
        const Wire *q = &b->wires[i];
        if (p->a.component != q->a.component || p->a.pin != q->a.pin ||
            p->b.component != q->b.component || p->b.pin != q->b.pin)
            return 0;
    }
    return 1;
}

static void retain_visual_edit_result(AppState *app)
{
    uint64_t previous = app->dc_job.snapshot.revision;
    if (!app->sim.auto_enabled || previous == app->bench.revision ||
        !same_electrical_assembly(&app->dc_job.snapshot, &app->bench))
        return;
    /* Never mutate the snapshot while its worker owns it. */
    if (app->dc_job.thread)
        return;
    if (app->sim.result_revision == previous)
        app->sim.result_revision = app->bench.revision;
    if (app->sim.burn_revision == previous)
        app->sim.burn_revision = app->bench.revision;
    if (app->sim.requested_revision == previous)
        app->sim.requested_revision = app->bench.revision;
    app->dc_job.snapshot.revision = app->bench.revision;
}

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
    app->sim.burn_revision = app->dc_job.snapshot.revision;
    memcpy(app->sim.burnt, r->burnt, sizeof(app->sim.burnt));
    memcpy(app->sim.burn_reason, r->burn_reason, sizeof(app->sim.burn_reason));
    memcpy(app->sim.burn_value, r->burn_value, sizeof(app->sim.burn_value));
    app->sim.burnt_count = r->burnt_count;
    app->sim.auto_failed = !app->dc_job.success;
    if (app->sim.auto_failed)
    {
        sim_state_clear_result(&app->sim);
        snprintf(app->sim.auto_status, sizeof(app->sim.auto_status), "Nao foi possivel calcular: %s", r->message);
        app->status = app->sim.auto_status;
    }
    else
    {
        const char *burn_text;
        if (r->has_trace)
        {
            if (!sim_state_has_result(&app->sim, SIM_RESULT_TRACE) || !app->sim.trace_automatic)
                app->show_trace = 0;
            app->sim.trace = app->dc_job.trace;
            sim_state_set_result(&app->sim, SIM_RESULT_TRACE, 1, app->dc_job.snapshot.revision);

            app->sim.trace_index = 0;
            app->sim.trace_pin = app->dc_job.probe.pin;
            app->sim.trace_current = 0;
            app->sim.trace_wire = -1;
            app->sim.trace_periodic = r->periodic;
            app->sim.trace_automatic = 1;
            app->sim.trace_play_start_index = 0;
            app->sim.trace_play_started = SDL_GetTicks();
            sim_state_discard_result(&app->sim, SIM_RESULT_DC);
            app->sim.dc_wire_valid = 0;
        }
        else
        {
            sim_state_discard_result(&app->sim, SIM_RESULT_TRACE);
            app->sim.trace_automatic = 0;
            sim_state_set_result(&app->sim, SIM_RESULT_DC, 1, app->dc_job.snapshot.revision);

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
    sim_state_set_result(&app->sim, SIM_RESULT_TRACE, app->dc_job.success, app->dc_job.snapshot.revision);
    app->sim.trace_index = 0;
    app->sim.trace_pin = app->dc_job.probe.pin;
    app->sim.trace_current = app->dc_job.transient == 3 ? 2 : app->dc_job.transient == 2 ? 1 : 0;
    app->sim.trace_wire = app->sim.trace_current == 2 ? app->dc_job.probe.component : -1;

    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
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
        if (app->dc_job.snapshot.revision != app->bench.revision)
        {
            /* An automatic result for an older assembly is simply dropped; a newer analysis follows. */
            if (!app->dc_job.automatic)
            {
                sim_state_discard_result(&app->sim, SIM_RESULT_DC);
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
            sim_state_set_result(&app->sim, SIM_RESULT_DC, app->dc_job.success, app->dc_job.snapshot.revision);
            app->sim.dc_wire_valid = app->dc_job.success && app->dc_job.measures_wires;

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
    if (app->sim.frame_revision != app->bench.revision)
    {
        app->sim.frame_revision = app->bench.revision;
        app->sim.bench_changed_at = SDL_GetTicks();
    }
    if (app->sim.auto_enabled && app->sim.sim_running && !app->dc_job.thread &&
        SDL_GetTicks() - app->sim.bench_changed_at >= AUTO_DEBOUNCE_MS)
    {
        int dirty = !app->sim.auto_requested_valid || app->sim.requested_revision != app->bench.revision;
        dirty = dirty || app->sim.auto_show_current != app->show_current || app->sim.auto_step != app->transient_step ||
                app->sim.auto_stop != app->transient_stop;
        if (dirty)
        {
            SimAutoOptions options = {app->show_current, sim_state_probe(&app->bench, app->selected, app->probe_pin),
                                      app->transient_step, app->transient_stop, burn_limits_default()};
            app->sim.requested_revision = app->bench.revision;
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

void app_simulation_update(AppState *app)
{
    retain_visual_edit_result(app);
    poll_result(app);
    schedule_analysis(app);
    const char *message = sim_state_invalidate(&app->sim, &app->bench);
    if (message)
        app->status = message;
    message = sim_state_advance(&app->sim, SDL_GetTicks());
    if (message)
        app->status = message;
    sim_state_leds(&app->sim, &app->bench, app->leds);
}
