#include <assert.h>
#include <math.h>
#include <string.h>
#include "spice_process.h"

int main(int argc, char **argv)
{
    Workbench b={0}; char message[128]; double current=42, led[WORKBENCH_MAX_COMPONENTS]={0}, wire[WORKBENCH_MAX_WIRES]={0};
    assert(bench_add(&b,PART_SOURCE,0,0)==0);
    assert(bench_add(&b,PART_RESISTOR,0,0)==1);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
    assert(!spice_dc_analyze(&b,"build/missing-ngspice.exe",&current,led,message,sizeof(message)));
    assert(current==42 && strstr(message,"nao encontrado"));
    if(argc>1) {
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(fabs(led[1]-.005)<1e-7);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(wire[0]-.005)<1e-7 && fabs(wire[1]+.005)<1e-7);
        assert(bench_add(&b,PART_RESISTOR,0,0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){2,1})==1);
        assert(spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(fabs(current+.01)<1e-7 && fabs(wire[0]-.005)<1e-7 && fabs(wire[2]-.005)<1e-7);
        bench_remove(&b,2);
        assert(bench_add(&b,PART_BREADBOARD,0,0)==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){1,0})==1);
        assert(!spice_dc_analyze_wires(&b,argv[1],&current,led,wire,message,sizeof(message)));
        assert(strstr(message,"ambigua"));
        assert(fabs(wire[0]-.005)<1e-7);
        bench_remove(&b,2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,1})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,0})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[1]+.005)<1e-7);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,0})==2);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==1);
        assert(!spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==2);
        assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_add(&b,PART_LED,0,0)==2);
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){0,1})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(led[2]>.003 && led[2]<.004);
        assert(led[1]>.003 && led[1]<.004);
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,0})==2);
        assert(bench_toggle_wire(&b,(Pin){2,1},(Pin){0,1})==2);
        assert(bench_toggle_wire(&b,(Pin){1,1},(Pin){2,1})==1);
        assert(bench_toggle_wire(&b,(Pin){2,0},(Pin){0,1})==1);
        assert(spice_dc_analyze(&b,argv[1],&current,led,message,sizeof(message)));
        assert(fabs(led[2])<1e-8);
    }
    return 0;
}
