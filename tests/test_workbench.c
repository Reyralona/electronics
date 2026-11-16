#include <assert.h>
#include <math.h>
#include "workbench.h"

/* Swapping a part keeps its place and the wires on terminals the new type still has. */
static void swap_keeps_wires(void)
{
    Workbench bench = {0};
    int source = bench_add(&bench, PART_SOURCE, 100, 100), gate = bench_add(&bench, PART_AND, 300, 100);
    assert(bench_rotate_part(&bench, gate));
    assert(bench_toggle_wire(&bench, (Pin){source, 0}, (Pin){gate, 0}) == 1);
    assert(bench_toggle_wire(&bench, (Pin){source, 1}, (Pin){gate, 2}) == 1);
    assert(bench_swap_type(&bench, gate, PART_NOT));
    assert(bench.parts[gate].type == PART_NOT && bench.parts[gate].x == 300 && bench.parts[gate].rotation == 1);
    assert(bench.wire_count == 1 && bench.wires[0].b.pin == 0);
    assert(!bench_swap_type(&bench, gate, PART_NOT) && !bench_swap_type(&bench, gate, PART_BREADBOARD));
    bench_snapshot_clear(&bench);
}

int main(void)
{
    swap_keeps_wires();
    {
        double value=42;
        assert(bench_parse_value("2,2e-6",&value) && value>2.199e-6 && value<2.201e-6);
        assert(!bench_parse_value("nan",&value) && value>0);
        assert(!bench_parse_value("1e999",&value));
        assert(!bench_parse_value("2.2foo",&value));
        assert(!bench_parse_value("0",&value));
        assert(!bench_parse_value(" 1",&value));
    }
    {
        Wire wire={0}; WirePoint points[WIRE_PATH_CAPACITY]; int count, i;
        wire.knee_count=2; wire.knees[0]=(WirePoint){100,0}; wire.knees[1]=(WirePoint){100,100};
        count=wire_path(&wire,(WirePoint){0,0},(WirePoint){200,100},20,points);
        assert(count==20 && points[0].x==0 && points[count-1].x==200);
        for(i=0;i<count;++i) assert(isfinite(points[i].x)&&isfinite(points[i].y));
        count=wire_path(&wire,(WirePoint){0,0},(WirePoint){200,100},0,points);
        assert(count==4 && points[1].x==100 && points[1].y==0);
        wire.knees[1]=wire.knees[0];
        count=wire_path(&wire,(WirePoint){100,0},(WirePoint){200,100},20,points);
        for(i=0;i<count;++i) assert(isfinite(points[i].x)&&isfinite(points[i].y));
    }
    Workbench b = {0};
    int led[WORKBENCH_MAX_COMPONENTS];
    Pin sp = {0, 0}, sn = {0, 1}, a = {1, 0}, c = {1, 1}, sw0 = {2, 0}, sw1 = {2, 1};
    assert(bench_add(&b, PART_SOURCE, 250 * b.part_count, 0) == 0);
    assert(bench_add(&b, PART_LED, 250 * b.part_count, 0) == 1);
    assert(bench_add(&b, PART_SWITCH, 250 * b.part_count, 0) == 2);
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
        assert(bench_add(&bread, PART_SOURCE, 250 * bread.part_count, 0) == 0);
        assert(bench_add(&bread, PART_LED, 250 * bread.part_count, 0) == 1);
        assert(bench_add(&bread, PART_BREADBOARD, 250 * bread.part_count, 0) == 2);
        assert(bench_pin_count(PART_BREADBOARD) == 420);
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
        assert(bench_toggle_wire(&bread, opposite, anode)==2);
        assert(bench_toggle_wire(&bread, positive, hole0)==2);
        assert(bench_toggle_wire(&bread, positive, (Pin){2,100})==1);
        assert(bench_toggle_wire(&bread, (Pin){2,104}, anode)==1);
        bench_simulate(&bread, led); assert(led[1]);
        assert(bench_toggle_wire(&bread, (Pin){2,104}, anode)==2);
        assert(bench_toggle_wire(&bread, (Pin){2,105}, anode)==1);
        bench_simulate(&bread, led); assert(!led[1]);
        assert(bench_toggle_wire(&bread, (Pin){2,105}, anode)==2);
        assert(bench_toggle_wire(&bread, positive, (Pin){2,100})==2);
        assert(bench_toggle_wire(&bread, positive, (Pin){2,180})==1);
        assert(bench_toggle_wire(&bread, (Pin){2,184}, anode)==1);
        bench_simulate(&bread, led); assert(led[1]);
        assert(bench_toggle_wire(&bread, (Pin){2,184}, anode)==2);
        assert(bench_toggle_wire(&bread, (Pin){2,185}, anode)==1);
        bench_simulate(&bread, led); assert(!led[1]);
        assert(bench_toggle_wire(&bread, (Pin){2,185}, anode)==2);
        assert(bench_toggle_wire(&bread, positive, (Pin){2,180})==2);
        assert(bench_toggle_wire(&bread, positive, (Pin){2,300})==1);
        assert(bench_toggle_wire(&bread, (Pin){2,329}, anode)==1);
        bench_simulate(&bread, led); assert(led[1]);
        assert(bench_toggle_wire(&bread, (Pin){2,329}, anode)==2);
        assert(bench_toggle_wire(&bread, (Pin){2,330}, anode)==1);
        bench_simulate(&bread, led); assert(!led[1]);
    }
    {
        Workbench b2={0}; Pin contact; int led2[WORKBENCH_MAX_COMPONENTS];
        assert(bench_add(&b2,PART_BREADBOARD,500,400)==0);
        assert(bench_add(&b2,PART_LED,488,347)==1);
        assert(bench_add(&b2,PART_SOURCE,200,200)==2);
        assert(bench_snap_to_breadboard(&b2,1));
        assert(bench_find_contact(&b2,1,0,&contact) && contact.component==0 && contact.pin==0);
        assert(bench_find_contact(&b2,1,1,&contact) && contact.component==0 && contact.pin==35);
        {
            float x,y;
            assert(bench_rotate_part(&b2,1));
            bench_pin_position(&b2.parts[1],0,&x,&y);
            assert(x==b2.parts[1].x && y==b2.parts[1].y-49);
            assert(!bench_find_contact(&b2,1,0,&contact));
            assert(bench_rotate_part(&b2,1)); assert(bench_rotate_part(&b2,1)); assert(bench_rotate_part(&b2,1));
            assert(bench_find_contact(&b2,1,0,&contact) && contact.pin==0);
            assert(!bench_rotate_part(&b2,0));
        }
        assert(bench_toggle_wire(&b2,(Pin){2,0},(Pin){0,4})==1);
        assert(bench_toggle_wire(&b2,(Pin){2,1},(Pin){0,39})==1);
        bench_simulate(&b2,led2); assert(led2[1]);
        b2.parts[1].x+=1;
        bench_simulate(&b2,led2); assert(!led2[1]);
        b2.parts[1].x-=1; b2.parts[0].y+=1;
        bench_simulate(&b2,led2); assert(!led2[1]);
    }
    {
        Workbench board = {0};
        bench_add(&board, PART_BREADBOARD, 550, 430);
        Part candidate = {.type = PART_RESISTOR, .x = 538, .y = 377};
        Workbench before = board;
        float x, y;
        assert(bench_breadboard_snap_position(&board, &candidate, &x, &y));
        assert(board.revision == before.revision && board.parts[0].x == before.parts[0].x);
        int resistor = bench_add(&board, PART_RESISTOR, candidate.x, candidate.y);
        assert(bench_snap_to_breadboard(&board, resistor));
        assert(board.parts[resistor].x == x && board.parts[resistor].y == y);
    }
    {
        Workbench route = {0};
        bench_add(&route, PART_SOURCE, 100, 100);
        bench_add(&route, PART_LED, 400, 100);
        bench_toggle_wire(&route, (Pin){0, 0}, (Pin){1, 0});
        WirePoint projected;
        int slot = -1;
        assert(wire_pick_segment(&route.wires[0], (WirePoint){0, 0}, (WirePoint){100, 0},
                                 18, (WirePoint){50, 3}, 4, &slot, &projected));
        assert(slot == 0 && projected.x == 50 && projected.y == 0);
        assert(bench_insert_knee(&route, 0, slot, projected));
        assert(bench_insert_knee(&route, 0, 0, (WirePoint){25, 0}));
        assert(route.wires[0].knees[1].x == 50);
        assert(!bench_insert_knee(&route, 0, 3, projected));
        assert(bench_remove_knee(&route, 0, 0));
        assert(route.wires[0].knees[0].x == 50);
        Wire corner = {.knee_count = 1, .knees = {{100, 0}}};
        assert(wire_pick_segment(&corner, (WirePoint){0, 0}, (WirePoint){100, 100},
                                 18, (WirePoint){95.5f, 4.5f}, 1, &slot, &projected));
        assert(slot == 0);
        assert(wire_pick_segment(&corner, (WirePoint){0, 0}, (WirePoint){100, 100},
                                 18, (WirePoint){100, 60}, 1, &slot, &projected));
        assert(slot == 1);
        while (route.wires[0].knee_count < WIRE_MAX_KNEES)
            assert(bench_insert_knee(&route, 0, route.wires[0].knee_count, (WirePoint){10, 10}));
        uint64_t revision = route.revision;
        assert(!bench_insert_knee(&route, 0, 0, projected));
        assert(route.revision == revision && route.wires[0].knee_count == WIRE_MAX_KNEES);
        assert(!wire_pick_segment(&corner, (WirePoint){0, 0}, (WirePoint){100, 100},
                                  18, (WirePoint){500, 500}, 1, &slot, &projected));
    }
    {
        for (int rotation = 0; rotation < 4; ++rotation)
        {
            Part part = {.type = PART_RESISTOR, .x = 123, .y = -456, .rotation = rotation};
            WirePoint local = {-17, 23};
            WirePoint world = bench_part_to_world(&part, local);
            WirePoint restored = bench_part_to_local(&part, world);
            assert(restored.x == local.x && restored.y == local.y);
            float x, y;
            bench_pin_position(&part, 0, &x, &y);
            restored = bench_part_to_local(&part, (WirePoint){x, y});
            assert(restored.x == -49 && restored.y == 0);
        }
    }
    {
        Workbench scaled = {0};
        bench_add(&scaled, PART_BREADBOARD, 550, 430);
        int resistor = bench_add(&scaled, PART_RESISTOR, 250 * scaled.part_count, 0);
        double resistance = scaled.parts[resistor].value;
        assert(bench_set_scale(&scaled, resistor, 1.0f / 7));
        float hx, hy;
        bench_pin_position(&scaled.parts[0], 0, &hx, &hy);
        bench_move_part(&scaled, resistor, hx + 7, hy);
        Pin contact;
        assert(bench_find_contact(&scaled, resistor, 0, &contact) && contact.pin == 0);
        assert(bench_find_contact(&scaled, resistor, 1, &contact) && contact.pin == 5);
        assert(scaled.parts[resistor].value == resistance);
        uint64_t revision = scaled.revision;
        assert(!bench_set_scale(&scaled, resistor, 0));
        assert(!bench_set_scale(&scaled, resistor, PART_SCALE_MAX + 1));
        assert(scaled.revision == revision);
        assert(bench_set_scale(&scaled, 0, .5f));
        bench_pin_position(&scaled.parts[0], 0, &hx, &hy);
        WirePoint local = bench_part_to_local(&scaled.parts[0], (WirePoint){hx, hy});
        assert(local.x == -63 && local.y == -55);
        for (int r = 0; r < 4; ++r)
        {
            scaled.parts[resistor].rotation = r;
            WirePoint world = bench_part_to_world(&scaled.parts[resistor], (WirePoint){-49, 0});
            local = bench_part_to_local(&scaled.parts[resistor], world);
            assert(fabsf(local.x + 49) < .001f && fabsf(local.y) < .001f);
        }
    }
    return 0;
}
