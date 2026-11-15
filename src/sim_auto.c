#include "component_registry.h"
#include "bench_scope.h"
#include "sim_auto.h"
#include "netlist.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

BurnLimits burn_limits_default(void)
{
    BurnLimits limits;
    limits.led_max_current = 0.030;   /* typical absolute maximum of a 5 mm indicator LED */
    limits.resistor_max_power = 0.25; /* common 1/4 W through-hole resistor */
    return limits;
}

double burn_stress(const Part *part, double current, const BurnLimits *limits, BurnReason *reason, double *measured)
{
    *reason = BURN_NONE;
    *measured = 0;
    if (!isfinite(current)) return 0;
    if (part->type == PART_LED) {
        *reason = BURN_OVERCURRENT;
        *measured = current;
        return current / limits->led_max_current;
    }
    if (part->type == PART_RESISTOR) {
        double power = current * current * part->value;
        *reason = BURN_OVERPOWER;
        *measured = power;
        return power / limits->resistor_max_power;
    }
    return 0;
}

int sim_auto_is_periodic(const Workbench *bench)
{
    int i;
    for (i = 0; i < bench->part_count; ++i) {
        if (bench->parts[i].type == PART_CLOCK && bench->parts[i].on) return 1;
    }
    return 0;
}

int sim_auto_is_dynamic(const Workbench *bench)
{
    int i;
    if (sim_auto_is_periodic(bench)) return 1;
    for (i = 0; i < bench->part_count; ++i) {
        PartType type = bench->parts[i].type;
        if (type == PART_CAPACITOR || type == PART_INDUCTOR) return 1;
    }
    return 0;
}

int sim_auto_dc_equivalent(const Workbench *in, Workbench *out)
{
    int i;
    if (!bench_snapshot_clone(out, in))
        return 0;
    for (i = 0; i < out->part_count; ++i) {
        Part *part = &out->parts[i];
        if (part->type != PART_CLOCK) continue;
        part->type = PART_SOURCE;
        part->value = 5.0;
    }
    return 1;
}

int sim_auto_open_parts(const Workbench *in, const unsigned char open[WORKBENCH_MAX_COMPONENTS], Workbench *out)
{
    int i;
    if (!bench_snapshot_clone(out, in))
        return 0;
    for (i = 0; i < out->part_count; ++i) {
        Part *part = &out->parts[i];
        if (!open[i] || bench_pin_count(part->type) != 2) continue;
        part->type = PART_RESISTOR;
        part->value = SIM_AUTO_OPEN_OHMS;
        part->on = 0;
    }
    return 1;
}

static int has_energized_source(const Workbench *bench)
{
    int i;
    for (i = 0; i < bench->part_count; ++i) {
        const Part *part = &bench->parts[i];
        if ((component_electrical_type(part->type) == PART_SOURCE || part->type == PART_CLOCK) && part->on) return 1;
    }
    return 0;
}

/* Opens every shorted source. An energized one burns; a switched-off one is just ignored. */
static int open_shorted_sources(const Workbench *test, unsigned char open[], SimAutoResult *result)
{
    unsigned char shorted[WORKBENCH_MAX_COMPONENTS];
    int i;
    if (!netlist_shorted_sources(test, shorted)) return 0;
    for (i = 0; i < test->part_count; ++i) {
        if (!shorted[i]) continue;
        open[i] = 1;
        if (!test->parts[i].on) continue;
        result->burnt[i] = 1;
        result->burn_reason[i] = BURN_SHORT;
        ++result->burnt_count;
    }
    return 1;
}

/* Marks the single most overstressed part, since the first part to fail changes every other current. */
static int burn_worst_part(const Workbench *test, unsigned char open[], const BurnLimits *limits,
                           const double currents[], SimAutoResult *result)
{
    int i, worst = -1;
    double worst_stress = 1.0, worst_value = 0;
    BurnReason worst_reason = BURN_NONE;
    for (i = 0; i < test->part_count; ++i) {
        BurnReason reason;
        double measured, stress;
        if (open[i]) continue;
        stress = burn_stress(&test->parts[i], currents[i], limits, &reason, &measured);
        if (stress <= worst_stress) continue;
        worst = i;
        worst_stress = stress;
        worst_reason = reason;
        worst_value = measured;
    }
    if (worst < 0) return -1;
    open[worst] = 1;
    result->burnt[worst] = 1;
    result->burn_reason[worst] = (unsigned char)worst_reason;
    result->burn_value[worst] = worst_value;
    ++result->burnt_count;
    return worst;
}

static int run_transient(const Workbench *bench, const char *executable, const SimAutoOptions *options,
                         SimAutoResult *result, SpiceTrace *trace)
{
    char message[128];
    int ok = 0;
    char instrumentation_error[128] = {0};
    if (options->instrument_wires) {
        ok = spice_transient_instrumented(bench, executable, 1, options->probe, options->step, options->stop,
                                          trace, message, sizeof(message));
        if (!ok) snprintf(instrumentation_error, sizeof(instrumentation_error), "%s", message);
    }
    if (!ok) {
        ok = spice_transient_analyze(bench, executable, options->probe, options->step, options->stop,
                                     trace, message, sizeof(message));
    }
    snprintf(result->message, sizeof(result->message), "%s", message);
    if (!ok) return 0;
    if (instrumentation_error[0]) {
        snprintf(result->message, sizeof(result->message), "Correntes indisponiveis: %s", instrumentation_error);
    }
    result->has_trace = 1;
    result->periodic = sim_auto_is_periodic(bench);
    return 1;
}

int sim_auto_run(const Workbench *bench, const char *executable, const SimAutoOptions *options,
                 SimAutoResult *result, SpiceTrace *trace)
{
    Workbench dc BENCH_AUTO_CLEAR = {0};
    Workbench test BENCH_AUTO_CLEAR = {0};
    unsigned char open[WORKBENCH_MAX_COMPONENTS] = {0};
    char message[128];
    int iteration, dynamic;
    if (!bench || !executable || !options || !result) return 0;
    memset(result, 0, sizeof(*result));
    dynamic = sim_auto_is_dynamic(bench);
    if (dynamic && !trace) return 0;
    /* DC high-level substitution cannot predict transient stress or thermal failure.
       Keep dynamic components intact and let the transient solver determine their state. */
    if (dynamic) {
        result->powered = has_energized_source(bench);
        return run_transient(bench, executable, options, result, trace);
    }
    if (!sim_auto_dc_equivalent(bench, &dc))
    {
        snprintf(result->message, sizeof(result->message), "Memoria insuficiente para preparar simulacao.");
        return 0;
    }
    for (iteration = 0; iteration <= bench->part_count + 1; ++iteration) {
        if (!sim_auto_open_parts(&dc, open, &test))
        {
            snprintf(result->message, sizeof(result->message), "Memoria insuficiente para preparar simulacao.");
            return 0;
        }
        if (open_shorted_sources(&test, open, result)) continue;
        if (!has_energized_source(&test)) break;
        result->powered = 1;
        if (!spice_dc_analyze(&test, executable, &result->source_current, result->currents, message, sizeof(message))) {
            snprintf(result->message, sizeof(result->message), "%s", message);
            return 0;
        }
        if (burn_worst_part(&test, open, &options->limits, result->currents, result) < 0) break;
        result->powered = 0;
    }
    if (!result->powered) {
        memset(result->currents, 0, sizeof(result->currents));
        result->source_current = 0;
        result->has_dc = 1;
        snprintf(result->message, sizeof(result->message), "Sem fonte de energia ligada.");
        return 1;
    }
    if (dynamic) {
        if (!sim_auto_open_parts(bench, open, &test))
        {
            snprintf(result->message, sizeof(result->message), "Memoria insuficiente para preparar simulacao.");
            return 0;
        }
        return run_transient(&test, executable, options, result, trace);
    }
    result->has_dc = 1;
    snprintf(result->message, sizeof(result->message), "Circuito calculado.");
    if (options->instrument_wires) {
        double current, currents[WORKBENCH_MAX_COMPONENTS];
        if (spice_dc_analyze_wires(&test, executable, &current, currents, result->wire_currents, message, sizeof(message))) {
            result->measures_wires = 1;
        } else {
            snprintf(result->message, sizeof(result->message), "%s", message);
        }
    }
    return 1;
}
