#include <assert.h>
#include "history.h"

int main(void)
{
    History h = {0};
    Project p = {0};
    int i;
    assert(!history_undo(&h, &p));
    history_record(&h, &p);
    p.circuit.source_on = 1;
    history_record(&h, &p);
    p.blocks[0].x = 2;
    assert(history_undo(&h, &p) && p.blocks[0].x == 0 && p.circuit.source_on == 1);
    assert(history_undo(&h, &p) && p.circuit.source_on == 0);
    assert(history_redo(&h, &p) && p.circuit.source_on == 1);
    history_record(&h, &p);
    p.blocks[0].x = 3;
    assert(!history_redo(&h, &p));
    for (i = 0; i < HISTORY_LIMIT + 2; ++i) { history_record(&h, &p); p.blocks[0].x++; }
    assert(h.undo_count == HISTORY_LIMIT);
    return 0;
}
