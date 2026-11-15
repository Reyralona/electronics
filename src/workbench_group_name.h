#ifndef WORKBENCH_GROUP_NAME_H
#define WORKBENCH_GROUP_NAME_H
#include "workbench.h"
int bench_group_name_valid(const char *name);
int bench_group_set_name(Workbench *bench, int group, const char *name);
#endif
