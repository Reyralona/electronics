#include "netlist.h"
#include <math.h>

#define PIN_TOTAL (WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS)
static int find_root(int *parents,int x) { while(parents[x]!=x) x=parents[x]; return x; }
static void unite(int *parents,int a,int b) { parents[find_root(parents,a)]=find_root(parents,b); }
static int pin_index(Pin p) { return p.component*WORKBENCH_MAX_PINS+p.pin; }
static int valid_pin(const Workbench *b,Pin p) { return p.component>=0 && p.component<b->part_count && p.pin>=0 && p.pin<bench_pin_count(b->parts[p.component].type); }

static NetlistResult write_netlist(FILE *out,const Workbench *b,int transient,int measure_wires,Pin probe,double step,double stop)
{
    int parents[PIN_TOTAL],i,j,source=-1,ground,positive;
    if(!out || !b || b->part_count<0 || b->part_count>WORKBENCH_MAX_COMPONENTS || b->wire_count<0 || b->wire_count>WORKBENCH_MAX_WIRES) return NETLIST_INVALID;
    if(transient && (!valid_pin(b,probe) || !isfinite(step) || !isfinite(stop) || step<=0 || stop<=0 || step>stop)) return NETLIST_INVALID;
    for(i=0;i<PIN_TOTAL;++i) parents[i]=i;
    for(i=0;i<b->wire_count;++i) {
        Wire w=b->wires[i];
        if(!valid_pin(b,w.a) || !valid_pin(b,w.b)) return NETLIST_INVALID;
        if(!measure_wires) unite(parents,pin_index(w.a),pin_index(w.b));
    }
    for(i=0;i<b->part_count;++i) {
        int base=i*WORKBENCH_MAX_PINS;
        PartType type=b->parts[i].type;
        if((type==PART_SOURCE || type==PART_RESISTOR || type==PART_CAPACITOR || type==PART_INDUCTOR) &&
           (!isfinite(b->parts[i].value) || b->parts[i].value<1e-12 || b->parts[i].value>1e12)) return NETLIST_INVALID;
        switch(b->parts[i].type) {
        case PART_SOURCE: if(source>=0) return NETLIST_UNSUPPORTED; source=i; break;
        case PART_SWITCH: if(b->parts[i].on) unite(parents,base,base+1); break;
        case PART_BREADBOARD: for(j=0;j<BREADBOARD_PINS;++j) unite(parents,base+(j/5)*5,base+j); break;
        case PART_RESISTOR: case PART_LED: case PART_CAPACITOR: case PART_INDUCTOR: break;
        default: return NETLIST_UNSUPPORTED;
        }
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type!=PART_BREADBOARD) {
        int pin; for(pin=0;pin<bench_pin_count(b->parts[i].type);++pin) { Pin contact;
            if(bench_find_contact(b,i,pin,&contact)) unite(parents,i*WORKBENCH_MAX_PINS+pin,pin_index(contact));
        }
    }
    if(source<0) return NETLIST_INVALID;
    ground=find_root(parents,source*WORKBENCH_MAX_PINS+1);
    positive=find_root(parents,source*WORKBENCH_MAX_PINS);
    if(ground==positive) return NETLIST_INVALID;
    if(measure_wires) {
        int paths[PIN_TOTAL];
        for(i=0;i<PIN_TOTAL;++i) paths[i]=find_root(parents,i);
        for(i=0;i<b->wire_count;++i) {
            int a=pin_index(b->wires[i].a), c=pin_index(b->wires[i].b);
            if(find_root(paths,a)==find_root(paths,c)) return NETLIST_AMBIGUOUS_WIRES;
            unite(paths,a,c);
        }
        if(find_root(paths,ground)==find_root(paths,positive)) return NETLIST_INVALID;
    }
    if(fprintf(out,"Electronics experimental %s\nVsource %d 0 %.17g\n",transient?"transient":"DC",positive==ground?0:positive+1,b->parts[source].on?b->parts[source].value:0.0)<0) return NETLIST_IO_ERROR;
    if(measure_wires) for(i=0;i<b->wire_count;++i) {
        int a=find_root(parents,pin_index(b->wires[i].a)),c=find_root(parents,pin_index(b->wires[i].b));
        if(fprintf(out,"Vwire%d %d %d 0\n",i,a==ground?0:a+1,c==ground?0:c+1)<0) return NETLIST_IO_ERROR;
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_RESISTOR) {
        int a=find_root(parents,i*WORKBENCH_MAX_PINS),c=find_root(parents,i*WORKBENCH_MAX_PINS+1);
        if(fprintf(out,"R%d %d %d %.17g\n",i,a==ground?0:a+1,c==ground?0:c+1,b->parts[i].value)<0) return NETLIST_IO_ERROR;
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_LED) {
        int a=find_root(parents,i*WORKBENCH_MAX_PINS),c=find_root(parents,i*WORKBENCH_MAX_PINS+1);
        if(fprintf(out,"D%d %d %d LED_GENERIC\n",i,a==ground?0:a+1,c==ground?0:c+1)<0) return NETLIST_IO_ERROR;
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_CAPACITOR) {
        int a=find_root(parents,i*WORKBENCH_MAX_PINS),c=find_root(parents,i*WORKBENCH_MAX_PINS+1);
        if(fprintf(out,"C%d %d %d %.17g IC=0\n",i,a==ground?0:a+1,c==ground?0:c+1,b->parts[i].value)<0) return NETLIST_IO_ERROR;
    }
    for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_INDUCTOR) {
        int a=find_root(parents,i*WORKBENCH_MAX_PINS),c=find_root(parents,i*WORKBENCH_MAX_PINS+1);
        if(fprintf(out,"L%d %d %d %.17g IC=0\n",i,a==ground?0:a+1,c==ground?0:c+1,b->parts[i].value)<0) return NETLIST_IO_ERROR;
    }
    if(fprintf(out,".model LED_GENERIC D(IS=1e-18 N=2)\n")<0) return NETLIST_IO_ERROR;
    if(transient) {
        int node=find_root(parents,pin_index(probe));
        if(fprintf(out,".control\ntran %.17g %.17g uic\nprint v(%d)\nquit\n.endc\n.end\n",step,stop,node==ground?0:node+1)<0) return NETLIST_IO_ERROR;
    } else {
        if(fprintf(out,".control\nop\nprint v(%d) @vsource[i]\n",positive==ground?0:positive+1)<0) return NETLIST_IO_ERROR;
        for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_LED)
            if(fprintf(out,"print @d%d[id]\n",i)<0) return NETLIST_IO_ERROR;
        for(i=0;i<b->part_count;++i) if(b->parts[i].type==PART_RESISTOR)
            if(fprintf(out,"print @r%d[i]\n",i)<0) return NETLIST_IO_ERROR;
        if(measure_wires) for(i=0;i<b->wire_count;++i)
            if(fprintf(out,"print @vwire%d[i]\n",i)<0) return NETLIST_IO_ERROR;
        if(fprintf(out,"quit\n.endc\n.end\n")<0) return NETLIST_IO_ERROR;
    }
    if(ferror(out)) return NETLIST_IO_ERROR;
    return NETLIST_OK;
}

NetlistResult netlist_write_dc(FILE *out,const Workbench *b) { return write_netlist(out,b,0,0,(Pin){0,0},0,0); }
NetlistResult netlist_write_dc_instrumented(FILE *out,const Workbench *b) { return write_netlist(out,b,0,1,(Pin){0,0},0,0); }
NetlistResult netlist_write_transient(FILE *out,const Workbench *b,Pin probe,double step,double stop) { return write_netlist(out,b,1,0,probe,step,stop); }
