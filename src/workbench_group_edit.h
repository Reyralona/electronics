#ifndef WORKBENCH_GROUP_EDIT_H
#define WORKBENCH_GROUP_EDIT_H
#include "workbench.h"
#include <stddef.h>
int bench_group_extract(const Workbench *parent, int group, Workbench *draft);
int bench_group_apply(Workbench *parent, int group, uint64_t expected_revision, const Workbench *draft,
                      const char *name, char *message, size_t capacity);
#endif
