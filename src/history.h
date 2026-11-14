#ifndef HISTORY_H
#define HISTORY_H
#include "project.h"
#define HISTORY_LIMIT 32
typedef struct {
    Project undo[HISTORY_LIMIT], redo[HISTORY_LIMIT];
    int undo_count, redo_count;
} History;
void history_record(History *history, const Project *current);
int history_undo(History *history, Project *current);
int history_redo(History *history, Project *current);
#endif
