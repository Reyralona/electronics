#ifndef SPICE_PROCESS_H
#define SPICE_PROCESS_H
#include <stddef.h>
#include "workbench.h"
int spice_dc_analyze(const Workbench *bench, const char *executable, double *current, double led_currents[WORKBENCH_MAX_COMPONENTS], char *message, size_t capacity);
int spice_dc_analyze_wires(const Workbench *bench, const char *executable, double *current, double component_currents[WORKBENCH_MAX_COMPONENTS], double wire_currents[WORKBENCH_MAX_WIRES], char *message, size_t capacity);
#endif
