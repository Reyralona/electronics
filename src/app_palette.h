#ifndef APP_PALETTE_H
#define APP_PALETTE_H
#include "workbench.h"
int is_gate(PartType type);
PartType palette_part(int page, int item);
const char *part_name(PartType type);
#endif
