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
#endif
