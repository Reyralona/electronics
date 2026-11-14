#include "spice_job.h"
#include "spice_process.h"
#include <string.h>

static int SDLCALL run_job(void *argument)
{
    SpiceJob *job=argument;
    if(job->measures_wires) job->success=spice_dc_analyze_wires(&job->snapshot,job->executable,&job->current,job->led_currents,job->wire_currents,job->message,sizeof(job->message));
    else job->success=spice_dc_analyze(&job->snapshot,job->executable,&job->current,job->led_currents,job->message,sizeof(job->message));
    SDL_SetAtomicInt(&job->done,1);
    return 0;
}

static int start_job(SpiceJob *job,const Workbench *bench,const char *executable,int wires)
{
    size_t length;
    if(!job || !bench || !executable || job->thread) return 0;
    length=strlen(executable);
    if(length>=sizeof(job->executable)) return 0;
    job->snapshot=*bench;
    job->measures_wires=wires;
    memcpy(job->executable,executable,length+1);
    job->success=0;
    SDL_SetAtomicInt(&job->done,0);
    job->thread=SDL_CreateThread(run_job,"analise-dc",job);
    return job->thread!=NULL;
}

int spice_job_start(SpiceJob *job,const Workbench *bench,const char *executable) { return start_job(job,bench,executable,0); }
int spice_job_start_wires(SpiceJob *job,const Workbench *bench,const char *executable) { return start_job(job,bench,executable,1); }

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
