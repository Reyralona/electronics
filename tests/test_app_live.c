#include "app_simulation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void wait_until(AppState *app, double time)
{
    Uint64 started = SDL_GetTicks();
    do
    {
        app_simulation_update(app);
        if (app->sim.live_mode && app->sim.live_time >= time && app->sim.result_revision == app->bench.revision)
            return;
        if (app->sim.auto_failed)
        {
            fprintf(stderr, "%s\n", app->status);
            assert(!app->sim.auto_failed);
        }
        SDL_Delay(1);
    } while (SDL_GetTicks() - started < 10000);
    fprintf(stderr, "Timed out: t=%g, wanted=%g, status=%s\n", app->sim.live_time, time, app->status);
    assert(0);
}
static void interactive_led_updates(const char *executable, int potentiometer, int breadboard)
{
    static AppState app;
    app_state_init(&app);
    snprintf(app.ngspice, sizeof(app.ngspice), "%s", executable);
    app.sim.auto_enabled = 1;
    bench_add(&app.bench, PART_SOURCE, 300, 200);
    bench_add(&app.bench, potentiometer ? PART_POTENTIOMETER : PART_RESISTOR, 500, 200);
    bench_add(&app.bench, PART_RESISTOR, 700, 200);
    bench_add(&app.bench, PART_LED, 900, 200);
    bench_set_value(&app.bench, 1, 10000);
    bench_set_value(&app.bench, 2, 1000);
    if (potentiometer)
        bench_set_control(&app.bench, 1, .1);
    else
        bench_set_value(&app.bench, 1, 1000);
    if (breadboard)
        bench_add(&app.bench, PART_BREADBOARD, 700, 500);
    bench_toggle_wire(&app.bench, (Pin){0, 0}, (Pin){1, 0});
    if (potentiometer)
        bench_toggle_wire(&app.bench, (Pin){1, 2}, (Pin){0, 1});
    bench_toggle_wire(&app.bench, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&app.bench, (Pin){2, 1}, (Pin){3, 0});
    bench_toggle_wire(&app.bench, (Pin){3, 1}, (Pin){0, 1});
    wait_until(&app, .00001);
    double bright = app.sim.dc_led_currents[3];
    assert(app.leds[3] == 1 && bright > .001);
    uint64_t displayed = app.sim.result_revision;
    int samples_during_edit = 0;
    for (int frame = 0; frame < 80; ++frame)
    {
        double position = .15 + .2 * (frame % 20) / 19.0;
        if (potentiometer)
            bench_set_control(&app.bench, 1, position);
        else
            bench_set_value(&app.bench, 1, 10000 * position);
        app_simulation_update(&app);
        assert(app.leds[3] == 1);
        assert(app.sim.sim_running && !app.sim.auto_failed);
        if (app.sim.result_revision != displayed)
        {
            ++samples_during_edit;
            displayed = app.sim.result_revision;
        }
        SDL_Delay(5);
    }
    assert(samples_during_edit >= 3);
    if (potentiometer)
        bench_set_control(&app.bench, 1, .35);
    else
        bench_set_value(&app.bench, 1, 3500);
    wait_until(&app, app.sim.live_time + .00001);
    assert(app.sim.dc_led_currents[3] < bright * .6);
    assert(app.leds[3] == 1 && !app.sim.display_pending);
    /* A real disconnection must not retain a lit LED from the old topology. */
    bench_toggle_wire(&app.bench, (Pin){2, 1}, (Pin){3, 0});
    app_simulation_update(&app);
    assert(app.leds[3] != 1);
    app_state_dispose(&app);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    interactive_led_updates(argv[1], 1, 0);
    interactive_led_updates(argv[1], 1, 1);
    interactive_led_updates(argv[1], 0, 1);
    static AppState app;
    app_state_init(&app);
    snprintf(app.ngspice, sizeof(app.ngspice), "%s", argv[1]);
    app.sim.auto_enabled = 1;
    app.transient_stop = .05;
    bench_add(&app.bench, PART_SOURCE, 300, 200);
    bench_add(&app.bench, PART_RESISTOR, 500, 200);
    bench_add(&app.bench, PART_CAPACITOR, 700, 200);
    bench_add(&app.bench, PART_RESISTOR, 900, 200);
    bench_toggle_wire(&app.bench, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&app.bench, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&app.bench, (Pin){2, 0}, (Pin){3, 0});
    bench_toggle_wire(&app.bench, (Pin){2, 1}, (Pin){0, 1});
    bench_toggle_wire(&app.bench, (Pin){3, 1}, (Pin){0, 1});
    wait_until(&app, .005);
    assert(app.sim.dc_led_currents[3] > .00249);
    app.status = "Montagem salva para revisao.";
    wait_until(&app, app.sim.live_time + .0002);
    assert(!strcmp(app.status, "Montagem salva para revisao."));
    app.sim.sim_running = 0;
    Uint64 draining = SDL_GetTicks();
    while (app.dc_job.thread && SDL_GetTicks() - draining < 10000)
    {
        app_simulation_update(&app);
        SDL_Delay(1);
    }
    assert(!app.dc_job.thread);
    double paused = app.sim.live_time;
    SDL_Delay(30);
    app_simulation_update(&app);
    assert(app.sim.live_time == paused && !app.dc_job.thread);
    bench_move_part(&app.bench, 2, 700, 350);
    app.sim.sim_running = 1;
    wait_until(&app, paused + .0002);
    assert(app.sim.dc_led_currents[3] > .00249);
    paused = app.sim.live_time;
    bench_set_on(&app.bench, 0, 0);
    wait_until(&app, paused + .001);
    assert(app.sim.dc_led_currents[3] > .0006 && app.sim.dc_led_currents[3] < .0013);
    assert(app.sim.sim_running);
    app.sim.sim_running = 0;
    draining = SDL_GetTicks();
    while (app.dc_job.thread && SDL_GetTicks() - draining < 10000)
    {
        app_simulation_update(&app);
        SDL_Delay(1);
    }
    assert(!app.dc_job.thread);
    double before_reset = app.sim.live_time;
    spice_job_request_reset(&app.dc_job);
    app.sim.auto_requested_valid = 0;
    app.sim.sim_running = 1;
    app.sim.live_time = 0;
    wait_until(&app, .0001);
    assert(app.sim.live_time < before_reset);
    assert(app.sim.dc_led_currents[3] < 1e-8);
    app_state_dispose(&app);
    puts("Live LED control sweeps without blank frames, breadboards, pause, movement and charged source-off passed.");
    return 0;
}
