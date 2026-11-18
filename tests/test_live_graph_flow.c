/* Live graph from a person's point of view (requested 17/11/2026): pressing T while the circuit runs
 * must keep the simulation going and graph the point live, and the graph window must close with its X.
 * Before this change T paused the simulation and computed a separate curve from t = 0. */
#include "app_commands.h"
#include "app_simulation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef int (*AppCondition)(const AppState *app);

static void run_until(AppState *app, AppCondition condition, const char *what)
{
    const Uint64 limit_ms = 15000;
    Uint64 started = SDL_GetTicks();
    while (SDL_GetTicks() - started < limit_ms)
    {
        app_simulation_update(app);
        if (condition(app))
            return;
        assert(!app->sim.auto_failed);
        SDL_Delay(1);
    }
    fprintf(stderr, "Timed out waiting for %s: t=%g, status=%s\n", what, app->sim.live_time, app->status);
    assert(0);
}

static double time_mark;

static int live_time_passed_mark(const AppState *app)
{
    return app->sim.live_mode && app->sim.sim_running && app->sim.live_time > time_mark;
}

/* Waits until the running simulation moves past its current time: the person sees time advance. */
static void see_simulation_advance(AppState *app)
{
    time_mark = app->sim.live_time + .01;
    run_until(app, live_time_passed_mark, "the live simulation to advance");
}

static int graph_shows_both_levels(const AppState *app)
{
    double low = 1e9, high = -1e9;
    for (int i = 0; i < app->live_graph.count; ++i)
    {
        double time, value;
        sim_live_graph_sample(&app->live_graph, i, &time, &value);
        low = value < low ? value : low;
        high = value > high ? value : high;
    }
    return app->live_graph.count > 0 && high - low > 4.5;
}

static int full_curve_ready(const AppState *app)
{
    return sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic;
}

static void press(AppState *app, SDL_Keycode key, SDL_Keymod mod)
{
    app_commands_key(app, NULL, key, mod);
}

static void build_clock_circuit(AppState *app)
{
    assert(bench_add(&app->bench, PART_CLOCK, 300, 300) == 0);
    assert(bench_add(&app->bench, PART_RESISTOR, 600, 300) == 1);
    assert(bench_set_value(&app->bench, 0, 100));
    bench_set_on(&app->bench, 0, 1);
    assert(bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0}) == 1);
    assert(bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){0, 1}) == 1);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    static AppState app;
    app_state_init(&app);
    snprintf(app.ngspice, sizeof(app.ngspice), "%s", argv[1]);
    app.sim.auto_enabled = 1;
    app.sim.sim_running = 1;
    app.transient_stop = .05;
    app.transient_step = 1e-4;

    /* 1. The person builds a 100 Hz clock driving a resistor and the simulation starts by itself. */
    build_clock_circuit(&app);
    see_simulation_advance(&app);

    /* 2. Selects the resistor, terminal 1, and presses T: a live graph opens, nothing pauses. */
    app.selected = 1;
    app.probe_pin = 0;
    app.probe_current = 0;
    press(&app, SDLK_T, SDL_KMOD_NONE);
    assert(app.live_graph.active && app.show_trace);
    assert(!strcmp(app.status, tr(TXT_APP_LIVE_GRAPH_OPENED)));
    assert(app.sim.sim_running);
    see_simulation_advance(&app);
    run_until(&app, graph_shows_both_levels, "the live graph to show the clock's 0 V and 5 V");
    printf("Live graph: %d samples while the simulation ran to t=%.3f s\n", app.live_graph.count,
           app.sim.live_time);

    /* 3. Clicks the graph's X: the window closes and the simulation is still running. */
    app_commands_close_graph(&app);
    assert(!app.live_graph.active);
    assert(!strcmp(app.status, tr(TXT_APP_GRAPH_CLOSED)));
    see_simulation_advance(&app);

    /* 4. Opposite case: Shift+T asks for the full curve from t = 0, which pauses the live session. */
    press(&app, SDLK_T, SDL_KMOD_SHIFT);
    assert(!app.live_graph.active);
    run_until(&app, full_curve_ready, "the full curve");
    assert(!app.sim.sim_running && app.show_trace);

    /* 5. The X of the full curve closes it and the live simulation resumes. */
    app_commands_close_graph(&app);
    assert(!sim_state_has_result(&app.sim, SIM_RESULT_TRACE));
    assert(!strcmp(app.status, tr(TXT_APP_GRAPH_CLOSED_RESUMED)));
    assert(app.sim.sim_running);
    see_simulation_advance(&app);

    /* 6. With the live graph open the person adds a part: the probed index may change, so it closes. */
    press(&app, SDLK_T, SDL_KMOD_NONE);
    assert(app.live_graph.active);
    assert(bench_add(&app.bench, PART_LED, 900, 300) == 2);
    app_simulation_update(&app);
    assert(!app.live_graph.active);
    assert(!strcmp(app.status, tr(TXT_APP_LIVE_GRAPH_CLOSED_CHANGED)));

    spice_job_join(&app.dc_job);
    app_state_dispose(&app);
    puts("Live graph flow passed.");
    return 0;
}
