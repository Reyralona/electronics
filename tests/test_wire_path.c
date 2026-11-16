#include "workbench.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* A knee that doubles back over a short stub is dropped; a deliberate long U-turn is kept. */
static float farthest_x(const WirePoint *path, int count)
{
    float x = -1e9f;
    for (int i = 0; i < count; ++i)
        x = fmaxf(x, path[i].x);
    return x;
}

int main(void)
{
    WirePoint path[WIRE_PATH_CAPACITY];
    Wire stub = {0};
    stub.knee_count = 2;
    stub.knees[0] = (WirePoint){108, 0};
    stub.knees[1] = (WirePoint){100, 0}; /* the first knee overshoots 8 units and the wire comes back */
    int count = wire_path(&stub, (WirePoint){0, 0}, (WirePoint){100, 100}, 18, path);
    assert(farthest_x(path, count) <= 100.5f);
    Wire u_turn = {0};
    u_turn.knee_count = 1;
    u_turn.knees[0] = (WirePoint){200, 0};
    count = wire_path(&u_turn, (WirePoint){0, 0}, (WirePoint){0, 5}, 0, path);
    assert(farthest_x(path, count) > 199);
    puts("Wire paths drop short reversal stubs and keep long U-turns.");
    return 0;
}
