#include "spice_live.h"
#include "netlist.h"
#include "component_registry.h"
#include "mcu_board.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* Datasheet protection as a person meets it on the bench (74HC08, TI ratings: 25 mA per output, 7 V supply).
 * Each case also checks its opposite: a correct circuit must stay silent. */
static const char *executable;
static SimAutoOptions options;
static SimAutoResult result;

static void wire(Workbench *bench, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}

static void step(SpiceLive *live, Workbench *bench)
{
    int success = spice_live_step(live, bench, executable, &options, .001, &result);
    if (!success)
        fprintf(stderr, "protection: %s\n", result.message);
    assert(success);
}

/* Supply (part 0) feeding a 74HC08 (part 1) with gate 1 inputs high; 1Y (pin 3) is free for the case. */
static void build_and_chip(Workbench *bench, double volts)
{
    assert(bench_add(bench, PART_SOURCE, 100, 100) == 0);
    assert(bench_add(bench, PART_74HC08, 500, 300) == 1);
    assert(bench_set_value(bench, 0, volts));
    assert(bench_set_control(bench, 0, 1));
    wire(bench, 0, 0, 1, 13);
    wire(bench, 0, 1, 1, 6);
    wire(bench, 1, 13, 1, 0);
    wire(bench, 1, 13, 1, 1);
}

/* Output high into a resistor to GND: 330 ohm is fine, 120 ohm (~29 mA) warns, a wire straight to GND burns. */
static void output_load(double ohms, PartAlert alert, int burns)
{
    static Workbench bench;
    build_and_chip(&bench, 5);
    if (ohms > 0)
    {
        assert(bench_add(&bench, PART_RESISTOR, 800, 300) == 2);
        assert(bench_set_value(&bench, 2, ohms));
        wire(&bench, 1, 2, 2, 0);
        wire(&bench, 2, 1, 0, 1);
    }
    else
        wire(&bench, 1, 2, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    step(live, &bench);
    assert(result.burnt[1] == burns);
    if (burns)
        assert(result.burn_reason[1] == BURN_OVERCURRENT && result.burn_value[1] > .045);
    else
        assert(result.alert[1] == alert);
    if (alert == PART_ALERT_OVERCURRENT && !burns)
        assert(result.alert_pin[1] == 2 && result.alert_value[1] > .025 && result.alert_value[1] < .045);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* Two outputs on one wire, one high and one low: the classic bus contention burns a chip. */
static void bus_contention(void)
{
    static Workbench bench;
    build_and_chip(&bench, 5);
    assert(bench_add(&bench, PART_74HC08, 500, 700) == 2);
    wire(&bench, 0, 0, 2, 13);
    wire(&bench, 0, 1, 2, 6);
    wire(&bench, 0, 1, 2, 0);
    wire(&bench, 1, 2, 2, 2);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    step(live, &bench);
    assert(result.burnt[1] || result.burnt[2]);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* Supply knob turned up: 5 V is silent, 6.5 V is above the recommended 6 V (warning), 8 V burns the chip. */
static void supply_voltage(double volts, PartAlert alert, int burns)
{
    static Workbench bench;
    build_and_chip(&bench, volts);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.burnt[1] == burns);
    assert(burns ? result.burn_reason[1] == BURN_OVERVOLTAGE : result.alert[1] == alert);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* A loose AND symbol (hidden 74HC supply) with its output wired to GND burns like the packaged gate. */
static void loose_gate_short(void)
{
    static Workbench bench;
    assert(bench_add(&bench, PART_SOURCE, 100, 100) == 0);
    assert(bench_add(&bench, PART_AND, 500, 300) == 1);
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 0, 0, 1, 1);
    wire(&bench, 1, 2, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    step(live, &bench);
    assert(result.burnt[1] && result.burn_reason[1] == BURN_OVERCURRENT);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* Segment a of a common-cathode display fed from 5 V: straight it burns, through 330 ohm it lights. */
static void display_segment(double ohms, int burns)
{
    static Workbench bench;
    assert(bench_add(&bench, PART_SOURCE, 100, 100) == 0);
    assert(bench_add(&bench, PART_SEVEN_SEGMENT, 600, 300) == 1);
    assert(bench_set_control(&bench, 0, 1));
    wire(&bench, 1, 4, 0, 1);
    if (ohms > 0)
    {
        assert(bench_add(&bench, PART_RESISTOR, 300, 500) == 2);
        assert(bench_set_value(&bench, 2, ohms));
        wire(&bench, 0, 0, 2, 0);
        wire(&bench, 2, 1, 1, 0);
    }
    else
        wire(&bench, 0, 0, 1, 0);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.burnt[1] == burns);
    assert(burns ? result.burn_reason[1] == BURN_OVERCURRENT : result.elements[1][0] > .005);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* Uno D13 driven HIGH: an LED with 220 ohm is fine; 10 ohm to GND (~140 mA, the resistor itself within
 * 1/4 W) burns the board. A bare LED would burn first, being the weaker part. */
static void uno_pin_load(double ohms, int led, int burns)
{
    static Workbench bench;
    const int d13 = 13, gnd = 22;
    assert(bench_add(&bench, PART_ARDUINO_UNO, 300, 300) == 0);
    assert(bench_add(&bench, PART_RESISTOR, 700, 600) == 1);
    assert(bench_set_value(&bench, 1, ohms));
    bench.parts[0].io_mode[d13] = MCU_OUTPUT;
    bench.parts[0].io_level[d13] = 1;
    wire(&bench, 0, d13, 1, 0);
    if (led)
    {
        assert(bench_add(&bench, PART_LED, 900, 300) == 2);
        wire(&bench, 1, 1, 2, 0);
        wire(&bench, 2, 1, 0, gnd);
    }
    else
        wire(&bench, 1, 1, 0, gnd);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.burnt[0] == burns && !result.burnt[1]);
    assert(burns ? result.burn_reason[0] == BURN_OVERCURRENT : fabs(result.currents[1]) > .005);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* A 5 V signal on an ESP32 input burns it (not 5 V tolerant); 3.3 V is fine. */
static void esp32_input(double volts, int burns)
{
    static Workbench bench;
    const int gpio2 = 0, gnd = 21;
    assert(bench_add(&bench, PART_ESP32, 300, 300) == 0);
    assert(bench_add(&bench, PART_SOURCE, 900, 300) == 1);
    assert(bench_set_value(&bench, 1, volts));
    wire(&bench, 1, 0, 0, gpio2);
    wire(&bench, 1, 1, 0, gnd);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.burnt[0] == burns);
    if (burns)
        assert(result.burn_reason[0] == BURN_PIN_VOLTAGE);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* Button to +5 V on input A of an AND, no resistor: released, A floats (warning on pin A). Pressed, A is
 * driven. With a 10 kOhm pull-down the released button is fine too. */
static void button_without_pulldown(void)
{
    static Workbench bench;
    assert(bench_add(&bench, PART_SOURCE, 100, 100) == 0);
    assert(bench_add(&bench, PART_BUTTON, 400, 100) == 1);
    assert(bench_add(&bench, PART_AND, 700, 300) == 2);
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 1, 1, 2, 0);
    wire(&bench, 0, 0, 2, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.alert[2] == PART_ALERT_FLOATING_INPUT && result.alert_pin[2] == 0);
    assert(bench_set_on(&bench, 1, 1));
    step(live, &bench);
    assert(result.alert[2] == PART_ALERT_NONE);
    assert(bench_set_on(&bench, 1, 0));
    assert(bench_add(&bench, PART_RESISTOR, 700, 600) == 3);
    assert(bench_set_value(&bench, 3, 10000));
    wire(&bench, 2, 0, 3, 0);
    wire(&bench, 3, 1, 0, 1);
    step(live, &bench);
    assert(result.alert[2] == PART_ALERT_NONE);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

/* A 74HC08 using only gate 1: the three unused gates stay silent (no warning flood). */
static void unused_quad_gates_silent(void)
{
    static Workbench bench;
    build_and_chip(&bench, 5);
    assert(bench_add(&bench, PART_RESISTOR, 800, 300) == 2);
    wire(&bench, 1, 2, 2, 0);
    wire(&bench, 2, 1, 0, 1);
    SpiceLive *live = spice_live_create();
    assert(live);
    step(live, &bench);
    assert(result.alert[1] == PART_ALERT_NONE);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    executable = argv[1];
    options = (SimAutoOptions){0, {0, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    output_load(330, PART_ALERT_NONE, 0);
    output_load(120, PART_ALERT_OVERCURRENT, 0);
    output_load(0, PART_ALERT_OVERCURRENT, 1);
    bus_contention();
    supply_voltage(5, PART_ALERT_NONE, 0);
    supply_voltage(6.5, PART_ALERT_OVERVOLTAGE, 0);
    supply_voltage(8, PART_ALERT_NONE, 1);
    loose_gate_short();
    display_segment(330, 0);
    display_segment(0, 1);
    uno_pin_load(220, 1, 0);
    uno_pin_load(10, 0, 1);
    esp32_input(3.3, 0);
    esp32_input(5, 1);
    button_without_pulldown();
    unused_quad_gates_silent();
    puts("Chip protection: 330 ohm silent, 120 ohm warns, short and bus contention burn, 6.5 V warns, 8 V burns, bare segment burns, Uno pin and ESP32 5 V input burn, floating input warns.");
    return 0;
}
