#include "spice_live.h"
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

static SimAutoOptions options;
static SimAutoResult result;
static const char *executable;
static void step(SpiceLive *live, Workbench *bench, double span)
{
    int success = spice_live_step(live, bench, executable, &options, span, &result);
    if (!success)
        fprintf(stderr, "Live: %s\n", result.message);
    assert(success);
}
static double voltage(SpiceLive *live, Workbench *bench, int component, int pin)
{
    double value;
    char message[512];
    int success = spice_live_voltage(live, bench, (Pin){component, pin}, &value, message, sizeof(message));
    if (!success)
        fprintf(stderr, "Voltage: %s\n", message);
    assert(success);
    return value;
}
static void wire(Workbench *bench, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}
/* Slot reported by the user: source, switch, 470 ohm, 1 uF across 1 kOhm + LED. */
static void build_led_hold(Workbench *bench)
{
    bench_add(bench, PART_SOURCE, 300, 100);
    bench_add(bench, PART_SWITCH, 500, 100);
    bench_add(bench, PART_RESISTOR, 700, 100);
    bench_add(bench, PART_CAPACITOR, 900, 100);
    bench_add(bench, PART_RESISTOR, 1100, 100);
    bench_add(bench, PART_LED, 1300, 100);
    bench_set_value(bench, 2, 470);
    bench_set_value(bench, 3, 1e-6);
    bench_set_on(bench, 1, 1);
    /* Same wire order and capacitor orientation as the saved slot: node numbering affects pivoting. */
    wire(bench, 0, 0, 1, 0);
    wire(bench, 1, 1, 2, 0);
    wire(bench, 2, 1, 3, 1);
    wire(bench, 0, 1, 3, 0);
    wire(bench, 2, 1, 4, 0);
    wire(bench, 3, 0, 5, 1);
    wire(bench, 5, 0, 4, 1);
}
/* Opening the supply or the switch must fade the LED gradually without stalling the solver. */
static void check_led_hold(int open_part)
{
    const double frame_span = .005 / 300;
    const int fade_frames = 60;
    Workbench bench = {0};
    build_led_hold(&bench);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench, .005);
    double lit = result.currents[5];
    assert(lit > .001);
    bench_set_on(&bench, open_part, 0);
    double previous = lit;
    for (int frame = 0; frame < fade_frames; ++frame)
    {
        DWORD started = GetTickCount();
        step(live, &bench, frame_span);
        assert(GetTickCount() - started < 2000);
        assert(result.currents[5] <= previous * 1.0001);
        previous = result.currents[5];
    }
    assert(previous > lit * .1 && previous < lit * .9);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    options = (SimAutoOptions){0, {0, 0}, 1e-5, .005, burn_limits_default()};
    check_led_hold(0);
    check_led_hold(1);
    Workbench rc = {0};
    bench_add(&rc, PART_SOURCE, 300, 100);
    bench_add(&rc, PART_SWITCH, 500, 100);
    bench_add(&rc, PART_RESISTOR, 700, 100);
    bench_add(&rc, PART_CAPACITOR, 900, 100);
    bench_add(&rc, PART_RESISTOR, 1100, 100);
    bench_set_on(&rc, 1, 1);
    wire(&rc, 0, 0, 1, 0);
    wire(&rc, 1, 1, 2, 0);
    wire(&rc, 2, 1, 3, 0);
    wire(&rc, 3, 0, 4, 0);
    wire(&rc, 3, 1, 0, 1);
    wire(&rc, 4, 1, 0, 1);
    SpiceLive *live_rc = spice_live_create();
    assert(live_rc);
    step(live_rc, &rc, .005);
    double charged = voltage(live_rc, &rc, 3, 0);
    assert(charged > 2.49 && charged < 2.51);
    double before = spice_live_time(live_rc);
    bench_set_on(&rc, 0, 0);
    step(live_rc, &rc, .001);
    assert(spice_live_time(live_rc) > before);
    double discharged = voltage(live_rc, &rc, 3, 0);
    assert(discharged > .85 && discharged < 1);
    bench_set_on(&rc, 0, 1);
    step(live_rc, &rc, .005);
    bench_set_on(&rc, 1, 0);
    step(live_rc, &rc, .001);
    assert(voltage(live_rc, &rc, 3, 0) > .85 && voltage(live_rc, &rc, 3, 0) < 1);
    before = spice_live_time(live_rc);
    bench_move_part(&rc, 3, 900, 200);
    step(live_rc, &rc, .0001);
    assert(spice_live_time(live_rc) > before);
    Workbench dff = {0};
    bench_add(&dff, PART_SOURCE, 300, 200);
    bench_add(&dff, PART_SOURCE, 500, 200);
    bench_add(&dff, PART_DFF, 700, 200);
    bench_add(&dff, PART_RESISTOR, 900, 200);
    bench_set_value(&dff, 0, 1e-6);
    bench_set_value(&dff, 1, 1e-6);
    wire(&dff, 0, 0, 2, 0);
    wire(&dff, 1, 0, 2, 1);
    wire(&dff, 0, 1, 1, 1);
    wire(&dff, 2, 2, 3, 0);
    wire(&dff, 3, 1, 0, 1);
    SpiceLive *live_dff = spice_live_create();
    assert(live_dff);
    step(live_dff, &dff, .001);
    bench_set_value(&dff, 0, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[3] < .0001);
    bench_set_value(&dff, 1, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[3] > .0049);
    bench_set_value(&dff, 0, 1e-6);
    step(live_dff, &dff, .001);
    assert(result.currents[3] > .0049);
    bench_set_value(&dff, 1, 1e-6);
    step(live_dff, &dff, .001);
    bench_set_value(&dff, 1, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[3] < .0001);
    assert(spice_live_time(live_rc) > before);
    Workbench buttons = {0};
    bench_add(&buttons, PART_SOURCE, 300, 300);
    bench_add(&buttons, PART_BUTTON, 500, 300);
    bench_add(&buttons, PART_BUTTON, 700, 300);
    bench_add(&buttons, PART_DFF, 900, 300);
    bench_add(&buttons, PART_RESISTOR, 300, 500);
    bench_add(&buttons, PART_RESISTOR, 500, 500);
    bench_add(&buttons, PART_RESISTOR, 700, 500);
    wire(&buttons, 0, 0, 1, 0);
    wire(&buttons, 0, 0, 2, 0);
    wire(&buttons, 1, 1, 3, 0);
    wire(&buttons, 2, 1, 3, 1);
    wire(&buttons, 3, 0, 4, 0);
    wire(&buttons, 3, 1, 5, 0);
    wire(&buttons, 3, 2, 6, 0);
    for (int i = 4; i < 7; ++i)
        wire(&buttons, i, 1, 0, 1);
    SpiceLive *live_buttons = spice_live_create();
    assert(live_buttons);
    step(live_buttons, &buttons, .001);
    bench_set_on(&buttons, 1, 1);
    step(live_buttons, &buttons, .001);
    bench_set_on(&buttons, 2, 1);
    step(live_buttons, &buttons, .001);
    assert(result.currents[6] > .0049);
    bench_set_on(&buttons, 1, 0);
    step(live_buttons, &buttons, .001);
    assert(result.currents[6] > .0049);
    bench_set_on(&buttons, 2, 0);
    step(live_buttons, &buttons, .001);
    bench_set_on(&buttons, 2, 1);
    step(live_buttons, &buttons, .001);
    assert(result.currents[6] < .0001);
    spice_live_destroy(live_buttons);
    bench_snapshot_clear(&buttons);
    Workbench burn = {0};
    bench_add(&burn, PART_CLOCK, 300, 300);
    bench_add(&burn, PART_LED, 500, 300);
    wire(&burn, 0, 0, 1, 0);
    wire(&burn, 0, 1, 1, 1);
    SpiceLive *live_burn = spice_live_create();
    assert(live_burn);
    step(live_burn, &burn, .0002);
    assert(result.burnt_count == 1 && result.burnt[1]);
    assert(result.currents[1] < 1e-6);
    bench_move_part(&burn, 1, 600, 300);
    step(live_burn, &burn, .0001);
    assert(result.burnt[1]);
    spice_live_destroy(live_burn);
    bench_snapshot_clear(&burn);
    spice_live_destroy(live_dff);
    spice_live_destroy(live_rc);
    bench_snapshot_clear(&dff);
    bench_snapshot_clear(&rc);
    puts("Workbench live capacitor, switch, source-off and DFF memory passed.");
    return 0;
}
