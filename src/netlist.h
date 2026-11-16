#ifndef NETLIST_H
#define NETLIST_H
#include <stdio.h>
#include "workbench.h"
/* Experimental DC: one 5 V source, 1 kOhm resistors, generic diode LEDs, switches and breadboards. */
typedef enum { NETLIST_OK, NETLIST_INVALID, NETLIST_UNSUPPORTED, NETLIST_IO_ERROR, NETLIST_AMBIGUOUS_WIRES } NetlistResult;
/* Finite contact impedances keep topology stable across live on/off changes.
 * open/closed must stay well below 1/DBL_EPSILON (~4.5e15): a 1e18 ratio (1 uOhm/1 TOhm) made the
 * transient timestep collapse after opening a supply that fed a charged capacitor and LED. */
#define NETLIST_LIVE_CLOSED_OHMS 1e-3
#define NETLIST_POT_MIN_SEGMENT_OHMS .001
#define NETLIST_LIVE_OPEN_OHMS 1e10
/* Input resistance of logic gate and 74HC inputs to their reference (50 nA at 5 V).
 * Must stay far below NETLIST_LIVE_OPEN_OHMS: with only the 1 TOhm rshunt, an input behind an open
 * switch divided to ~4.95 V and read HIGH, lighting a 4-input AND with every switch off. */
#define NETLIST_GATE_INPUT_OHMS 1e8
NetlistResult netlist_write_live(FILE *out, const Workbench *bench, int measure_wires);
/* Terminals joined by wires, breadboard strips or direct contact (switch states ignored). */
int netlist_same_net(const Workbench *bench, Pin a, Pin b);
int netlist_live_node(const Workbench *bench, int measure_wires, Pin pin);
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
