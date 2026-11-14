#include <assert.h>
#include <math.h>
#include <string.h>
#include "spice_process.h"

int main(int argc, char **argv)
{
    Workbench b={0}; char message[128]; double current=42;
    assert(bench_add(&b,PART_SOURCE,0,0)==0);
    assert(bench_add(&b,PART_RESISTOR,0,0)==1);
    assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){1,0})==1);
    assert(bench_toggle_wire(&b,(Pin){0,1},(Pin){1,1})==1);
    assert(!spice_dc_analyze(&b,"build/missing-ngspice.exe",&current,message,sizeof(message)));
    assert(current==42 && strstr(message,"nao encontrado"));
    if(argc>1) {
        assert(spice_dc_analyze(&b,argv[1],&current,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
        assert(bench_toggle_wire(&b,(Pin){0,0},(Pin){0,1})==1);
        assert(!spice_dc_analyze(&b,argv[1],&current,message,sizeof(message)));
        assert(fabs(current+.005)<1e-7);
    }
    return 0;
}
