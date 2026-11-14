#include <assert.h>
#include "workbench.h"

int main(void)
{
    Workbench b = {0};
    int led[WORKBENCH_MAX_COMPONENTS];
    Pin sp = {0, 0}, sn = {0, 1}, a = {1, 0}, c = {1, 1}, sw0 = {2, 0}, sw1 = {2, 1};
    assert(bench_add(&b, PART_SOURCE, 0, 0) == 0);
    assert(bench_add(&b, PART_LED, 0, 0) == 1);
    assert(bench_add(&b, PART_SWITCH, 0, 0) == 2);
    assert(!bench_toggle_wire(&b, sp, sp));
    assert(bench_toggle_wire(&b, sp, sw0) == 1);
    assert(bench_toggle_wire(&b, sw1, a) == 1);
    assert(bench_toggle_wire(&b, c, sn) == 1);
    bench_simulate(&b, led); assert(!led[1]);
    b.parts[2].on = 1;
    bench_simulate(&b, led); assert(led[1]);
    assert(bench_toggle_wire(&b, a, sw1) == 2);
    bench_simulate(&b, led); assert(!led[1]);
    assert(bench_toggle_wire(&b, a, sw1) == 1);
    assert(bench_toggle_wire(&b, sp, sn) == 1);
    bench_simulate(&b, led); assert(!led[1]);
    bench_remove(&b, 0);
    assert(b.part_count == 2 && b.wire_count == 1);
    assert(b.wires[0].a.component < 2 && b.wires[0].b.component < 2);
    {
        Workbench bread = {0};
        Pin positive = {0, 0}, negative = {0, 1}, anode = {1, 0}, cathode = {1, 1};
        Pin hole0 = {2, 0}, hole4 = {2, 4}, other = {2, 5}, opposite = {2, 50};
        assert(bench_add(&bread, PART_SOURCE, 0, 0) == 0);
        assert(bench_add(&bread, PART_LED, 0, 0) == 1);
        assert(bench_add(&bread, PART_BREADBOARD, 0, 0) == 2);
        assert(bench_pin_count(PART_BREADBOARD) == 100);
        assert(bench_toggle_wire(&bread, positive, hole0));
        assert(bench_toggle_wire(&bread, hole4, anode));
        assert(bench_toggle_wire(&bread, cathode, negative));
        bench_simulate(&bread, led); assert(led[1]);
        assert(bench_toggle_wire(&bread, hole4, anode) == 2);
        assert(bench_toggle_wire(&bread, other, anode));
        bench_simulate(&bread, led); assert(!led[1]);
        assert(bench_toggle_wire(&bread, other, anode) == 2);
        assert(bench_toggle_wire(&bread, opposite, anode));
        bench_simulate(&bread, led); assert(!led[1]);
    }
    return 0;
}
