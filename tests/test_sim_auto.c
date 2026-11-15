#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sim_auto.h"
#include "netlist.h"

static void wire(Workbench *b, int ca, int pa, int cb, int pb)
{
    assert(bench_toggle_wire(b, (Pin){ca, pa}, (Pin){cb, pb}) == 1);
}

static void test_limits(void)
{
    BurnLimits limits = burn_limits_default();
    Part led = {.type = PART_LED};
    Part resistor = {.type = PART_RESISTOR, .value = 1000};
    Part source = {.type = PART_SOURCE, .on = 1, .value = 5};
    BurnReason reason;
    double measured;
    assert(burn_stress(&led, 0.020, &limits, &reason, &measured) < 1 && reason == BURN_OVERCURRENT);
    assert(burn_stress(&led, 0.050, &limits, &reason, &measured) > 1 && measured == 0.050);
    assert(burn_stress(&led, -1e-12, &limits, &reason, &measured) < 1);
    assert(burn_stress(&resistor, 0.005, &limits, &reason, &measured) < 1 && reason == BURN_OVERPOWER);
    resistor.value = 10;
    assert(burn_stress(&resistor, 0.5, &limits, &reason, &measured) > 1 && fabs(measured - 2.5) < 1e-12);
    assert(burn_stress(&source, 100, &limits, &reason, &measured) == 0 && reason == BURN_NONE);
    assert(burn_stress(&led, NAN, &limits, &reason, &measured) == 0);
}

static void test_transforms(void)
{
    Workbench b = {0}, out;
    unsigned char open[WORKBENCH_MAX_COMPONENTS] = {0};
    assert(bench_add(&b, PART_CLOCK, 0, 0) == 0);
    assert(bench_add(&b, PART_LED, 0, 0) == 1);
    assert(sim_auto_is_dynamic(&b) && sim_auto_is_periodic(&b));
    sim_auto_dc_equivalent(&b, &out);
    assert(out.parts[0].type == PART_SOURCE && out.parts[0].value == 5.0 && out.parts[0].on);
    open[1] = 1;
    sim_auto_open_parts(&b, open, &out);
    assert(out.parts[1].type == PART_RESISTOR && out.parts[1].value == SIM_AUTO_OPEN_OHMS);
    assert(out.parts[0].type == PART_CLOCK);
    b.parts[0].on = 0;
    assert(!sim_auto_is_periodic(&b) && !sim_auto_is_dynamic(&b));
    assert(bench_add(&b, PART_CAPACITOR, 0, 0) == 2);
    assert(sim_auto_is_dynamic(&b) && !sim_auto_is_periodic(&b));
}

static void test_shorts(void)
{
    Workbench b = {0};
    unsigned char shorted[WORKBENCH_MAX_COMPONENTS];
    assert(bench_add(&b, PART_SOURCE, 0, 0) == 0);
    assert(bench_add(&b, PART_RESISTOR, 0, 0) == 1);
    wire(&b, 0, 0, 1, 0);
    wire(&b, 1, 1, 0, 1);
    assert(netlist_shorted_sources(&b, shorted) == 0);
    b.parts[1].type = PART_INDUCTOR;
    assert(netlist_shorted_sources(&b, shorted) == 1 && shorted[0]);
    b.parts[1].type = PART_SWITCH;
    b.parts[1].on = 0;
    assert(netlist_shorted_sources(&b, shorted) == 0);
    b.parts[1].on = 1;
    assert(netlist_shorted_sources(&b, shorted) == 1);
}

static void run(const Workbench *b, const char *exe, SimAutoResult *result, SpiceTrace *trace)
{
    SimAutoOptions options = {0, {0, 0}, 1e-5, .005, burn_limits_default()};
    int ok = sim_auto_run(b, exe, &options, result, trace);
    if (!ok)
        fprintf(stderr, "sim_auto_run: %s\n", result->message);
    assert(ok);
}

static void test_with_ngspice(const char *exe)
{
    static SpiceTrace trace;
    SimAutoResult r;
    Workbench b = {0};

    /* LED straight on 5 V burns; once open, no current flows. */
    assert(bench_add(&b, PART_SOURCE, 0, 0) == 0);
    assert(bench_add(&b, PART_LED, 0, 0) == 1);
    wire(&b, 0, 0, 1, 0);
    wire(&b, 1, 1, 0, 1);
    run(&b, exe, &r, NULL);
    assert(r.burnt_count == 1 && r.burnt[1] && r.burn_reason[1] == BURN_OVERCURRENT && r.burn_value[1] > 0.5);
    assert(r.has_dc && fabs(r.currents[1]) < 1e-6);

    /* Adding a 1 kOhm resistor fixes it: nothing burns and the LED conducts a few mA. */
    assert(bench_add(&b, PART_RESISTOR, 0, 0) == 2);
    b.wire_count = 0;
    wire(&b, 0, 0, 2, 0);
    wire(&b, 2, 1, 1, 0);
    wire(&b, 1, 1, 0, 1);
    run(&b, exe, &r, NULL);
    assert(r.burnt_count == 0 && r.currents[1] > 0.002 && r.currents[1] < 0.004);

    /* With 10 Ohm in series the LED is the most stressed part: it fails first and the resistor survives. */
    b.parts[2].value = 10;
    run(&b, exe, &r, NULL);
    assert(r.burnt_count == 1 && r.burnt[1] && !r.burnt[2]);

    /* 10 Ohm alone across 5 V dissipates 2.5 W and burns. */
    b.wire_count = 0;
    wire(&b, 0, 0, 2, 0);
    wire(&b, 2, 1, 0, 1);
    run(&b, exe, &r, NULL);
    assert(r.burnt_count == 1 && r.burnt[2] && r.burn_reason[2] == BURN_OVERPOWER &&
           fabs(r.burn_value[2] - 2.5) < 1e-3);

    /* A wire across the source is a short: the source burns and the circuit loses power. */
    b.parts[2].value = 1000;
    b.wire_count = 0;
    wire(&b, 0, 0, 2, 0);
    wire(&b, 2, 1, 1, 0);
    wire(&b, 1, 1, 0, 1);
    wire(&b, 0, 0, 0, 1);
    run(&b, exe, &r, NULL);
    assert(r.burnt[0] && r.burn_reason[0] == BURN_SHORT && !r.powered && r.currents[1] == 0);

    /* Unfinished circuit: one LED terminal is loose. It must solve without errors. */
    b.wire_count = 0;
    wire(&b, 0, 0, 2, 0);
    wire(&b, 2, 1, 1, 0);
    run(&b, exe, &r, NULL);
    assert(r.burnt_count == 0 && r.powered && fabs(r.currents[1]) < 1e-6);

    /* A clock makes the circuit dynamic: a looping transient is produced. */
    memset(&b, 0, sizeof(b));
    assert(bench_add(&b, PART_CLOCK, 0, 0) == 0);
    assert(bench_add(&b, PART_RESISTOR, 0, 0) == 1);
    assert(bench_add(&b, PART_LED, 0, 0) == 2);
    wire(&b, 0, 0, 1, 0);
    wire(&b, 1, 1, 2, 0);
    wire(&b, 2, 1, 0, 1);
    run(&b, exe, &r, &trace);
    assert(r.has_trace && r.periodic && r.burnt_count == 0 && trace.count > 10);

    /* Dynamic overload is not inferred from a DC approximation. */
    b.parts[1].type = PART_SWITCH;
    b.parts[1].on = 1;
    run(&b, exe, &r, &trace);
    assert(!r.burnt[2] && r.has_trace);

    /* Redundant ideal wires do not hide why wire-current instrumentation was unavailable. */
    memset(&b, 0, sizeof(b));
    assert(bench_add(&b, PART_CLOCK, 0, 0) == 0);
    assert(bench_add(&b, PART_RESISTOR, 0, 0) == 1);
    assert(bench_add(&b, PART_LED, 0, 0) == 2);
    assert(bench_add(&b, PART_SWITCH, 0, 0) == 3);
    b.parts[3].on = 1;
    wire(&b, 0, 0, 1, 0);
    wire(&b, 1, 1, 2, 0);
    wire(&b, 2, 1, 0, 1);
    wire(&b, 0, 0, 3, 0);
    wire(&b, 0, 0, 3, 1);
    {
        SimAutoOptions options = {1, {0, 0}, 1e-5, .005, burn_limits_default()};
        assert(sim_auto_run(&b, exe, &options, &r, &trace));
        assert(r.has_trace && !trace.measures_wires);
        assert(strstr(r.message, "Correntes indisponiveis:"));
    }

    /* An inductor across a voltage source ramps current; it is not an immediate DC short. */
    memset(&b, 0, sizeof(b));
    assert(bench_add(&b, PART_SOURCE, 0, 0) == 0);
    assert(bench_add(&b, PART_INDUCTOR, 0, 0) == 1);
    wire(&b, 0, 0, 1, 0);
    wire(&b, 1, 1, 0, 1);
    run(&b, exe, &r, &trace);
    assert(r.has_trace && r.powered && !r.burnt_count);
    assert(trace.count > 10);
}

int main(int argc, char **argv)
{
    test_limits();
    test_transforms();
    test_shorts();
    if (argc > 1)
        test_with_ngspice(argv[1]);
    puts(argc > 1 ? "Testes de simulacao automatica (com ngspice) passaram."
                  : "Testes de simulacao automatica passaram.");
    return 0;
}
