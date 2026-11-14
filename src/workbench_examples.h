#ifndef WORKBENCH_EXAMPLES_H
#define WORKBENCH_EXAMPLES_H
#include "workbench.h"
#define WORKBENCH_EXAMPLE_COUNT 2
/* Replaces the destination only after a complete example is built. */
int workbench_example_create(int index, Workbench *destination, int *selected);
const char *workbench_example_name(int index);
#endif
