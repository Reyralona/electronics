#include "spice_live.h"
#include "component_registry.h"
#include "netlist.h"
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_live_history_policy(void)
{
    Workbench bench = {0};
    assert(bench_add(&bench, PART_SOURCE, 0, 0) >= 0);
    FILE *file = tmpfile();
    assert(file);
    assert(netlist_write_live(file, &bench, 0) == NETLIST_OK);
    assert(fseek(file, 0, SEEK_SET) == 0);
    char text[4096];
    size_t count = fread(text, 1, sizeof(text) - 1, file);
    text[count] = 0;
    assert(strstr(text, ".save none\n"));
    fclose(file);
    bench_snapshot_clear(&bench);
}

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
/* RC and LED circuit assembled directly by the test. */
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
    /* Wire order and capacitor orientation affect node numbering and pivoting. */
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
/* User's 4-input AND (slot reported 16/11): (A AND B) AND (C AND D) driving resistor + LED.
 * Each switch only connects +5 V to a gate input; nothing pulls an open input down, as a student
 * would build it. Parts: 0 source, 1-4 switches, 5-7 AND, 8 resistor, 9 LED. */
enum { AND4_SWITCHES = 4, AND4_LED = 9 };
static void build_four_input_and(Workbench *bench)
{
    bench_add(bench, PART_SOURCE, 100, 300);
    for (int i = 0; i < AND4_SWITCHES; ++i)
        bench_add(bench, PART_SWITCH, 300, 150 + 100 * i);
    bench_add(bench, PART_AND, 500, 200);
    bench_add(bench, PART_AND, 500, 400);
    bench_add(bench, PART_AND, 700, 300);
    bench_add(bench, PART_RESISTOR, 900, 300);
    bench_add(bench, PART_LED, 1100, 300);
    for (int i = 0; i < AND4_SWITCHES; ++i)
        wire(bench, 0, 0, 1 + i, 0);
    wire(bench, 1, 1, 5, 0);
    wire(bench, 2, 1, 5, 1);
    wire(bench, 3, 1, 6, 0);
    wire(bench, 4, 1, 6, 1);
    wire(bench, 5, 2, 7, 0);
    wire(bench, 6, 2, 7, 1);
    wire(bench, 7, 2, 8, 0);
    wire(bench, 8, 1, 9, 0);
    wire(bench, 9, 1, 0, 1);
}
static int led_lit(void)
{
    const double visible_amps = 1e-3;
    return result.currents[AND4_LED] > visible_amps;
}
/* What a person checks after building it: all off -> dark; turning switches on one at a time keeps
 * it dark until the fourth; any switch turned back off darkens it again. */
static void check_four_input_and(void)
{
    const double settle = .001;
    Workbench bench = {0};
    build_four_input_and(&bench);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench, settle);
    if (led_lit())
        fprintf(stderr, "4AND: LED lit with every switch off (%g A)\n", result.currents[AND4_LED]);
    assert(!led_lit());
    for (int on = 1; on <= AND4_SWITCHES; ++on)
    {
        bench_set_on(&bench, on, 1);
        step(live, &bench, settle);
        assert(led_lit() == (on == AND4_SWITCHES));
    }
    for (int off = 1; off <= AND4_SWITCHES; ++off)
    {
        bench_set_on(&bench, off, 0);
        step(live, &bench, settle);
        assert(!led_lit());
        bench_set_on(&bench, off, 1);
        step(live, &bench, settle);
        assert(led_lit());
    }
    if (getenv("BANCADA_BENCH_LIVE"))
    {
        LARGE_INTEGER frequency, started, finished;
        assert(QueryPerformanceFrequency(&frequency));
        assert(QueryPerformanceCounter(&started));
        step(live, &bench, .02);
        assert(QueryPerformanceCounter(&finished));
        double elapsed = (double)(finished.QuadPart - started.QuadPart) / frequency.QuadPart;
        fprintf(stderr, "4AND live, one batch: simulated 0.020 s in %.3f s wall (%.3fx)\n", elapsed, .02 / elapsed);
    }
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}
/* A person runs a heavy circuit: one batch passes the time limit and fails visibly.
 * The next batch must work again (restarting the session) instead of failing forever. */
static void check_recovers_after_timeout(void)
{
    Workbench rc = {0};
    bench_add(&rc, PART_SOURCE, 300, 100);
    bench_add(&rc, PART_RESISTOR, 500, 100);
    bench_add(&rc, PART_CAPACITOR, 700, 100);
    wire(&rc, 0, 0, 1, 0);
    wire(&rc, 1, 1, 2, 0);
    wire(&rc, 2, 1, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rc, .001);
    const unsigned impatient_ms = 1;
    spice_live_set_timeout_ms(live, impatient_ms);
    assert(!spice_live_step(live, &rc, executable, &options, 1.0, &result));
    printf("Timed-out batch reported: %s\n", result.message);
    spice_live_set_timeout_ms(live, 0);
    step(live, &rc, .001);
    assert(spice_live_restarted(live));
    step(live, &rc, .001);
    assert(!spice_live_restarted(live));
    spice_live_destroy(live);
    bench_snapshot_clear(&rc);
}

/* Samples of consecutive batches must join without gaps or overlaps. */
static double check_samples_follow(const SimLiveSamples *samples, double expected_start)
{
    assert(samples->valid && samples->count > 0 && samples->interval > 0);
    if (expected_start >= 0)
        assert(fabs(samples->start - expected_start) < samples->interval * 1e-6);
    return samples->start + samples->count * samples->interval;
}

/* A person opens the live graph on a charging capacitor and on a clock while the simulation runs. */
static void check_live_probe(void)
{
    Workbench rc = {0};
    bench_add(&rc, PART_SOURCE, 300, 100);
    bench_add(&rc, PART_RESISTOR, 500, 100);
    bench_add(&rc, PART_CAPACITOR, 700, 100);
    bench_set_value(&rc, 1, 1000);
    bench_set_value(&rc, 2, 1e-6);
    wire(&rc, 0, 0, 1, 0);
    wire(&rc, 1, 1, 2, 0);
    wire(&rc, 2, 1, 0, 1);
    SimAutoOptions probed = options;
    probed.step = 1e-3;
    probed.live_probe = (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {2, 0}, 5e-5};
    SpiceLive *live = spice_live_create();
    assert(live);
    double next = -1, last = -1;
    for (int batch = 0; batch < 4; ++batch)
    {
        assert(spice_live_step(live, &rc, executable, &probed, .001, &result));
        next = check_samples_follow(&result.live_samples, next);
        for (int i = 0; i < result.live_samples.count; ++i)
        {
            assert(result.live_samples.values[i] >= last - 1e-9);
            last = result.live_samples.values[i];
        }
    }
    printf("Live probe on RC: last sample %.4f V at t=%.4f s\n", last, next);
    assert(last > 4.5 && last < 5.01);
    /* The opposite case: a point without a solver vector says so instead of drawing zeros. */
    probed.live_probe.target = (Pin){2, 1};
    assert(spice_live_step(live, &rc, executable, &probed, .001, &result));
    assert(!result.live_samples.valid);
    spice_live_destroy(live);
    bench_snapshot_clear(&rc);

    Workbench clock = {0};
    bench_add(&clock, PART_CLOCK, 300, 100);
    bench_add(&clock, PART_RESISTOR, 500, 100);
    bench_set_value(&clock, 0, 100);
    bench_set_on(&clock, 0, 1);
    wire(&clock, 0, 0, 1, 0);
    wire(&clock, 1, 1, 0, 1);
    probed.live_probe = (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {1, 0}, 1e-4};
    live = spice_live_create();
    assert(live);
    double low = 1e9, high = -1e9;
    next = -1;
    for (int batch = 0; batch < 4; ++batch)
    {
        assert(spice_live_step(live, &clock, executable, &probed, .005, &result));
        next = check_samples_follow(&result.live_samples, next);
        for (int i = 0; i < result.live_samples.count; ++i)
        {
            low = fmin(low, result.live_samples.values[i]);
            high = fmax(high, result.live_samples.values[i]);
        }
    }
    printf("Live probe on 100 Hz clock: %.3f V to %.3f V\n", low, high);
    assert(high - low > 4.5);
    spice_live_destroy(live);
    bench_snapshot_clear(&clock);
}

/* A person shorts the outputs through a switch: the bench supply holds its current limit (CC), a 9 V
 * battery has no limit and burns; opening the switch again feeds the resistor normally. */
static void check_short(PartType supply)
{
    static Workbench bench;
    bench_add(&bench, supply, 100, 100);
    bench_add(&bench, PART_SWITCH, 400, 100);
    bench_add(&bench, PART_RESISTOR, 700, 100);
    bench_set_on(&bench, 1, 0);
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 0, 1, 1, 1);
    wire(&bench, 0, 0, 2, 0);
    wire(&bench, 0, 1, 2, 1);
    const double limit = .2;
    if (supply == PART_SOURCE)
        assert(bench_set_control(&bench, 0, limit / SOURCE_CURRENT_LIMIT_MAX));
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench, .001);
    assert(!result.burnt[0]);
    bench_set_on(&bench, 1, 1);
    step(live, &bench, .001);
    if (supply == PART_SOURCE)
        assert(!result.burnt[0] && fabs(-result.source_current - limit) < .005);
    else
        assert(result.burnt[0]);
    bench_set_on(&bench, 1, 0);
    step(live, &bench, .001);
    assert(!result.burnt[0] && fabs(result.currents[2]) > .001);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* 5 V into 100 ohm asks 50 mA: with 100 mA the supply stays at 5 V (CV); lowering the knob to 20 mA
 * holds 20 mA and the output drops to 2 V (CC); raising it back restores 5 V. */
static void check_constant_current(void)
{
    static Workbench bench;
    bench_add(&bench, PART_SOURCE, 100, 100);
    bench_add(&bench, PART_RESISTOR, 400, 100);
    assert(bench_set_value(&bench, 1, 100));
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 0, 1, 1, 1);
    assert(bench_set_control(&bench, 0, .1 / SOURCE_CURRENT_LIMIT_MAX));
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench, .001);
    assert(fabs(fabs(result.currents[1]) - .05) < 1e-3);
    assert(bench_set_control(&bench, 0, .02 / SOURCE_CURRENT_LIMIT_MAX));
    step(live, &bench, .001);
    assert(fabs(fabs(result.currents[1]) - .02) < 1e-3);
    assert(fabs(voltage(live, &bench, 1, 0) - voltage(live, &bench, 1, 1)) < 2.1);
    assert(bench_set_control(&bench, 0, .1 / SOURCE_CURRENT_LIMIT_MAX));
    step(live, &bench, .001);
    assert(fabs(fabs(result.currents[1]) - .05) < 1e-3);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
    puts("Bench supply CV/CC: 50 mA at 5 V, limited to 20 mA, short held at the limit.");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    check_live_history_policy();
    options = (SimAutoOptions){0, {0, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    check_short(PART_SOURCE);
    check_short(PART_BATTERY_9V);
    check_constant_current();
    check_recovers_after_timeout();
    check_live_probe();
    check_four_input_and();
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
    /* The first source powers the logic (hidden supply pins); D and CLK come from their own sources.
     * Output high is the 5 V supply behind the ~50 ohm CMOS output: 5 V / 1050 ohm. */
    Workbench dff = {0};
    bench_add(&dff, PART_SOURCE, 100, 200);
    bench_add(&dff, PART_SOURCE, 300, 200);
    bench_add(&dff, PART_SOURCE, 500, 200);
    bench_add(&dff, PART_DFF, 700, 200);
    bench_add(&dff, PART_RESISTOR, 900, 200);
    bench_set_value(&dff, 0, 5);
    bench_set_value(&dff, 1, 1e-6);
    bench_set_value(&dff, 2, 1e-6);
    wire(&dff, 1, 0, 3, 0);
    wire(&dff, 2, 0, 3, 1);
    wire(&dff, 1, 1, 0, 1);
    wire(&dff, 2, 1, 0, 1);
    wire(&dff, 3, 2, 4, 0);
    wire(&dff, 4, 1, 0, 1);
    SpiceLive *live_dff = spice_live_create();
    assert(live_dff);
    step(live_dff, &dff, .001);
    bench_set_value(&dff, 1, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[4] < .0001);
    bench_set_value(&dff, 2, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[4] > .0045);
    bench_set_value(&dff, 1, 1e-6);
    step(live_dff, &dff, .001);
    assert(result.currents[4] > .0045);
    bench_set_value(&dff, 2, 1e-6);
    step(live_dff, &dff, .001);
    bench_set_value(&dff, 2, 5);
    step(live_dff, &dff, .001);
    assert(result.currents[4] < .0001);
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
    assert(result.currents[6] > .0045 /* 5 V / (1 kOhm + ~50 Ohm CMOS output) */);
    bench_set_on(&buttons, 1, 0);
    step(live_buttons, &buttons, .001);
    assert(result.currents[6] > .0045);
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
    if (getenv("BANCADA_BENCH_LIVE"))
    {
        const int samples = 20;
        const double span = .001;
        LARGE_INTEGER frequency, started, finished;
        assert(QueryPerformanceFrequency(&frequency));
        assert(QueryPerformanceCounter(&started));
        for (int i = 0; i < samples; ++i)
            step(live_rc, &rc, span);
        assert(QueryPerformanceCounter(&finished));
        double elapsed = (double)(finished.QuadPart - started.QuadPart) / frequency.QuadPart;
        fprintf(stderr, "RC live: simulated %.3f s in %.3f s wall (%.3fx)\n", samples * span,
                elapsed, samples * span / elapsed);
        assert(QueryPerformanceCounter(&started));
        step(live_rc, &rc, samples * span);
        assert(QueryPerformanceCounter(&finished));
        elapsed = (double)(finished.QuadPart - started.QuadPart) / frequency.QuadPart;
        fprintf(stderr, "RC live, one batch: simulated %.3f s in %.3f s wall (%.3fx)\n", samples * span,
                elapsed, samples * span / elapsed);
    }
    if (getenv("BANCADA_BENCH_LIVE_LONG"))
    {
        SpiceLive *benchmark = spice_live_create();
        assert(benchmark);
        ULONGLONG previous = GetTickCount64();
        for (int i = 0; i < 500; ++i)
        {
            step(benchmark, &rc, .02);
            if (i % 50 == 49)
            {
                ULONGLONG now = GetTickCount64();
                printf("RC t=%.2f s, wall=%llu ms/50 batches\n", spice_live_time(benchmark),
                       (unsigned long long)(now - previous));
                previous = now;
            }
        }
        spice_live_destroy(benchmark);
    }
    spice_live_destroy(live_dff);
    spice_live_destroy(live_rc);
    bench_snapshot_clear(&dff);
    bench_snapshot_clear(&rc);
    puts("Workbench live capacitor, switch, source-off and DFF memory passed.");
    return 0;
}
