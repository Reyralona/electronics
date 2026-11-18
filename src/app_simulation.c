#include "component_registry.h"
#include "app_simulation.h"
#include "ui_text.h"
#include "spice_live.h"
#include "component_devices.h"
#include "spice_trace.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#define AUTO_DEBOUNCE_MS 120
#define LIVE_FRAME_INTERVAL_MS 16
#define LIVE_MAX_BATCH_SECONDS .02
#define LIVE_MAX_BATCH_STEPS 2000.0
#define LIVE_TARGET_BATCH_WALL_SECONDS .1

double app_simulation_live_span(uint64_t previous_ms, uint64_t now_ms, double step, double speed,
                                double observed_rate)
{
    if (!isfinite(step) || step <= 0 || !isfinite(speed) || speed < .05 || speed > 1.0)
        return 0;
    double elapsed = previous_ms && now_ms >= previous_ms ? (now_ms - previous_ms) * .001 : .016;
    double maximum = fmin(LIVE_MAX_BATCH_SECONDS, step * LIVE_MAX_BATCH_STEPS);
    if (isfinite(observed_rate) && observed_rate > 0)
        maximum = fmin(maximum, observed_rate * LIVE_TARGET_BATCH_WALL_SECONDS);
    return fmax(step, fmin(elapsed * speed, maximum));
}

/* Wire endpoints and coincident terminals define connectivity, including breadboards.
 * Parameter edits may retain a displayed sample without claiming it is up to date. */
static int same_electrical_assembly(const Workbench *a, const Workbench *b, int compare_controls)
{
    if (a->part_count != b->part_count || a->wire_count != b->wire_count)
        return 0;
    for (int i = 0; i < a->part_count; ++i)
    {
        const Part *p = &a->parts[i];
        const Part *q = &b->parts[i];
        if (p->type != q->type || p->group != q->group || p->board_layout != q->board_layout)
            return 0;
        if (compare_controls && (p->on != q->on || p->value != q->value || p->control != q->control))
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
    if (!app->sim.auto_enabled || app->dc_job.live_reset_pending || previous == app->bench.revision ||
        !same_electrical_assembly(&app->dc_job.snapshot, &app->bench, 1))
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
            snprintf(buffer, capacity, "%s",
                     tr(bench->parts[i].type == PART_CLOCK ? TXT_APP_BURN_CLOCK_SHORT : TXT_APP_BURN_SOURCE_SHORT));
        else if (reason[i] == BURN_OVERPOWER)
            snprintf(buffer, capacity, tr(TXT_APP_BURN_RESISTOR), value[i], burn_limits_default().resistor_max_power);
        else
            snprintf(buffer, capacity, tr(TXT_APP_BURN_LED), value[i] * 1000.0,
                     burn_limits_default().led_max_current * 1000.0);
        return buffer;
    }
    return NULL;
}

static void accept_automatic_result(AppState *app)
{
    const SimAutoResult *r = &app->dc_job.auto_result;
    int was_live = app->sim.live_mode, was_failed = app->sim.auto_failed, had_burnt = app->sim.burnt_count;
    app->sim.live_mode = app->dc_job.continuous;
    if (app->dc_job.continuous && app->dc_job.success)
    {
        double time = spice_live_time(app->dc_job.live);
        uint64_t now = SDL_GetTicks();
        if (app->sim.live_sample_at && now > app->sim.live_sample_at && time > app->sim.live_time)
        {
            double rate = (time - app->sim.live_time) * 1000.0 / (now - app->sim.live_sample_at);
            app->sim.live_rate = app->sim.live_rate > 0 ? .8 * app->sim.live_rate + .2 * rate : rate;
        }
        else
            app->sim.live_rate = 0;
        app->sim.live_time = time;
        app->sim.live_sample_at = now;
    }
    app->sim.burn_revision = app->dc_job.snapshot.revision;
    memcpy(app->sim.burnt, r->burnt, sizeof(app->sim.burnt));
    memcpy(app->sim.burn_reason, r->burn_reason, sizeof(app->sim.burn_reason));
    memcpy(app->sim.burn_value, r->burn_value, sizeof(app->sim.burn_value));
    app->sim.burnt_count = r->burnt_count;
    app->sim.auto_failed = !app->dc_job.success;
    if (app->sim.auto_failed)
    {
        sim_state_clear_result(&app->sim);
        snprintf(app->sim.auto_status, sizeof(app->sim.auto_status), tr(TXT_APP_CALCULATION_FAILED),
                 r->message);
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
            sim_state_accept_elements(&app->sim, &app->dc_job.snapshot, r->elements, app->sim.live_time);
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
            if (!app->dc_job.continuous)
            {
                snprintf(app->sim.auto_status, sizeof(app->sim.auto_status), "%s", r->message);
                app->status = app->sim.auto_status;
            }
            else if (spice_live_restarted(app->dc_job.live))
                app->status = tr(TXT_APP_LIVE_RESTARTED);
            else if (app->dc_job.reset_live)
                app->status = tr(TXT_APP_LIVE_RESET);
            else if (!was_live || was_failed || had_burnt)
                app->status = r->powered ? tr(TXT_APP_SIM_RUNNING) :
                                          tr(TXT_APP_SUPPLY_OFF);
            /* Steady live samples update the footer, preserving action/error feedback in the status line. */
        }
    }
}

static void accept_transient_result(AppState *app)
{
    app->sim.live_mode = 0;
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
        app->status = sim_state_trace_text(&app->sim);
    }
    else
    {
        memcpy(app->sim.dc_status, app->dc_job.message, sizeof(app->sim.dc_status));
        app->status = app->sim.dc_status;
    }
}

static int compatible_live_sample(const AppState *app)
{
    return app->sim.auto_enabled && app->sim.sim_running && app->dc_job.automatic &&
           app->dc_job.continuous && !app->dc_job.live_reset_pending &&
           same_electrical_assembly(&app->dc_job.snapshot, &app->bench, 0);
}

static void poll_result(AppState *app)
{
    if (spice_job_poll(&app->dc_job))
    {
        if (app->dc_job.snapshot.revision != app->bench.revision &&
            !(app->dc_job.success && compatible_live_sample(app)))
        {
            /* Topology changes invalidate old samples; parameter updates can display a compatible sample. */
            if (!app->dc_job.automatic)
            {
                sim_state_discard_result(&app->sim, SIM_RESULT_DC);
                app->status = tr(TXT_APP_RESULT_DISCARDED);
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

static int needs_live_session(const Workbench *bench)
{
    if (sim_auto_is_dynamic(bench))
        return 1;
    for (int i = 0; i < bench->part_count; ++i)
        if (component_electrical_type(bench->parts[i].type) == PART_SOURCE ||
            bench->parts[i].type == PART_DFF || bench->parts[i].type == PART_CLOCK ||
            component_environment(bench->parts[i].type) || component_device(bench->parts[i].type))
            return 1;
    return 0;
}

static void schedule_analysis(AppState *app)
{
    /* Powered circuits update continuously; only the unpowered batch fallback is debounced. */
    if (app->sim.frame_revision != app->bench.revision)
    {
        app->sim.frame_revision = app->bench.revision;
        app->sim.bench_changed_at = SDL_GetTicks();
    }
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic && app->show_trace &&
        app->sim.result_revision == app->bench.revision)
        return;
    int continuous = needs_live_session(&app->bench);
    if (app->sim.auto_enabled && app->sim.sim_running && !app->dc_job.thread &&
        (continuous || SDL_GetTicks() - app->sim.bench_changed_at >= AUTO_DEBOUNCE_MS))
    {
        int dirty = !app->sim.auto_requested_valid || app->sim.requested_revision != app->bench.revision;
        dirty = dirty || (continuous && SDL_GetTicks() - app->live_last_scheduled >= LIVE_FRAME_INTERVAL_MS);
        if (app->sim.auto_failed && app->sim.auto_requested_valid && app->sim.requested_revision == app->bench.revision)
            dirty = 0;
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
            int started;
            if (continuous)
            {
                uint64_t now = SDL_GetTicks();
                options.step = sim_auto_live_max_step(&app->bench);
                double span = app_simulation_live_span(app->live_last_scheduled, now, options.step,
                                                       app->simulation_speed, app->sim.live_rate);
                started = spice_job_start_live(&app->dc_job, &app->bench, app->ngspice, &options,
                                               span);
            }
            else
                started = spice_job_start_auto(&app->dc_job, &app->bench, app->ngspice, &options);
            app->live_last_scheduled = SDL_GetTicks();
            if (!started)
            {
                app->sim.auto_failed = 1;
                app->status = tr(TXT_APP_SIMULATION_START_FAILED);
            }
        }
    }
}

/* Programs run on the wall clock while the simulation runs; paused means frozen. */
static void step_microcontrollers(AppState *app)
{
    uint64_t now = SDL_GetTicks();
    double elapsed = app->mcu_last_ticks && app->sim.sim_running ? (double)(now - app->mcu_last_ticks) : 0;
    app->mcu_last_ticks = now;
    int measured = sim_state_has_result(&app->sim, SIM_RESULT_DC);
    mcu_runtime_step(app->mcu, &app->bench, measured ? (const double(*)[COMPONENT_MAX_ELEMENTS])app->sim.dc_elements : NULL,
                     elapsed);
}

static void start_pending_trace(AppState *app)
{
    if (!app->trace_pending_kind || app->dc_job.thread)
        return;
    int kind = app->trace_pending_kind;
    int index = app->trace_pending_index;
    int pin = app->trace_pending_pin;
    int current = app->trace_pending_current;
    uint64_t revision = app->trace_pending_revision;
    app->trace_pending_kind = 0;
    if (revision != app->bench.revision)
    {
        app->status = tr(TXT_APP_RESULT_DISCARDED);
        return;
    }
    int started = 0;
    if (kind == 2 && index >= 0 && index < app->bench.wire_count)
        started = spice_job_start_transient_wire(&app->dc_job, &app->bench, app->ngspice, index,
                                                 app->transient_step, app->transient_stop);
    else if (kind == 1 && index >= 0 && index < app->bench.part_count &&
             (current || (pin >= 0 && pin < bench_pin_count(app->bench.parts[index].type))))
    {
        Pin probe = {index, current ? 0 : pin};
        if (app->show_current)
            started = spice_job_start_transient_instrumented(&app->dc_job, &app->bench, app->ngspice,
                                                              current ? 2 : 1, probe, app->transient_step,
                                                              app->transient_stop);
        else if (current)
            started = spice_job_start_transient_current(&app->dc_job, &app->bench, app->ngspice, index,
                                                        app->transient_step, app->transient_stop);
        else
            started = spice_job_start_transient(&app->dc_job, &app->bench, app->ngspice, probe,
                                                app->transient_step, app->transient_stop);
    }
    if (started)
    {
        sim_state_clear_result(&app->sim);
        app->status = tr(kind == 2 ? TXT_APP_CALCULATING_WIRE_CURRENT : TXT_APP_CALCULATING_CURVE);
    }
    else
        app->status = tr(TXT_APP_CALCULATION_START_FAILED);
}

void app_simulation_update(AppState *app)
{
    retain_visual_edit_result(app);
    poll_result(app);
    start_pending_trace(app);
    step_microcontrollers(app);
    app->sim.display_pending = sim_state_has_result(&app->sim, SIM_RESULT_DC) &&
                               app->sim.result_revision != app->bench.revision &&
                               !app->sim.auto_failed && compatible_live_sample(app);
    /* Validate against the old snapshot before scheduling replaces it. */
    const char *message = sim_state_invalidate(&app->sim, &app->bench);
    schedule_analysis(app);
    if (message)
        app->status = message;
    message = sim_state_advance(&app->sim, SDL_GetTicks());
    if (message)
        app->status = message;
    sim_state_leds(&app->sim, &app->bench, app->leds);
}
