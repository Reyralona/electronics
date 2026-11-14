#ifndef NETLIST_H
#define NETLIST_H
#include <stdio.h>
#include "workbench.h"
/* Experimental DC: one 5 V source, 1 kOhm resistors, generic diode LEDs, switches and breadboards. */
typedef enum { NETLIST_OK, NETLIST_INVALID, NETLIST_UNSUPPORTED, NETLIST_IO_ERROR } NetlistResult;
NetlistResult netlist_write_dc(FILE *out, const Workbench *bench);
#endif
