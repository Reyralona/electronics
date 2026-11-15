#ifndef NETLIST_H
#define NETLIST_H
#include <stdio.h>
#include "workbench.h"
/* Experimental DC: one 5 V source, 1 kOhm resistors, generic diode LEDs, switches and breadboards. */
typedef enum { NETLIST_OK, NETLIST_INVALID, NETLIST_UNSUPPORTED, NETLIST_IO_ERROR, NETLIST_AMBIGUOUS_WIRES } NetlistResult;
NetlistResult netlist_write_dc(FILE *out, const Workbench *bench);
/* Each wire becomes a zero-volt current sensor; ideal wire cycles are ambiguous. */
NetlistResult netlist_write_dc_instrumented(FILE *out, const Workbench *bench);
/* Transient output contains time and voltage at probe; capacitors and inductors start at zero. */
NetlistResult netlist_write_transient(FILE *out, const Workbench *bench, Pin probe, double step_seconds, double stop_seconds);
NetlistResult netlist_write_transient_current(FILE *out, const Workbench *bench, int component, double step_seconds, double stop_seconds);
NetlistResult netlist_write_transient_wire(FILE *out, const Workbench *bench, int wire, double step_seconds, double stop_seconds);
/* Marks sources and clocks whose terminals are joined by wires, closed switches, breadboard groups or
   inductors (a short at DC). Returns how many were marked. */
int netlist_shorted_sources(const Workbench *bench, unsigned char shorted[WORKBENCH_MAX_COMPONENTS]);
/* Series resistance of the generic LED model, in ohms. */
#define NETLIST_LED_SERIES_OHMS 2.0
NetlistResult netlist_write_transient_instrumented(FILE *out, const Workbench *bench, int mode, Pin probe, double step_seconds, double stop_seconds);
#endif
