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

typedef enum { BURN_NONE, BURN_OVERCURRENT, BURN_OVERPOWER, BURN_SHORT, BURN_OVERVOLTAGE, BURN_PIN_VOLTAGE } BurnReason;
/* Feedback below burning: the part still works, but a real one would be stressed or out of spec. */
typedef enum { PART_ALERT_NONE, PART_ALERT_CURRENT_LIMIT, PART_ALERT_OVERCURRENT, PART_ALERT_OVERVOLTAGE,
               PART_ALERT_FLOATING_INPUT } PartAlert;

typedef struct {
    double led_max_current;     /* A, forward current */
    double resistor_max_power;  /* W */
} BurnLimits;

BurnLimits burn_limits_default(void);
/* LEDs, display segments and RGB colors share the indicator LED current limit. */
int burn_is_led(PartType type);

/* Returns how far a part is beyond its limit (> 1 means it burns) and why. 0 for parts without a limit. */
double burn_stress(const Part *part, double current, const BurnLimits *limits, BurnReason *reason, double *measured);

/* Circuits with clocks, capacitors or inductors need a transient analysis to be shown. */
int sim_auto_is_dynamic(const Workbench *bench);
/* Circuits with an active clock repeat; their playback loops. */
int sim_auto_is_periodic(const Workbench *bench);
/* Powered or dynamic circuits run in the continuous session (and can use the live graph). */
int sim_auto_uses_live_session(const Workbench *bench);
/* Largest solver step of the continuous session. It is independent of the graph step in the
   settings: clock and square edges are solver breakpoints and capacitors/inductors are bounded by the
   solver's error control, so only a sine source needs points inside each period. */
#define SIM_LIVE_MAX_STEP_SECONDS 1e-3
#define SIM_LIVE_SINE_POINTS_PER_PERIOD 32.0
double sim_auto_live_max_step(const Workbench *bench);
/* Worst-case DC copy: active clocks become sources held at their high level (5 V). */
int sim_auto_dc_equivalent(const Workbench *in, Workbench *out);
/* Copy in which every marked part becomes an open circuit (a 1 GOhm resistor between its first two pins). */
int sim_auto_open_parts(const Workbench *in, const unsigned char open[WORKBENCH_MAX_COMPONENTS], Workbench *out);

#define SIM_AUTO_OPEN_OHMS 1e9

/* What the live graph measures. The continuous session samples it inside every batch. */
typedef enum { SIM_LIVE_PROBE_NONE, SIM_LIVE_PROBE_VOLTAGE, SIM_LIVE_PROBE_PART_CURRENT,
               SIM_LIVE_PROBE_WIRE_CURRENT } SimLiveProbeKind;

typedef struct {
    SimLiveProbeKind kind;
    Pin target;        /* pin for a voltage, {part, 0} for a part current, {wire, 0} for a wire current */
    double interval;   /* requested simulated seconds between samples */
} SimLiveProbe;

#define SIM_LIVE_MAX_SAMPLES 256

typedef struct {
    int valid;         /* 0 when the probed point has no solver vector (the graph says so) */
    int count;
    double start, interval;
    double values[SIM_LIVE_MAX_SAMPLES];
} SimLiveSamples;

typedef struct {
    int instrument_wires;
    Pin probe;
    double step, stop;
    BurnLimits limits;
    SimLiveProbe live_probe;
} SimAutoOptions;

typedef struct {
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS];
    unsigned char burn_reason[WORKBENCH_MAX_COMPONENTS];
    double burn_value[WORKBENCH_MAX_COMPONENTS];   /* current in A, or power in W, that exceeded the limit */
    int burnt_count;
    unsigned char alert[WORKBENCH_MAX_COMPONENTS];     /* PartAlert */
    unsigned char alert_pin[WORKBENCH_MAX_COMPONENTS]; /* terminal the alert refers to */
    double alert_value[WORKBENCH_MAX_COMPONENTS];      /* measured current (A) or voltage (V) */
    int powered;
    int has_dc;
    double source_current;
    double currents[WORKBENCH_MAX_COMPONENTS];     /* LED and resistor currents, pin 1 -> 2 */
    double elements[WORKBENCH_MAX_COMPONENTS][COMPONENT_MAX_ELEMENTS]; /* per-device measurements */
    double wire_currents[WORKBENCH_MAX_WIRES];
    int measures_wires;
    int has_trace;
    int periodic;
    SimLiveSamples live_samples;
    char message[160];
} SimAutoResult;

/* Runs the burn check and the analysis that matches the circuit. trace may be NULL for static circuits.
   Returns 0 only when the solver could not produce a result; the message then holds the reason. */
int sim_auto_run(const Workbench *bench, const char *executable, const SimAutoOptions *options,
                 SimAutoResult *result, SpiceTrace *trace);
#endif
