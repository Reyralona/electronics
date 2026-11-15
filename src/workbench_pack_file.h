#ifndef WORKBENCH_PACK_FILE_H
#define WORKBENCH_PACK_FILE_H
#include "workbench.h"
#include <stdio.h>
int bench_groups_write(FILE *file, const Workbench *bench);
int bench_groups_read(FILE *file, Workbench *bench, int version);
#endif
