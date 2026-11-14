#include "history.h"
#include <string.h>

static void push(Project *stack, int *count, const Project *project)
{
    if (*count == HISTORY_LIMIT) {
        memmove(stack, stack + 1, (HISTORY_LIMIT - 1) * sizeof(*stack));
        --*count;
    }
    stack[(*count)++] = *project;
}

void history_record(History *history, const Project *current)
{
    push(history->undo, &history->undo_count, current);
    history->redo_count = 0;
}

int history_undo(History *history, Project *current)
{
    if (!history->undo_count) return 0;
    push(history->redo, &history->redo_count, current);
    *current = history->undo[--history->undo_count];
    return 1;
}

int history_redo(History *history, Project *current)
{
    if (!history->redo_count) return 0;
    push(history->undo, &history->undo_count, current);
    *current = history->redo[--history->redo_count];
    return 1;
}
