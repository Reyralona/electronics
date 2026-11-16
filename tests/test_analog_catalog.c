#include "component_registry.h"
#include "workbench_file.h"
#include "workbench_history.h"
#include "spice_process.h"
#include "spice_live.h"
#include "netlist.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static const char *executable;
static void wire(Workbench *bench, int a, int ap, int b, int bp)
{
    assert(bench_toggle_wire(bench, (Pin){a, ap}, (Pin){b, bp}) == 1);
}

static double currents[WORKBENCH_MAX_COMPONENTS];
static void analyze(Workbench *bench)
{
    double source;
    char message[512];
    int success = spice_dc_analyze(bench, executable, &source, currents, message, sizeof(message));
    if (!success)
        fprintf(stderr, "%s\n", message);
    assert(success);
}

static void batteries(void)
{
    static const PartType types[] = {PART_BATTERY_9V, PART_BATTERY_AA, PART_BATTERY_COIN};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
    {
        Workbench bench = {0};
        bench_add(&bench, types[i], 300, 200);
        bench_add(&bench, PART_RESISTOR, 600, 200);
        bench_set_value(&bench, 1, 100);
        wire(&bench, 0, 0, 1, 0);
        wire(&bench, 0, 1, 1, 1);
        analyze(&bench);
        double expected = bench.parts[0].value / (100 + component_source_resistance(types[i]));
        assert(fabs(currents[1] - expected) < expected * 1e-5);
        bench_set_on(&bench, 0, 0);
        analyze(&bench);
        assert(fabs(currents[1]) < 1e-9);
        bench_snapshot_clear(&bench);
    }
}

static void diode(void)
{
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, PART_RESISTOR, 500, 200);
    bench_add(&bench, PART_DIODE, 700, 200);
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 1, 1, 2, 0);
    wire(&bench, 2, 1, 0, 1);
    analyze(&bench);
    assert(currents[2] > .003 && currents[2] < .0045);
    assert(fabs(currents[1] - currents[2]) < 1e-7);
    assert(bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){2, 0}) == 2);
    assert(bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1}) == 2);
    wire(&bench, 1, 1, 2, 1);
    wire(&bench, 2, 0, 0, 1);
    analyze(&bench);
    assert(fabs(currents[2]) < 1e-9);
    bench_snapshot_clear(&bench);
}

static void transistor(PartType type)
{
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, type, 500, 300);
    bench_add(&bench, PART_RESISTOR, 700, 200);
    bench_add(&bench, PART_RESISTOR, 700, 400);
    bench_add(&bench, PART_SOURCE, 300, 500);
    bench_set_value(&bench, 3, 100000);
    int emitter_supply = type == PART_PNP ? 0 : 1;
    wire(&bench, 1, 0, 0, emitter_supply);
    wire(&bench, 1, 2, 2, 0);
    wire(&bench, 2, 1, 0, 1 - emitter_supply);
    wire(&bench, 1, 1, 3, 0);
    wire(&bench, 3, 1, 4, 0);
    wire(&bench, 4, 1, 0, 1);
    bench_set_value(&bench, 4, type == PART_PNP ? 5 : 1e-6);
    analyze(&bench);
    assert(fabs(currents[1]) < 1e-8);
    bench_set_value(&bench, 4, type == PART_PNP ? 4 : 1);
    analyze(&bench);
    double active = fabs(currents[1]);
    assert(active > .0002 && active < .0006);
    assert(type == PART_PNP ? currents[1] < 0 : currents[1] > 0);
    bench_set_value(&bench, 3, 1000);
    bench_set_value(&bench, 4, type == PART_PNP ? 1e-6 : 5);
    analyze(&bench);
    assert(fabs(currents[1]) > .0045 && fabs(currents[1]) < .0051);
    bench_snapshot_clear(&bench);
}

static void environment_and_persistence(void)
{
    static const PartType types[] = {PART_LDR, PART_FLEX, PART_FORCE, PART_TILT,
                                    PART_PHOTODIODE, PART_AMBIENT_LIGHT, PART_TEMPERATURE, PART_SOIL};
    Workbench bench = {0}, loaded = {0};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
    {
        assert(bench_add(&bench, types[i], 300 + 100 * i, 300) == (int)i);
        assert(bench_set_control(&bench, (int)i, .27));
        assert(!bench_set_control(&bench, (int)i, NAN));
        assert(!bench_set_control(&bench, (int)i, 1.01));
    }
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.part_count == bench.part_count);
    for (int i = 0; i < loaded.part_count; ++i)
    {
        assert(loaded.parts[i].type == types[i]);
        assert(loaded.parts[i].control == .27);
    }
    double dim = component_effective_value(&loaded.parts[0]);
    bench_set_control(&loaded, 0, .9);
    assert(component_effective_value(&loaded.parts[0]) < dim);
    bench_set_control(&loaded, 1, 0);
    double flat = component_effective_value(&loaded.parts[1]);
    bench_set_control(&loaded, 1, 1);
    assert(component_effective_value(&loaded.parts[1]) == 4 * flat);
    bench_set_control(&loaded, 2, 0);
    assert(component_effective_value(&loaded.parts[2]) == 1e8);
    bench_set_control(&loaded, 2, 1);
    assert(component_effective_value(&loaded.parts[2]) < 1000);
    bench_set_control(&loaded, 3, 0);
    assert(component_effective_on(&loaded.parts[3]));
    bench_set_control(&loaded, 3, 1);
    assert(!component_effective_on(&loaded.parts[3]));
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&bench);
}

static void sensor_live(PartType type)
{
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, type, 600, 200);
    bench_add(&bench, PART_RESISTOR, 900, 200);
    wire(&bench, 0, 0, 1, type == PART_PHOTODIODE ? 1 : 0);
    wire(&bench, 1, type == PART_PHOTODIODE ? 0 : 1, 2, 0);
    wire(&bench, 2, 1, 0, 1);
    bench_set_control(&bench, 1, 0);
    SpiceLive *live = spice_live_create();
    assert(live);
    SimAutoOptions options = {0, {0, 0}, 1e-5, .001, burn_limits_default()};
    SimAutoResult result;
    assert(spice_live_step(live, &bench, executable, &options, .001, &result));
    double initial = result.currents[2];
    double time = spice_live_time(live);
    bench_set_control(&bench, 1, 1);
    assert(spice_live_step(live, &bench, executable, &options, .001, &result));
    assert(!spice_live_restarted(live));
    assert(spice_live_time(live) > time);
    if (type == PART_FLEX || type == PART_TILT)
        assert(result.currents[2] < initial * .6);
    else
        assert(result.currents[2] > initial + 1e-5);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

static void new_types_roundtrip(void)
{
    Workbench bench = {0}, loaded = {0};
    for (PartType type = PART_DIODE; (size_t)type < component_registry_count(); ++type)
    {
        int index = bench_add(&bench, type, 300 + (type % 6) * 120, 200 + (type / 6) * 100);
        assert(index >= 0);
        assert(component_from_legacy(type, 23) == NULL);
        assert(component_from_legacy(type, 24)->type == type);
    }
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &bench));
    rewind(file);
    assert(workbench_read(file, &loaded));
    fclose(file);
    assert(loaded.part_count == bench.part_count);
    for (int i = 0; i < loaded.part_count; ++i)
    {
        assert(loaded.parts[i].type == bench.parts[i].type);
        assert(loaded.parts[i].value == bench.parts[i].value);
        assert(loaded.parts[i].control == bench.parts[i].control);
    }
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&bench);
}

static void powered_sensor(PartType type)
{
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, type, 600, 200);
    bench_add(&bench, PART_RESISTOR, 900, 200);
    bench_set_value(&bench, 2, 1000000);
    wire(&bench, 0, 0, 1, 0);
    wire(&bench, 0, 1, 1, 2);
    wire(&bench, 1, 1, 2, 0);
    wire(&bench, 2, 1, 0, 1);
    analyze(&bench);
    if (type == PART_TEMPERATURE)
        assert(currents[2] > .749e-6 && currents[2] < .751e-6);
    else
        assert(currents[2] > 1e-6 && currents[2] < 2e-6);
    SpiceLive *live = spice_live_create();
    assert(live);
    SimAutoOptions options = {0, {0, 0}, 1e-5, .001, burn_limits_default()};
    SimAutoResult result;
    char message[512];
    int success = spice_live_step(live, &bench, executable, &options, .001, &result);
    if (!success)
        fprintf(stderr, "Powered live: %s\n", result.message);
    assert(success);
    bench_set_control(&bench, 1, 1);
    assert(spice_live_step(live, &bench, executable, &options, .001, &result));
    assert(!spice_live_restarted(live));
    double output;
    assert(spice_live_voltage(live, &bench, (Pin){1, 1}, &output, message, sizeof(message)));
    if (type == PART_TEMPERATURE)
        assert(output > 1.749 && output < 1.751);
    else
        assert(output > 4.5 && output < 4.55);
    bench_set_value(&bench, 0, 2);
    assert(spice_live_step(live, &bench, executable, &options, .001, &result));
    if (type == PART_TEMPERATURE)
        assert(fabs(result.currents[2]) < 1e-9);
    bench_set_value(&bench, 0, 5);
    bench_set_value(&bench, 2, 100);
    assert(spice_live_step(live, &bench, executable, &options, .001, &result));
    if (type == PART_TEMPERATURE)
        assert(fabs(result.currents[2] - .00025) < 1e-7);
    spice_live_destroy(live);
    bench_snapshot_clear(&bench);
}

int main(int argc, char **argv)
{
    environment_and_persistence();
    new_types_roundtrip();
    if (argc == 2)
    {
        executable = argv[1];
        powered_sensor(PART_TEMPERATURE);
        powered_sensor(PART_SOIL);
        batteries();
        diode();
        transistor(PART_NPN);
        transistor(PART_PNP);
        sensor_live(PART_LDR);
        sensor_live(PART_FLEX);
        sensor_live(PART_FORCE);
        sensor_live(PART_TILT);
        sensor_live(PART_PHOTODIODE);
        sensor_live(PART_AMBIENT_LIGHT);
    }
    puts("Analog catalog: battery loading, diode reverse bias, BJT regions, environment persistence and live controls passed.");
    return 0;
}
