#ifndef WORKBENCH_EXAMPLES_H
#define WORKBENCH_EXAMPLES_H
#include "workbench.h"
#define WORKBENCH_EXAMPLE_COUNT 15
/* Replaces the destination only after a complete example is built. Indices 0..2 are stable. */
int workbench_example_create(int index, Workbench *destination, int *selected);
const char *workbench_example_name(int index);
const char *workbench_example_category(int index);
const char *workbench_example_description(int index);
#endif
