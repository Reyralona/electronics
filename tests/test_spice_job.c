#include <assert.h>
#include <math.h>
#include <string.h>
#include "spice_job.h"

static void finish(SpiceJob *job)
{
    Uint64 deadline=SDL_GetTicks()+12000;
    while(!spice_job_poll(job)) { assert(SDL_GetTicks()<deadline); SDL_Delay(1); }
}
int main(int argc,char **argv)
{
    Workbench bench={0}; SpiceJob job={0};
    assert(SDL_Init(0));
    assert(bench_add(&bench,PART_SOURCE,0,0)==0);
    assert(bench_add(&bench,PART_RESISTOR,0,0)==1);
    assert(bench_toggle_wire(&bench,(Pin){0,0},(Pin){1,0})==1);
    assert(bench_toggle_wire(&bench,(Pin){0,1},(Pin){1,1})==1);
    assert(spice_job_start(&job,&bench,"build/missing-ngspice.exe"));
    assert(!spice_job_start(&job,&bench,"build/missing-ngspice.exe"));
    bench.parts[0].on=0;
    finish(&job);
    assert(!job.success && job.snapshot.parts[0].on==1);
    assert(strstr(job.message,"nao encontrado"));
    if(argc>1) {
        bench.parts[0].on=1;
        assert(spice_job_start(&job,&bench,argv[1]));
        finish(&job);
        assert(job.success && fabs(job.current+.005)<1e-7);
        assert(spice_job_start_wires(&job,&bench,argv[1]));
        finish(&job);
        assert(job.success && job.measures_wires);
        assert(fabs(job.wire_currents[0]-.005)<1e-7);
        assert(fabs(job.wire_currents[1]+.005)<1e-7);
        assert(bench_add(&bench,PART_CAPACITOR,0,0)==2);
        assert(bench_toggle_wire(&bench,(Pin){0,1},(Pin){1,1})==2);
        assert(bench_toggle_wire(&bench,(Pin){1,1},(Pin){2,0})==1);
        assert(bench_toggle_wire(&bench,(Pin){2,1},(Pin){0,1})==1);
        assert(spice_job_start_transient(&job,&bench,argv[1],(Pin){2,0},1e-5,.005));
        finish(&job);
        assert(job.success && job.transient && job.trace.count>2);
        assert(job.trace.samples[job.trace.count-1].value>4.9);
        assert(spice_job_start_transient_current(&job,&bench,argv[1],1,1e-5,.005));
        finish(&job);
        assert(job.success && job.transient==2 && job.trace.samples[0].value>0.004);
        assert(job.trace.samples[job.trace.count-1].value<0.0001);
        assert(spice_job_start_transient_wire(&job,&bench,argv[1],0,1e-5,.005));
        finish(&job);
        assert(job.success && job.transient==3 && job.trace.samples[0].value>0.004);
        assert(spice_job_start_transient_instrumented(&job,&bench,argv[1],1,(Pin){2,0},1e-5,.005));
        finish(&job);
        assert(job.success && job.trace.measures_wires && (job.trace.wires_forward[0][0]&1));

    }
    spice_job_clear(&job);
    assert(!job.thread && job.snapshot.part_count == 0);
    spice_job_clear(&job);
    SDL_Quit();
    return 0;
}
