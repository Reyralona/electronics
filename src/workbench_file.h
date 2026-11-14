#ifndef WORKBENCH_FILE_H
#define WORKBENCH_FILE_H
#include <stdio.h>
#include "workbench.h"

/* Parsing is transactional: on failure the destination remains unchanged. */
int workbench_read(FILE *file, Workbench *destination);
int workbench_write(FILE *file, const Workbench *bench);
int workbench_save(const char *path, const Workbench *bench);
int workbench_load(const char *path, Workbench *bench);
#endif
