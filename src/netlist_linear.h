#ifndef NETLIST_LINEAR_H
#define NETLIST_LINEAR_H
#include <stdio.h>
#include "workbench.h"
#include "netlist_protection.h"
/* SPICE text of linear ICs (NE555, LM358, LM393, LM7805, ULN2003A, L293D) as ideal datasheet models:
 * thresholds, output levels, drops and limits from TI datasheets (docs/datasheets/ANALOGICOS_E_LIMITES.md),
 * no bandwidth, slew rate or thermal behaviour. Element names use the prefix "l<index>_" and every chip
 * has V l<index>_alive, which the live session drops to 0 when the chip burns. Pure text generation. */
int netlist_linear_supported(PartType type);
/* nodes: SPICE node of each terminal (0 = ground). protection (may be NULL) receives the supply term. */
int netlist_linear_write(FILE *out, const Part *part, int index, const int *nodes, NetlistProtection *protection);
int netlist_linear_models(FILE *out);
/* Supply ratings of a linear IC; returns 0 for other parts. Outputs have no per-pin current check. */
int netlist_linear_ratings(PartType type, double *supply_absolute_max, double *supply_recommended_max);
#endif
