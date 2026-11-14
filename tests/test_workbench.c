#include <assert.h>
#include <math.h>
#include "workbench.h"

int main(void)
{
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
    return 0;
}
