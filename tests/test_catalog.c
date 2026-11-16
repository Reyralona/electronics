#include "workbench_file.h"
#include "spice_process.h"
#include "component_registry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void gate_case(PartType type, int a, int b, const char *executable)
{
    /* Source 0 powers the logic (first source = hidden supply pins); sources 1 and 2 drive the inputs. */
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 100, 300);
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, PART_SOURCE, 300, 400);
    bench_add(&bench, type, 600, 300);
    bench_add(&bench, PART_RESISTOR, 800, 300);
    bench_set_value(&bench, 0, 5);
    bench_set_value(&bench, 1, a ? 5 : 1e-6);
    bench_set_value(&bench, 2, b ? 5 : 1e-6);
    bench_toggle_wire(&bench, (Pin){1, 0}, (Pin){3, 0});
    bench_toggle_wire(&bench, (Pin){2, 0}, (Pin){3, 1});
    bench_toggle_wire(&bench, (Pin){1, 1}, (Pin){0, 1});
    bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1});
    bench_toggle_wire(&bench, (Pin){3, 2}, (Pin){4, 0});
    bench_toggle_wire(&bench, (Pin){4, 1}, (Pin){0, 1});
    double current, currents[WORKBENCH_MAX_COMPONENTS];
    char message[256];
    int success = spice_dc_analyze(&bench, executable, &current, currents, message, sizeof(message));
    if (!success)
        fprintf(stderr, "%s\n", message);
    assert(success);
    int expected = type == PART_NOR ? !(a || b) : a == b;
    assert(expected ? currents[4] > .004 : fabs(currents[4]) < 1e-6);
    bench_snapshot_clear(&bench);
}

static void quad_case(PartType type, int a, int b, const char *executable)
{
    static const int pins[4][3] = {{0, 1, 2}, {3, 4, 5}, {8, 9, 7}, {11, 12, 10}};
    Workbench bench = {0};
    for (int i = 0; i < 3; ++i)
        bench_add(&bench, PART_SOURCE, 300, 200 + 150 * i);
    bench_add(&bench, type, 600, 300);
    bench_set_value(&bench, 1, a ? 5 : 1e-6);
    bench_set_value(&bench, 2, b ? 5 : 1e-6);
    bench_toggle_wire(&bench, (Pin){0, 1}, (Pin){1, 1});
    bench_toggle_wire(&bench, (Pin){0, 1}, (Pin){2, 1});
    bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){3, 13});
    bench_toggle_wire(&bench, (Pin){0, 1}, (Pin){3, 6});
    for (int g = 0; g < 4; ++g)
    {
        bench_add(&bench, PART_RESISTOR, 800, 200 + 100 * g);
        bench_toggle_wire(&bench, (Pin){1, 0}, (Pin){3, pins[g][0]});
        bench_toggle_wire(&bench, (Pin){2, 0}, (Pin){3, pins[g][1]});
        bench_toggle_wire(&bench, (Pin){3, pins[g][2]}, (Pin){4 + g, 0});
        bench_toggle_wire(&bench, (Pin){4 + g, 1}, (Pin){0, 1});
    }
    double current, currents[WORKBENCH_MAX_COMPONENTS];
    char message[256];
    assert(spice_dc_analyze(&bench, executable, &current, currents, message, sizeof(message)));
    int high = type == PART_74HC08 ? a && b : type == PART_74HC32 ? a || b : a != b;
    for (int g = 0; g < 4; ++g)
        assert(high ? currents[4 + g] > .0047 : fabs(currents[4 + g]) < 1e-6);
    bench_set_value(&bench, 0, 1e-6);
    assert(spice_dc_analyze(&bench, executable, &current, currents, message, sizeof(message)));
    for (int g = 0; g < 4; ++g)
        assert(fabs(currents[4 + g]) < 1e-6);
    bench_snapshot_clear(&bench);
}

/* "Fonte de 2 V, AND com as duas entradas na fonte, LED direto na saida": the gate is powered by the source
 * (hidden supply pins), so 2 V is a valid high and the output is ~2 V, not an invented 5 V. Opposite cases:
 * the same LED without resistor on a 5 V supply exceeds its 30 mA limit; below 2 V the 74HC-like gate is off. */
static double gate_led_current(double supply, const char *executable)
{
    Workbench bench = {0};
    bench_add(&bench, PART_SOURCE, 300, 200);
    bench_add(&bench, PART_AND, 600, 300);
    bench_add(&bench, PART_LED, 800, 300);
    bench_set_value(&bench, 0, supply);
    bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&bench, (Pin){0, 0}, (Pin){1, 1});
    bench_toggle_wire(&bench, (Pin){1, 2}, (Pin){2, 0});
    bench_toggle_wire(&bench, (Pin){2, 1}, (Pin){0, 1});
    double current, currents[WORKBENCH_MAX_COMPONENTS];
    char message[256];
    int success = spice_dc_analyze(&bench, executable, &current, currents, message, sizeof(message));
    if (!success)
        fprintf(stderr, "%s\n", message);
    assert(success);
    bench_snapshot_clear(&bench);
    return currents[2];
}

static void test_gate_follows_supply(const char *executable)
{
    const double led_limit = .030;
    double at_2v = gate_led_current(2, executable);
    double at_5v = gate_led_current(5, executable);
    double at_1v5 = gate_led_current(1.5, executable);
    assert(at_2v >= 0 && at_2v < led_limit);
    assert(at_5v > led_limit);
    assert(fabs(at_1v5) < 1e-6);
}

int main(int argc, char **argv)
{
    assert(component_info(PART_POTENTIOMETER)->pin_count == 3);
    assert(component_info(PART_BUTTON)->pin_count == 2);
    Workbench pot = {0}, loaded = {0};
    bench_add(&pot, PART_SOURCE, 300, 200);
    bench_add(&pot, PART_POTENTIOMETER, 500, 200);
    bench_add(&pot, PART_RESISTOR, 700, 200);
    bench_set_value(&pot, 2, 1e6);
    assert(bench_set_control(&pot, 1, .25));
    assert(!bench_set_control(&pot, 1, NAN));
    assert(!bench_set_control(&pot, 1, 1.01));
    bench_toggle_wire(&pot, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&pot, (Pin){0, 1}, (Pin){1, 2});
    bench_toggle_wire(&pot, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&pot, (Pin){2, 1}, (Pin){0, 1});
    FILE *file = tmpfile();
    assert(file && workbench_write(file, &pot));
    rewind(file);
    assert(workbench_read(file, &loaded) && loaded.parts[1].control == .25);
    fclose(file);
    if (argc > 1)
    {
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 2; ++b)
            {
                quad_case(PART_74HC08, a, b, argv[1]);
                quad_case(PART_74HC32, a, b, argv[1]);
                quad_case(PART_74HC86, a, b, argv[1]);
                gate_case(PART_NOR, a, b, argv[1]);
                gate_case(PART_XNOR, a, b, argv[1]);
            }
        test_gate_follows_supply(argv[1]);
        double current, currents[WORKBENCH_MAX_COMPONENTS];
        char message[256];
        assert(spice_dc_analyze(&pot, argv[1], &current, currents, message, sizeof(message)));
        assert(currents[2] > 3.74e-6 && currents[2] < 3.751e-6);
        bench_set_control(&pot, 1, .75);
        assert(spice_dc_analyze(&pot, argv[1], &current, currents, message, sizeof(message)));
        assert(currents[2] > 1.24e-6 && currents[2] < 1.251e-6);
        Workbench light = {0};
        bench_add(&light, PART_SOURCE, 300, 200);
        bench_add(&light, PART_RESISTOR, 500, 200);
        bench_add(&light, PART_LED, 700, 200);
        bench_set_value(&light, 1, 470);
        bench_toggle_wire(&light, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&light, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&light, (Pin){2, 1}, (Pin){0, 1});
        static SpiceTrace light_trace;
        assert(spice_transient_analyze(&light, argv[1], (Pin){2, 0}, 1e-5, .001,
                                       &light_trace, message, sizeof(message)));
        int bright = light_trace.led_brightness[light_trace.count - 1][2];
        assert(bright > 0 && bright < 255);
        bench_set_value(&light, 1, 4700);
        assert(spice_transient_analyze(&light, argv[1], (Pin){2, 0}, 1e-5, .001,
                                       &light_trace, message, sizeof(message)));
        assert(light_trace.led_brightness[light_trace.count - 1][2] > 0);
        assert(light_trace.led_brightness[light_trace.count - 1][2] < bright);
        bench_snapshot_clear(&light);
        Workbench button = {0};
        bench_add(&button, PART_SOURCE, 300, 200);
        bench_add(&button, PART_BUTTON, 500, 200);
        bench_add(&button, PART_RESISTOR, 700, 200);
        bench_toggle_wire(&button, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&button, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&button, (Pin){2, 1}, (Pin){0, 1});
        assert(spice_dc_analyze(&button, argv[1], &current, currents, message, sizeof(message)));
        assert(fabs(currents[2]) < 1e-8);
        bench_set_on(&button, 1, 1);
        assert(spice_dc_analyze(&button, argv[1], &current, currents, message, sizeof(message)));
        assert(currents[2] > .0049 && currents[2] < .0051);
        file = tmpfile();
        assert(file && workbench_write(file, &button));
        rewind(file);
        assert(workbench_read(file, &loaded) && !loaded.parts[1].on);
        fclose(file);
        bench_snapshot_clear(&button);
    }
    bench_snapshot_clear(&loaded);
    bench_snapshot_clear(&pot);
    return 0;
}
