#ifndef SPICE_PROCESS_H
#define SPICE_PROCESS_H
#include <stddef.h>
#include "workbench.h"
int spice_dc_analyze(const Workbench *bench, const char *executable, double *current, double led_currents[WORKBENCH_MAX_COMPONENTS], char *message, size_t capacity);
int spice_dc_analyze_wires(const Workbench *bench, const char *executable, double *current, double component_currents[WORKBENCH_MAX_COMPONENTS], double wire_currents[WORKBENCH_MAX_WIRES], char *message, size_t capacity);
#define SPICE_TRANSIENT_MAX_SAMPLES 2048
typedef struct { double time, value; } SpiceSample;
typedef struct { SpiceSample samples[SPICE_TRANSIENT_MAX_SAMPLES]; int count; } SpiceTrace;
int spice_transient_analyze(const Workbench *bench, const char *executable, Pin probe, double step, double stop, SpiceTrace *trace, char *message, size_t capacity);
int spice_transient_current(const Workbench *bench, const char *executable, int component, double step, double stop, SpiceTrace *trace, char *message, size_t capacity);
int spice_transient_wire(const Workbench *bench, const char *executable, int wire, double step, double stop, SpiceTrace *trace, char *message, size_t capacity);
#endif
