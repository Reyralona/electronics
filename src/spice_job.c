#include "spice_job.h"
#include "spice_process.h"
#include <stdio.h>
#include <string.h>

static int SDLCALL run_job(void *argument)
{
    SpiceJob *job=argument;
    if(job->automatic) {
        job->success=sim_auto_run(&job->snapshot,job->executable,&job->auto_options,&job->auto_result,&job->trace);
        snprintf(job->message,sizeof(job->message),"%s",job->auto_result.message);
    }
    else if(job->transient && job->measures_wires) job->success=spice_transient_instrumented(&job->snapshot,job->executable,job->transient,job->probe,job->step,job->stop,&job->trace,job->message,sizeof(job->message));
    else if(job->transient==3) job->success=spice_transient_wire(&job->snapshot,job->executable,job->probe.component,job->step,job->stop,&job->trace,job->message,sizeof(job->message));
    else if(job->transient==2) job->success=spice_transient_current(&job->snapshot,job->executable,job->probe.component,job->step,job->stop,&job->trace,job->message,sizeof(job->message));
    else if(job->transient) job->success=spice_transient_analyze(&job->snapshot,job->executable,job->probe,job->step,job->stop,&job->trace,job->message,sizeof(job->message));
    else if(job->measures_wires) job->success=spice_dc_analyze_wires(&job->snapshot,job->executable,&job->current,job->led_currents,job->wire_currents,job->message,sizeof(job->message));
    else job->success=spice_dc_analyze(&job->snapshot,job->executable,&job->current,job->led_currents,job->message,sizeof(job->message));
    SDL_SetAtomicInt(&job->done,1);
    return 0;
}

/* Every field the thread reads is written before the thread is created. */
static int start_job(SpiceJob *job,const Workbench *bench,const char *executable,int wires,int transient,Pin probe,double step,double stop,const SimAutoOptions *automatic)
{
    size_t length;
    if(!job || !bench || !executable || job->thread) return 0;
    length=strlen(executable);
    if(length>=sizeof(job->executable)) return 0;
    if (!bench_snapshot_clone(&job->snapshot, bench))
    {
        snprintf(job->message, sizeof(job->message), "Nao foi possivel copiar a montagem para simulacao.");
        return 0;
    }
    job->automatic=automatic!=NULL;
    if(automatic) job->auto_options=*automatic;
    job->measures_wires=wires; job->transient=transient; job->probe=probe; job->step=step; job->stop=stop;
    memcpy(job->executable,executable,length+1);
    job->success=0;
    SDL_SetAtomicInt(&job->done,0);
    job->thread=SDL_CreateThread(run_job,"analise-dc",job);
    return job->thread!=NULL;
}

int spice_job_start(SpiceJob *job,const Workbench *bench,const char *executable) { return start_job(job,bench,executable,0,0,(Pin){0,0},0,0,NULL); }
int spice_job_start_wires(SpiceJob *job,const Workbench *bench,const char *executable) { return start_job(job,bench,executable,1,0,(Pin){0,0},0,0,NULL); }

int spice_job_start_transient(SpiceJob *job,const Workbench *bench,const char *executable,Pin probe,double step,double stop) { return start_job(job,bench,executable,0,1,probe,step,stop,NULL); }

int spice_job_start_transient_current(SpiceJob *job,const Workbench *bench,const char *executable,int component,double step,double stop) { return start_job(job,bench,executable,0,2,(Pin){component,0},step,stop,NULL); }

int spice_job_start_transient_wire(SpiceJob *job,const Workbench *bench,const char *executable,int wire,double step,double stop) { return start_job(job,bench,executable,0,3,(Pin){wire,0},step,stop,NULL); }

int spice_job_start_transient_instrumented(SpiceJob *job,const Workbench *bench,const char *executable,int mode,Pin probe,double step,double stop) { if(mode<1 || mode>3) return 0; return start_job(job,bench,executable,1,mode,probe,step,stop,NULL); }

int spice_job_start_auto(SpiceJob *job,const Workbench *bench,const char *executable,const SimAutoOptions *options)
{
    if(!options) return 0;
    return start_job(job,bench,executable,options->instrument_wires,0,options->probe,options->step,options->stop,options);
}

void spice_job_join(SpiceJob *job)
{
    if(job && job->thread) { SDL_WaitThread(job->thread,NULL); job->thread=NULL; }
}

int spice_job_poll(SpiceJob *job)
{
    if(!job || !job->thread || !SDL_GetAtomicInt(&job->done)) return 0;
    spice_job_join(job);
    return 1;
}

void spice_job_clear(SpiceJob *job)
{
    if (!job)
        return;
    spice_job_join(job);
    bench_snapshot_clear(&job->snapshot);
}
