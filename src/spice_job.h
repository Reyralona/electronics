#ifndef SPICE_JOB_H
#define SPICE_JOB_H
#include <SDL3/SDL.h>
#include "workbench.h"
typedef struct {
    SDL_Thread *thread;
    SDL_AtomicInt done;
    Workbench snapshot;
    char executable[1024], message[128];
    double current;
    int success;
} SpiceJob;
int spice_job_start(SpiceJob *job, const Workbench *bench, const char *executable);
/* Returns 1 after a finished job is joined; 0 otherwise. */
int spice_job_poll(SpiceJob *job);
void spice_job_join(SpiceJob *job);
#endif
