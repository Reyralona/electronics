#ifndef WORKBENCH_PACK_H
#define WORKBENCH_PACK_H
#include "workbench_selection.h"
#include <stddef.h>

int bench_pin_exposed(const Part *part, int pin);
int bench_group_first(const Workbench *bench, int group);
int bench_group_ports(const Workbench *bench, int group);
int bench_group_port(const Workbench *bench, int group, int index, Pin *pin);
float bench_group_height(const Workbench *bench, int group);
void bench_terminal_position(const Workbench *bench, Pin pin, float *x, float *y);
int bench_pack_selection(Workbench *bench, const BenchSelection *selection, char *message, size_t capacity);
int bench_unpack_selection(Workbench *bench, const BenchSelection *selection);
int bench_group_wire_internal(const Workbench *bench, const Wire *wire);
#endif
