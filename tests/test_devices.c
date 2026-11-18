#include "spice_live.h"
#include "workbench_file.h"
#include "component_devices.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static const char *executable;
static SimAutoOptions options;
static SimAutoResult result;

static void wire(Workbench *bench, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}

static int add(Workbench *bench, PartType type, double value)
{
    int index = bench_add(bench, type, 200 + bench->part_count * 130.0f, 200 + (bench->part_count % 3) * 160.0f);
    assert(index >= 0);
    if (value > 0)
        assert(bench_set_value(bench, index, value));
    return index;
}

static void run(SpiceLive *live, const Workbench *bench, double span)
{
    int success = spice_live_step(live, bench, executable, &options, span, &result);
    if (!success)
        fprintf(stderr, "Device test: %s\n", result.message);
    assert(success);
}

static void rgb_led(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), led = add(&b, PART_RGB_LED, 0);
    for (int color = 0; color < 3; ++color)
    {
        int resistor = add(&b, PART_RESISTOR, 220);
        wire(&b, source, 0, resistor, 0);
        wire(&b, resistor, 1, led, color == 0 ? 0 : color + 1);
    }
    wire(&b, led, 1, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .001);
    assert(result.elements[led][0] > .005 && result.elements[led][1] > .004 && result.elements[led][2] > .002);
    assert(result.elements[led][0] > result.elements[led][2]);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void seven_segment(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), display = add(&b, PART_SEVEN_SEGMENT, 0), resistor = add(&b, PART_RESISTOR, 330);
    wire(&b, source, 0, resistor, 0);
    wire(&b, resistor, 1, display, 0);
    wire(&b, display, 4, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .001);
    assert(result.elements[display][0] > .005);
    for (int segment = 1; segment < COMPONENT_SEGMENTS; ++segment)
        assert(fabs(result.elements[display][segment]) < 1e-6);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void dip_switch(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), dip = add(&b, PART_DIP_SWITCH, 0), load = add(&b, PART_RESISTOR, 1000);
    wire(&b, source, 0, dip, 2);
    wire(&b, dip, 6, load, 0);
    wire(&b, load, 1, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .0005);
    assert(fabs(result.currents[load]) < 1e-6);
    assert(bench_set_states(&b, dip, 1u << 2));
    assert(!bench_set_states(&b, dip, 1u << 4));
    run(live, &b, .0005);
    assert(fabs(result.currents[load] - .005) < 1e-4);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void relay(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), coil_switch = add(&b, PART_SWITCH, 0), relay_part = add(&b, PART_RELAY, 0);
    int open_load = add(&b, PART_RESISTOR, 1000), closed_load = add(&b, PART_RESISTOR, 1000);
    wire(&b, source, 0, coil_switch, 0);
    wire(&b, coil_switch, 1, relay_part, 0);
    wire(&b, relay_part, 1, source, 1);
    wire(&b, source, 0, relay_part, 2);
    wire(&b, relay_part, 3, open_load, 0);
    wire(&b, relay_part, 4, closed_load, 0);
    wire(&b, open_load, 1, source, 1);
    wire(&b, closed_load, 1, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .001);
    assert(fabs(result.currents[open_load]) < 1e-5 && result.currents[closed_load] > .004);
    assert(bench_set_on(&b, coil_switch, 1));
    run(live, &b, .002);
    assert(result.elements[relay_part][0] > .03 && result.elements[relay_part][1] > .9);
    assert(result.currents[open_load] > .004 && fabs(result.currents[closed_load]) < 1e-5);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void motor(PartType type)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), motor_part = add(&b, type, 0);
    wire(&b, source, 0, motor_part, 0);
    wire(&b, motor_part, 1, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .0002);
    double early = result.elements[motor_part][0], inrush = result.elements[motor_part][1];
    run(live, &b, .02);
    assert(result.elements[motor_part][0] > early && result.elements[motor_part][0] > 300);
    assert(fabs(result.elements[motor_part][1]) < fabs(inrush));
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void encoder(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), motor_part = add(&b, PART_ENCODER_MOTOR, 0), load = add(&b, PART_RESISTOR, 10000);
    wire(&b, source, 0, motor_part, 0);
    wire(&b, motor_part, 1, source, 1);
    wire(&b, source, 0, motor_part, 2);
    wire(&b, motor_part, 3, source, 1);
    wire(&b, motor_part, 4, load, 0);
    wire(&b, load, 1, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .01);
    double low = 1, high = 0;
    for (int sample = 0; sample < 60; ++sample)
    {
        run(live, &b, 1e-5);
        low = fmin(low, result.currents[load]);
        high = fmax(high, result.currents[load]);
    }
    assert(low < 1e-4 && high > 3e-4);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void servo(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), servo_part = add(&b, PART_SERVO, 0);
    int upper = add(&b, PART_RESISTOR, 1000), lower = add(&b, PART_RESISTOR, 1000);
    wire(&b, source, 0, servo_part, 1);
    wire(&b, servo_part, 0, source, 1);
    wire(&b, source, 0, upper, 0);
    wire(&b, upper, 1, lower, 0);
    wire(&b, lower, 1, source, 1);
    wire(&b, upper, 1, servo_part, 2);
    SpiceLive *live = spice_live_create();
    run(live, &b, .005);
    assert(result.elements[servo_part][0] > 80 && result.elements[servo_part][0] < 100);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void generator_and_scope(void)
{
    Workbench b = {0};
    int generator = add(&b, PART_FUNCTION_GENERATOR, 1000), scope = add(&b, PART_OSCILLOSCOPE, 0);
    wire(&b, generator, 0, scope, 0);
    wire(&b, generator, 1, scope, 1);
    SpiceLive *live = spice_live_create();
    double low = 10, high = -10;
    for (int sample = 0; sample < 20; ++sample)
    {
        run(live, &b, 1e-4);
        low = fmin(low, result.elements[scope][0]);
        high = fmax(high, result.elements[scope][0]);
    }
    assert(result.powered && low < 1 && high > 4 && high < 5.01);
    assert(bench_set_states(&b, generator, 1));
    run(live, &b, 1e-4);
    assert(spice_live_restarted(live));
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void multimeter(void)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), upper = add(&b, PART_RESISTOR, 1000), lower = add(&b, PART_RESISTOR, 1000);
    int meter = add(&b, PART_MULTIMETER, 0);
    wire(&b, source, 0, upper, 0);
    wire(&b, upper, 1, lower, 0);
    wire(&b, lower, 1, source, 1);
    wire(&b, meter, 1, upper, 1);
    wire(&b, meter, 0, source, 1);
    SpiceLive *live = spice_live_create();
    run(live, &b, .0005);
    assert(fabs(result.elements[meter][0] - 2.5) < .01);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
    Workbench ohm = {0};
    int resistor = add(&ohm, PART_RESISTOR, 1000), ohmmeter = add(&ohm, PART_MULTIMETER, 0);
    assert(bench_set_states(&ohm, ohmmeter, 2));
    wire(&ohm, ohmmeter, 1, resistor, 0);
    wire(&ohm, ohmmeter, 0, resistor, 1);
    live = spice_live_create();
    run(live, &ohm, .0005);
    assert(fabs(result.elements[ohmmeter][0] / COMPONENT_METER_OHMS_TEST_AMPS - 1000) < 5);
    spice_live_destroy(live);
    bench_snapshot_clear(&ohm);
}

static void sensor_module(PartType type, double low_control, double high_control)
{
    Workbench b = {0};
    int source = add(&b, PART_SOURCE, 0), sensor = add(&b, type, 0), load = add(&b, PART_RESISTOR, 10000);
    wire(&b, source, 0, sensor, 0);
    wire(&b, sensor, 2, source, 1);
    wire(&b, sensor, 1, load, 0);
    wire(&b, load, 1, source, 1);
    assert(bench_set_control(&b, sensor, low_control));
    SpiceLive *live = spice_live_create();
    run(live, &b, .0005);
    double low = result.currents[load];
    assert(bench_set_control(&b, sensor, high_control));
    run(live, &b, .0005);
    assert(result.currents[load] > low + 1e-4);
    spice_live_destroy(live);
    bench_snapshot_clear(&b);
}

static void states_roundtrip(void)
{
    Workbench b = {0}, loaded = {0};
    int dip = add(&b, PART_DIP_SWITCH, 0), meter = add(&b, PART_MULTIMETER, 0);
    assert(bench_set_states(&b, dip, 0x9) && bench_set_states(&b, meter, 1));
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &b));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.parts[dip].states == 0x9 && loaded.parts[meter].states == 1);
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&b);
}

int main(int argc, char **argv)
{
    states_roundtrip();
    assert(argc == 2);
    executable = argv[1];
    options = (SimAutoOptions){0, {0, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
    rgb_led();
    seven_segment();
    dip_switch();
    relay();
    motor(PART_DC_MOTOR);
    motor(PART_VIBRATION_MOTOR);
    encoder();
    servo();
    generator_and_scope();
    multimeter();
    sensor_module(PART_PIR, 0, 1);
    sensor_module(PART_GAS, 0, 1);
    sensor_module(PART_IR_OBSTACLE, 0, 1);
    sensor_module(PART_DISTANCE, 1, 0);
    puts("Devices: RGB, 7-segment, DIP, relay, motors, encoder, servo, generator/scope, meter and sensor modules passed.");
    return 0;
}
