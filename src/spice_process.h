#ifndef SPICE_PROCESS_H
#define SPICE_PROCESS_H
#include <stddef.h>
#include "workbench.h"
int spice_dc_analyze(const Workbench *bench, const char *executable, double *current, char *message, size_t capacity);
#endif
