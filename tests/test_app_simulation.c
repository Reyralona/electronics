#include "app_simulation.h"
#include "ui_text.h"
#include <assert.h>
#include <math.h>
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
    assert(!strcmp(app->status, tr(TXT_APP_RESULT_DISCARDED)));
}

static void paused_program(void)
{
    static AppState app;
    app_state_init(&app);
    app.sim.sim_running = 0;
    assert(bench_add(&app.bench, PART_ARDUINO_UNO, 300, 200) == 0);
    assert(bench_set_code(&app.bench, 0,
                          "int n; void setup(){} void loop(){Serial.println(++n);}"));
    /* Create a board while paused: neither setup nor loop executes. Resume, then pause again. */
    app_simulation_update(&app);
    assert(!app.mcu->mcu[0].source);
    app.sim.sim_running = 1;
    app_simulation_update(&app);
    assert(app.mcu->mcu[0].serial[0]);
    char serial[MCU_SERIAL_CAPACITY];
    strcpy(serial, app.mcu->mcu[0].serial);
    app.sim.sim_running = 0;
    double clock = app.mcu->clock_ms;
    SDL_Delay(2);
    app_simulation_update(&app);
    assert(app.mcu->clock_ms == clock && !strcmp(app.mcu->mcu[0].serial, serial));
    app.sim.sim_running = 1;
    SDL_Delay(2);
    app_simulation_update(&app);
    assert(app.mcu->clock_ms > clock && strcmp(app.mcu->mcu[0].serial, serial));
    app_state_dispose(&app);
}

static void paused_dip_change(void)
{
    static AppState app;
    app_state_init(&app);
    app.sim.auto_enabled = 1;
    app.sim.sim_running = 0;
    assert(bench_add(&app.bench, PART_DIP_SWITCH, 300, 200) == 0);
    assert(bench_snapshot_clone(&app.dc_job.snapshot, &app.bench));
    app.sim.auto_requested_valid = 1;
    app.sim.requested_revision = app.bench.revision;
    sim_state_set_result(&app.sim, SIM_RESULT_DC, 1, app.bench.revision);
    /* Moving the body keeps the reading; flipping a contact invalidates it while paused. */
    assert(bench_move_part(&app.bench, 0, 500, 200));
    app_simulation_update(&app);
    assert(sim_state_has_result(&app.sim, SIM_RESULT_DC));
    assert(bench_set_states(&app.bench, 0, 1));
    app_simulation_update(&app);
    assert(!sim_state_has_result(&app.sim, SIM_RESULT_DC));
    app_state_dispose(&app);
}

int main(void)
{
    paused_program();
    paused_dip_change();
    assert(fabs(app_simulation_live_span(1000, 1016, 1e-5, 1, 0) - .016) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1100, 1e-5, 1, 0) - .02) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1100, 1e-6, 1, 0) - .002) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1100, .1, 1, 0) - .1) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1016, 1e-5, .25, 0) - .004) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1016, 1e-5, .05, 0) - .0008) < 1e-12);
    assert(app_simulation_live_span(1000, 1100, 0, 1, 0) == 0);
    assert(app_simulation_live_span(1000, 1100, 1e-5, 0, 0) == 0);
    assert(app_simulation_live_span(1000, 1100, 1e-5, 2, 0) == 0);
    /* A slow circuit gets shorter batches, so the screen keeps updating about ten times a second. */
    assert(fabs(app_simulation_live_span(1000, 1100, 1e-3, 1, .01) - .001) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1100, 1e-3, 1, 2) - .02) < 1e-12);
    assert(fabs(app_simulation_live_span(1000, 1100, 1e-3, 1, 1e-6) - 1e-3) < 1e-12);
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
    /* Pure geometry retains automatic playback and the same trace position. */
    assert(bench_snapshot_clone(&app.dc_job.snapshot, &app.bench));
    app.sim.auto_enabled = 1;
    app.sim.sim_running = 0;
    app.sim.auto_requested_valid = 1;
    app.sim.requested_revision = app.bench.revision;
    sim_state_set_result(&app.sim, SIM_RESULT_TRACE, 1, app.bench.revision);
    app.sim.trace_index = 1;
    assert(bench_move_part(&app.bench, 0, 650, 400));
    app_simulation_update(&app);
    assert(sim_state_has_result(&app.sim, SIM_RESULT_TRACE));
    assert(app.sim.trace_index == 1);
    assert(app.sim.result_revision == app.bench.revision);
    assert(app.sim.requested_revision == app.bench.revision);
    assert(bench_add(&app.bench, PART_RESISTOR, 800, 400) == 1);
    app_simulation_update(&app);
    assert(!sim_state_has_result(&app.sim, SIM_RESULT_TRACE));

    app_state_dispose(&app);
    return 0;
}
