#include "workbench_examples.h"
#include <string.h>

static int connect(Workbench *b,Pin a,Pin c,int color,const WirePoint *knees,int count)
{
    Wire *wire;
    if(count<0 || count>WIRE_MAX_KNEES || bench_toggle_wire(b,a,c)!=1) return 0;
    wire=&b->wires[b->wire_count-1]; wire->color=color; wire->knee_count=count;
    if(count) memcpy(wire->knees,knees,(size_t)count*sizeof(*knees));
    return 1;
}
const char *workbench_example_name(int index)
{
    static const char *names[]={"LED na breadboard: P calcula DC.","Flip-flop D: T calcula Q; F1 escolhe terminal 3."};
    return index>=0 && index<WORKBENCH_EXAMPLE_COUNT?names[index]:NULL;
}
int workbench_example_create(int index,Workbench *destination,int *selected)
{
    Workbench b={0}; int selection;
    if(!destination || !selected || index<0 || index>=WORKBENCH_EXAMPLE_COUNT) return 0;
    if(index==0) {
        WirePoint supply[]={{291,150},{587,150}}, return_path[]={{1019,570},{389,570}}, signal[]={{780,300},{921,300}};
        if(bench_add(&b,PART_SOURCE,340,210)!=0 || bench_add(&b,PART_BREADBOARD,650,400)!=1 ||
           bench_add(&b,PART_RESISTOR,638,347)!=2 || !bench_snap_to_breadboard(&b,2) || bench_add(&b,PART_LED,970,250)!=3) return 0;
        if(!connect(&b,(Pin){0,0},(Pin){1,4},1,supply,2) ||
           !connect(&b,(Pin){1,39},(Pin){3,0},3,signal,2) ||
           !connect(&b,(Pin){3,1},(Pin){0,1},2,return_path,2)) return 0;
        selection=3;
    } else {
        WirePoint common[]={{409,260},{409,350}}, data[]={{311,425},{571,425}}, clock[]={{311,130},{571,130}}, back[]={{1019,560},{409,560}};
        if(bench_add(&b,PART_CLOCK,360,210)!=0 || bench_add(&b,PART_CLOCK,360,350)!=1 ||
           !bench_set_value(&b,1,400) || bench_add(&b,PART_DFF,620,280)!=2 ||
           bench_add(&b,PART_RESISTOR,790,280)!=3 || bench_add(&b,PART_LED,970,280)!=4) return 0;
        if(!connect(&b,(Pin){0,1},(Pin){1,1},2,common,2) ||
           !connect(&b,(Pin){1,0},(Pin){2,0},3,data,2) ||
           !connect(&b,(Pin){0,0},(Pin){2,1},1,clock,2) ||
           !connect(&b,(Pin){2,2},(Pin){3,0},0,NULL,0) ||
           !connect(&b,(Pin){3,1},(Pin){4,0},0,NULL,0) ||
           !connect(&b,(Pin){4,1},(Pin){0,1},2,back,2)) return 0;
        selection=2;
    }
    *destination=b; *selected=selection; return 1;
}
