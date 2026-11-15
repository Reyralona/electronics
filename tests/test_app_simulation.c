#include "app_simulation.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int SDLCALL completed_worker(void *unused)
{
    (void)unused;
    return 0;
}

static void test_discard_after_undo(AppState *app)
{
    app->dc_job.snapshot = app->bench;
    app->dc_job.success = 1;
    app->dc_job.current = .003;
    app->dc_job.thread = SDL_CreateThread(completed_worker, "completed-test", NULL);
    assert(app->dc_job.thread);
    SDL_SetAtomicInt(&app->dc_job.done, 1);
    bench_history_record(app->history, &app->bench);
    assert(bench_move_part(&app->bench, 0, app->bench.parts[0].x + 20, app->bench.parts[0].y));
    assert(bench_history_undo(app->history, &app->bench));
    assert(app->bench.parts[0].x == app->dc_job.snapshot.parts[0].x);
    assert(app->bench.revision != app->dc_job.snapshot.revision);
    app_simulation_update(app);
    assert(!app->dc_job.thread && !sim_state_has_result(&app->sim, SIM_RESULT_DC));
    assert(strstr(app->status, "descartado"));
}

int main(void)
{
    static AppState app;
    app_state_init(&app);
    assert(app.history);
    app.sim.sim_running = 0;
    assert(bench_add(&app.bench, PART_LED, 300, 200) == 0);

    /* The DC display uses only a result for the current assembly. */

    sim_state_set_result(&app.sim, SIM_RESULT_DC, 1, app.bench.revision);
    app.sim.dc_led_currents[0] = .003;
    app_simulation_update(&app);
    assert(app.leds[0] == 1 && sim_state_has_result(&app.sim, SIM_RESULT_DC));
    assert(bench_move_part(&app.bench, 0, app.bench.parts[0].x + 20, app.bench.parts[0].y));
    app_simulation_update(&app);
    assert(!sim_state_has_result(&app.sim, SIM_RESULT_DC));

    /* Paused transient inspection displays the selected sample, without a window. */

    sim_state_set_result(&app.sim, SIM_RESULT_TRACE, 1, app.bench.revision);
    app.sim.trace.count = 2;
    app.sim.trace.samples[0] = (SpiceSample){0, 0};
    app.sim.trace.samples[1] = (SpiceSample){.001, 5};
    app.sim.trace.leds_on[0] = 0;
    app.sim.trace.leds_on[1] = 1;
    app.sim.trace_index = 0;
    app_simulation_update(&app);
    assert(app.leds[0] == 0);
    app.sim.trace_index = 1;
    app_simulation_update(&app);
    assert(app.leds[0] == 1);
    assert(bench_move_part(&app.bench, 0, app.bench.parts[0].x + 20, app.bench.parts[0].y));
    app_simulation_update(&app);
    assert(!sim_state_has_result(&app.sim, SIM_RESULT_TRACE));
    test_discard_after_undo(&app);
    free(app.history);
    return 0;
}
