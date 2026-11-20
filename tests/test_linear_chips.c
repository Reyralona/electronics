#include "spice_live.h"
#include "component_registry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Linear ICs as a person meets them on a breadboard; expected values come from the TI datasheet formulas
 * and levels (docs/datasheets/ANALOGICOS_E_LIMITES.md). */
static const char *executable;
static SimAutoResult result;

typedef struct
{
    Workbench bench;
    float next_x;
} Rig;

static void wire(Rig *rig, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(&rig->bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}

/* Each new part sits apart from the others, so no terminals touch by accident. */
static int add(Rig *rig, PartType type, double value)
{
    rig->next_x += 220;
    int part = bench_add(&rig->bench, type, rig->next_x, 400);
    assert(part >= 0);
    if (value > 0)
        assert(bench_set_value(&rig->bench, part, value));
    return part;
}

/* A bench supply at `volts` with its current limit at the maximum; part 0 is the main 5 V supply. */
static int supply(Rig *rig, double volts)
{
    int source = add(rig, PART_SOURCE, volts);
    assert(bench_set_control(&rig->bench, source, 1));
    if (source)
        wire(rig, source, 1, 0, 1);
    return source;
}

static int resistor(Rig *rig, double ohms, int a, int ap, int b, int bp)
{
    int part = add(rig, PART_RESISTOR, ohms);
    wire(rig, a, ap, part, 0);
    wire(rig, part, 1, b, bp);
    return part;
}

static void step(SpiceLive *live, Rig *rig, double span)
{
    SimAutoOptions options = {0, {0, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    int success = spice_live_step(live, &rig->bench, executable, &options, span, &result);
    if (!success)
        fprintf(stderr, "linear chips: %s\n", result.message);
    assert(success && result.burnt_count == 0);
}

static double volts(SpiceLive *live, Rig *rig, int part, int pin)
{
    double value;
    char message[256];
    assert(spice_live_voltage(live, &rig->bench, (Pin){part, pin}, &value, message, sizeof(message)));
    return value;
}

/* NE555 astable, RA = 1 kOhm, RB = 10 kOhm, C = 100 nF: f = 1.44 / ((RA + 2 RB) C) = 686 Hz (datasheet
 * 6.3). The output high level sits about 1.7 V under VCC. RESET to GND keeps the output low. */
static void timer_astable(int reset_low)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, 5);
    int timer = add(&rig, PART_NE555, 0), cap = add(&rig, PART_CAPACITOR, 100e-9);
    wire(&rig, timer, 7, 0, 0);
    wire(&rig, timer, 0, 0, 1);
    wire(&rig, timer, 3, 0, reset_low ? 1 : 0);
    resistor(&rig, 1000, 0, 0, timer, 6);
    resistor(&rig, 10000, timer, 6, timer, 5);
    wire(&rig, timer, 5, timer, 1);
    wire(&rig, timer, 5, cap, 0);
    wire(&rig, cap, 1, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .005);
    const double sample = 2e-5, window = .02;
    int rises = 0, previous = volts(live, &rig, timer, 2) > 2;
    double highest = 0;
    for (double t = 0; t < window; t += sample)
    {
        step(live, &rig, sample);
        double out = volts(live, &rig, timer, 2);
        highest = fmax(highest, out);
        rises += out > 2 && !previous;
        previous = out > 2;
    }
    if (reset_low)
        assert(rises == 0 && highest < .5);
    else
    {
        double expected = 1.44 / ((1000 + 2 * 10000) * 100e-9) * window;
        assert(fabs(rises - expected) <= expected * .1 + 1);
        assert(fabs(highest - (5 - 1.7)) < .15);
    }
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* LM358 non-inverting amplifier, gain 1 + 10k/10k = 2: 1 V in gives 2 V out; 3 V in would need 6 V and
 * stops at V+ - 1.5 V = 3.5 V. */
static void op_amp_gain(double input, double expected)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, 5);
    int signal = supply(&rig, input), amp = add(&rig, PART_LM358, 0);
    wire(&rig, amp, 7, 0, 0);
    wire(&rig, amp, 3, 0, 1);
    wire(&rig, signal, 0, amp, 2);
    resistor(&rig, 10000, amp, 0, amp, 1);
    resistor(&rig, 10000, amp, 1, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .001);
    assert(fabs(volts(live, &rig, amp, 0) - expected) < .02);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* LM393 with a 10 kOhm pull-up: IN- at 2.5 V (divider). IN+ at 2 V pulls the output low; 3 V releases it. */
static void comparator_switches(double plus, int high)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, 5);
    int signal = supply(&rig, plus), cmp = add(&rig, PART_LM393, 0);
    wire(&rig, cmp, 7, 0, 0);
    wire(&rig, cmp, 3, 0, 1);
    wire(&rig, signal, 0, cmp, 2);
    resistor(&rig, 10000, 0, 0, cmp, 1);
    resistor(&rig, 10000, cmp, 1, 0, 1);
    resistor(&rig, 10000, 0, 0, cmp, 0);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .001);
    double out = volts(live, &rig, cmp, 0);
    assert(high ? out > 4.9 : out < .2);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* LM7805 with 100 ohm: 9 V in gives 5 V out; 6 V in is under the 2 V dropout and gives 4 V. */
static void regulator_output(double input, double expected)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, input);
    int reg = add(&rig, PART_LM7805, 0);
    wire(&rig, 0, 0, reg, 0);
    wire(&rig, reg, 1, 0, 1);
    resistor(&rig, 100, reg, 2, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .001);
    assert(fabs(volts(live, &rig, reg, 2) - expected) < .03);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* ULN2003A channel 1 sinking a 100 ohm load from 5 V: input high gives about 40 mA (about 1 V saturation),
 * input low leaves the load off. */
static void darlington_sinks(int input_high)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, 5);
    int uln = add(&rig, PART_ULN2003, 0);
    wire(&rig, uln, 7, 0, 1);
    wire(&rig, uln, 8, 0, 0);
    wire(&rig, uln, 0, 0, input_high ? 0 : 1);
    int load = resistor(&rig, 100, 0, 0, uln, 15);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .001);
    double amps = fabs(result.currents[load]);
    assert(input_high ? amps > .033 && amps < .043 : amps < 1e-4);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

/* L293D channel 1 from a 9 V motor supply into 1 kOhm: EN and 1A high drive 1Y about 1.4 V under VCC2
 * (less at this light load); EN low leaves 1Y off. */
static void half_bridge_drives(int enabled)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    supply(&rig, 5);
    int motor = supply(&rig, 9), drv = add(&rig, PART_L293D, 0);
    wire(&rig, drv, 15, 0, 0);
    wire(&rig, drv, 7, motor, 0);
    wire(&rig, drv, 3, 0, 1);
    wire(&rig, drv, 0, 0, enabled ? 0 : 1);
    wire(&rig, drv, 1, 0, 0);
    int load = resistor(&rig, 1000, drv, 2, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &rig, .001);
    double out = volts(live, &rig, drv, 2);
    assert(enabled ? out > 9 - 1.8 && out < 9 - .7 : fabs(result.currents[load]) < 1e-4);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

static void loaded_supply_conserves_current(PartType type, double limit)
{
    static Rig rig;
    memset(&rig, 0, sizeof(rig));
    int source = supply(&rig, 5);
    assert(bench_set_control(&rig.bench, source, limit / 3));
    int chip = add(&rig, type, 0);
    wire(&rig, source, 0, chip, 7);
    wire(&rig, source, 1, chip, type == PART_NE555 ? 0 : 3);
    int output = type == PART_NE555 ? 2 : 0;
    if (type == PART_NE555)
    {
        wire(&rig, source, 0, chip, 3);
        wire(&rig, source, 1, chip, 1);
        wire(&rig, source, 1, chip, 5);
    }
    else
    {
        wire(&rig, source, 0, chip, 2);
        wire(&rig, source, 1, chip, 1);
        wire(&rig, source, 1, chip, 4);
        wire(&rig, source, 1, chip, 5);
    }
    resistor(&rig, 100, chip, output, source, 1);
    SpiceLive *live = spice_live_create();
    step(live, &rig, .002);
    double load = volts(live, &rig, chip, output) / 100;
    assert(load > .001 && -result.source_current >= load - 1e-6);
    if (limit < .02)
    {
        /* The existing CV/CC stage has finite gain, allowing a small current excess. */
        assert(load <= limit + .0003);
        assert(volts(live, &rig, chip, 7) < 4.99);
    }
    else
        assert(volts(live, &rig, chip, 7) > 4.99);
    spice_live_destroy(live);
    bench_snapshot_clear(&rig.bench);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    loaded_supply_conserves_current(PART_NE555, .01);
    loaded_supply_conserves_current(PART_NE555, 1);
    loaded_supply_conserves_current(PART_LM358, .01);
    loaded_supply_conserves_current(PART_LM358, 1);
    timer_astable(0);
    timer_astable(1);
    op_amp_gain(1, 2);
    op_amp_gain(3, 3.5);
    comparator_switches(2, 0);
    comparator_switches(3, 1);
    regulator_output(9, 5);
    regulator_output(6, 4);
    darlington_sinks(1);
    darlington_sinks(0);
    half_bridge_drives(1);
    half_bridge_drives(0);
    puts("Linear ICs: 555 astable at 686 Hz and reset, LM358 gain and clamp, LM393 switching, "
         "LM7805 regulation and dropout, ULN2003A sink, L293D drive and disable passed.");
    return 0;
}
