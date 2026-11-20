#ifndef APP_PALETTE_H
#define APP_PALETTE_H
#include "workbench.h"
int is_gate(PartType type);
#define PALETTE_PAGES 9
int palette_count(int page);
const char *palette_page_name(int page);
PartType palette_part(int page, int item);
int palette_filtered_count(int page, const char *query);
PartType palette_filtered_part(int page, const char *query, int item);
const char *part_name(PartType type);
#endif
