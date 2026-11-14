#ifndef SPICE_JOB_H
#define SPICE_JOB_H
#include <SDL3/SDL.h>
#include "workbench.h"
#include "spice_process.h"
typedef struct {
    SDL_Thread *thread;
    SDL_AtomicInt done;
    Workbench snapshot;
    char executable[1024], message[128];
    double current;
    double led_currents[WORKBENCH_MAX_COMPONENTS];
    double wire_currents[WORKBENCH_MAX_WIRES];
    int measures_wires, transient;
    Pin probe; double step, stop; SpiceTrace trace;
    int success;
} SpiceJob;
int spice_job_start(SpiceJob *job, const Workbench *bench, const char *executable);
int spice_job_start_wires(SpiceJob *job, const Workbench *bench, const char *executable);
int spice_job_start_transient(SpiceJob *job, const Workbench *bench, const char *executable, Pin probe, double step, double stop);
int spice_job_start_transient_current(SpiceJob *job, const Workbench *bench, const char *executable, int component, double step, double stop);
int spice_job_start_transient_wire(SpiceJob *job, const Workbench *bench, const char *executable, int wire, double step, double stop);
/* Returns 1 after a finished job is joined; 0 otherwise. */
int spice_job_poll(SpiceJob *job);
void spice_job_join(SpiceJob *job);
#endif
