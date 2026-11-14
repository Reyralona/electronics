#include "netlist.h"

#define PIN_TOTAL (WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS)
static int find_root(int *parents,int x) { while(parents[x]!=x) x=parents[x]; return x; }
static void unite(int *parents,int a,int b) { parents[find_root(parents,a)]=find_root(parents,b); }
static int pin_index(Pin p) { return p.component*WORKBENCH_MAX_PINS+p.pin; }
static int valid_pin(const Workbench *b,Pin p) { return p.component>=0 && p.component<b->part_count && p.pin>=0 && p.pin<bench_pin_count(b->parts[p.component].type); }

NetlistResult netlist_write_dc(FILE *out,const Workbench *b)
{
    int parents[PIN_TOTAL],i,j,source=-1,ground,positive;
    if(!out || !b || b->part_count<0 || b->part_count>WORKBENCH_MAX_COMPONENTS || b->wire_count<0 || b->wire_count>WORKBENCH_MAX_WIRES) return NETLIST_INVALID;
    for(i=0;i<PIN_TOTAL;++i) parents[i]=i;
    for(i=0;i<b->wire_count;++i) {
        Wire w=b->wires[i];
        if(!valid_pin(b,w.a) || !valid_pin(b,w.b)) return NETLIST_INVALID;
        unite(parents,pin_index(w.a),pin_index(w.b));
    }
    for(i=0;i<b->part_count;++i) {
        int base=i*WORKBENCH_MAX_PINS;
        switch(b->parts[i].type) {
        case PART_SOURCE: if(source>=0) return NETLIST_UNSUPPORTED; source=i; break;
        case PART_SWITCH: if(b->parts[i].on) unite(parents,base,base+1); break;
        case PART_BREADBOARD: for(j=0;j<100;++j) unite(parents,base+(j/5)*5,base+j); break;
        case PART_RESISTOR: break;
        default: return NETLIST_UNSUPPORTED;
        }
    }
    if(source<0) return NETLIST_INVALID;
    ground=find_root(parents,source*WORKBENCH_MAX_PINS+1);
    positive=find_root(parents,source*WORKBENCH_MAX_PINS);
    if(ground==positive) return NETLIST_INVALID;
    if(fprintf(out,"Electronics experimental DC\nVsource %d 0 %d\n",positive==ground?0:positive+1,b->parts[source].on?5:0)<0) return NETLIST_IO_ERROR;
    for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_RESISTOR) {
        int a=find_root(parents,i*WORKBENCH_MAX_PINS),c=find_root(parents,i*WORKBENCH_MAX_PINS+1);
        if(fprintf(out,"R%d %d %d 1000\n",i,a==ground?0:a+1,c==ground?0:c+1)<0) return NETLIST_IO_ERROR;
    }
    if(fprintf(out,".control\nop\nprint v(%d) @vsource[i]\nquit\n.endc\n.end\n",positive==ground?0:positive+1)<0 || ferror(out)) return NETLIST_IO_ERROR;
    return NETLIST_OK;
}
