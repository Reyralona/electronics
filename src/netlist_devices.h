#ifndef NETLIST_DEVICES_H
#define NETLIST_DEVICES_H
#include <stdio.h>
#include "workbench.h"
/* SPICE models of multi-element devices (see component_devices.h). Pure text generation. */
int netlist_device_supported(PartType type);
/* nodes: SPICE node of each terminal (0 = ground). Returns 0 on I/O failure. */
int netlist_device_write(FILE *out, const Part *part, int index, const int *nodes);
int netlist_device_models(FILE *out);
/* Values the live session can change with `alter` without rebuilding the circuit. */
int netlist_device_control_count(const Part *part);
int netlist_device_control(const Part *part, int index, int control, char *name, size_t capacity, double *value);
/* Device whose settings change the netlist text itself (waveform, frequency): rebuild on change. */
int netlist_device_rebuilds_on_change(PartType type);
#endif
