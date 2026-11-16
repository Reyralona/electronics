#ifndef APP_PALETTE_H
#define APP_PALETTE_H
#include "workbench.h"
int is_gate(PartType type);
#define PALETTE_PAGES 6
int palette_count(int page);
const char *palette_page_name(int page);
PartType palette_part(int page, int item);
const char *part_name(PartType type);
#endif
