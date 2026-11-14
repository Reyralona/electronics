#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "netlist.h"

int main(void)
{
    Workbench b={0}; FILE *f; char text[4096]; size_t n;
    assert(bench_add(&b,PART_SOURCE,0,0)==0);
    assert(bench_add(&b,PART_RESISTOR,0,0)==1);
    assert(bench_add(&b,PART_BREADBOARD,0,0)==2);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
    assert(bench_toggle_wire(&b,(Pin){2,4},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){0,1})==1);
    f=fopen("build/netlist-reference.cir","w+"); assert(f);
    assert(netlist_write_dc(f,&b)==NETLIST_OK); rewind(f);
    n=fread(text,1,sizeof(text)-1,f); text[n]='\0'; fclose(f);
    assert(strstr(text,"Vsource") && strstr(text,"R1 ") && strstr(text," 1000"));
    assert(bench_set_value(&b,0,10)); assert(bench_set_value(&b,1,2000));
    f=fopen("build/netlist-adjustable.cir","w"); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_OK); fclose(f);
    assert(bench_set_value(&b,0,5)); assert(bench_set_value(&b,1,1000));
    f=fopen("build/netlist-wires.cir","w"); assert(f);
    assert(netlist_write_dc_instrumented(f,&b)==NETLIST_OK); fclose(f);
    /* A second path between holes in the same two ideal groups has no unique wire currents. */
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,1})==1);
    f=tmpfile(); assert(f); assert(netlist_write_dc_instrumented(f,&b)==NETLIST_AMBIGUOUS_WIRES); fclose(f);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,1})==2);
    assert(bench_add(&b,PART_LED,0,0)==3);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){0,1})==2);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){3,0})==1);
    assert(bench_toggle_wire(&b,(Pin){3,1},(Pin){0,1})==1);
    f=fopen("build/netlist-led-forward.cir","w"); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_OK); fclose(f);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){3,0})==2);
    assert(bench_toggle_wire(&b,(Pin){3,1},(Pin){0,1})==2);
    assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){3,1})==1);
    assert(bench_toggle_wire(&b,(Pin){3,0},(Pin){0,1})==1);
    f=fopen("build/netlist-led-reverse.cir","w"); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_OK); fclose(f);
    bench_remove(&b,3);
    assert(bench_add(&b,PART_SOURCE,0,0)==3);
    f=tmpfile(); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_UNSUPPORTED); fclose(f);
    bench_remove(&b,3);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==1);
    f=tmpfile(); assert(f); assert(netlist_write_dc(f,&b)==NETLIST_INVALID); fclose(f);
    {
        Workbench rc={0};
        assert(bench_add(&rc,PART_SOURCE,0,0)==0);
        assert(bench_add(&rc,PART_RESISTOR,0,0)==1);
        assert(bench_add(&rc,PART_CAPACITOR,0,0)==2);
        assert(bench_toggle_wire(&rc,(Pin){0,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){2,1},(Pin){0,1})==1);
        f=fopen("build/netlist-rc.cir","w"); assert(f);
        assert(netlist_write_transient(f,&rc,(Pin){2,0},.00001,.005)==NETLIST_OK); fclose(f);
        f=tmpfile(); assert(f);
        assert(netlist_write_transient(f,&rc,(Pin){2,0},0,.005)==NETLIST_INVALID);
        assert(netlist_write_transient(f,&rc,(Pin){2,99},.00001,.005)==NETLIST_INVALID); fclose(f);
        bench_remove(&rc,2);
        assert(bench_add(&rc,PART_INDUCTOR,0,0)==2);
        assert(bench_toggle_wire(&rc,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&rc,(Pin){2,1},(Pin){0,1})==1);
        f=fopen("build/netlist-rl.cir","w"); assert(f);
        assert(netlist_write_transient(f,&rc,(Pin){2,0},.0000001,.00005)==NETLIST_OK); fclose(f);
    }
    {
        Workbench snapped={0};
        assert(bench_add(&snapped,PART_SOURCE,200,200)==0);
        assert(bench_add(&snapped,PART_BREADBOARD,500,400)==1);
        assert(bench_add(&snapped,PART_RESISTOR,488,347)==2);
        assert(bench_snap_to_breadboard(&snapped,2));
        assert(bench_toggle_wire(&snapped,(Pin){0,0},(Pin){1,4})==1);
        assert(bench_toggle_wire(&snapped,(Pin){0,1},(Pin){1,39})==1);
        f=fopen("build/netlist-snap.cir","w"); assert(f);
        assert(netlist_write_dc(f,&snapped)==NETLIST_OK); fclose(f);
        assert(bench_toggle_wire(&snapped,(Pin){0,0},(Pin){1,4})==2);
        assert(bench_toggle_wire(&snapped,(Pin){0,1},(Pin){1,39})==2);
        assert(bench_toggle_wire(&snapped,(Pin){0,0},(Pin){1,100})==1);
        assert(bench_toggle_wire(&snapped,(Pin){1,104},(Pin){1,4})==1);
        assert(bench_toggle_wire(&snapped,(Pin){0,1},(Pin){1,39})==1);
        f=fopen("build/netlist-extended-board.cir","w"); assert(f);
        assert(netlist_write_dc(f,&snapped)==NETLIST_OK); fclose(f);
        assert(bench_toggle_wire(&snapped,(Pin){0,0},(Pin){1,100})==2);
        assert(bench_toggle_wire(&snapped,(Pin){1,104},(Pin){1,4})==2);
        assert(bench_toggle_wire(&snapped,(Pin){0,0},(Pin){1,300})==1);
        assert(bench_toggle_wire(&snapped,(Pin){1,329},(Pin){1,4})==1);
        f=fopen("build/netlist-rail.cir","w"); assert(f);
        assert(netlist_write_dc(f,&snapped)==NETLIST_OK); fclose(f);
    }
    return 0;
}
