#ifndef SIM_AUTO_H
#define SIM_AUTO_H
#include <stddef.h>
#include "workbench.h"
#include "component_devices.h"
#include "spice_process.h"

/* Automatic analysis used while the simulation is running.
   The burn model is didactic: limits are typical catalogue values, not data for a specific part.
   A burnt part is treated as an open circuit. Burns are derived from the current assembly only, so
   fixing the circuit makes the part work again without replacing it. */

typedef enum { BURN_NONE, BURN_OVERCURRENT, BURN_OVERPOWER, BURN_SHORT } BurnReason;

typedef struct {
    double led_max_current;     /* A, forward current */
    double resistor_max_power;  /* W */
} BurnLimits;

BurnLimits burn_limits_default(void);

/* Returns how far a part is beyond its limit (> 1 means it burns) and why. 0 for parts without a limit. */
double burn_stress(const Part *part, double current, const BurnLimits *limits, BurnReason *reason, double *measured);

/* Circuits with clocks, capacitors or inductors need a transient analysis to be shown. */
int sim_auto_is_dynamic(const Workbench *bench);
/* Circuits with an active clock repeat; their playback loops. */
int sim_auto_is_periodic(const Workbench *bench);
/* Worst-case DC copy: active clocks become sources held at their high level (5 V). */
int sim_auto_dc_equivalent(const Workbench *in, Workbench *out);
/* Copy in which every marked part becomes an open circuit (a 1 GOhm resistor between its first two pins). */
int sim_auto_open_parts(const Workbench *in, const unsigned char open[WORKBENCH_MAX_COMPONENTS], Workbench *out);

#define SIM_AUTO_OPEN_OHMS 1e9

typedef struct {
    int instrument_wires;
    Pin probe;
    double step, stop;
    BurnLimits limits;
} SimAutoOptions;

typedef struct {
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS];
    unsigned char burn_reason[WORKBENCH_MAX_COMPONENTS];
    double burn_value[WORKBENCH_MAX_COMPONENTS];   /* current in A, or power in W, that exceeded the limit */
    int burnt_count;
    int powered;
    int has_dc;
    double source_current;
    double currents[WORKBENCH_MAX_COMPONENTS];     /* LED and resistor currents, pin 1 -> 2 */
    double elements[WORKBENCH_MAX_COMPONENTS][COMPONENT_MAX_ELEMENTS]; /* per-device measurements */
    double wire_currents[WORKBENCH_MAX_WIRES];
    int measures_wires;
    int has_trace;
    int periodic;
    char message[160];
} SimAutoResult;

/* Runs the burn check and the analysis that matches the circuit. trace may be NULL for static circuits.
   Returns 0 only when the solver could not produce a result; the message then holds the reason. */
int sim_auto_run(const Workbench *bench, const char *executable, const SimAutoOptions *options,
                 SimAutoResult *result, SpiceTrace *trace);
#endif
