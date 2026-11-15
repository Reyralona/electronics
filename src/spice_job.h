#ifndef SPICE_JOB_H
#define SPICE_JOB_H
#include <SDL3/SDL.h>
#include "workbench.h"
#include "spice_process.h"
#include "sim_auto.h"
typedef struct {
    SDL_Thread *thread;
    SDL_AtomicInt done;
    Workbench snapshot;
    char executable[1024], message[160];
    double current;
    double led_currents[WORKBENCH_MAX_COMPONENTS];
    double wire_currents[WORKBENCH_MAX_WIRES];
    int measures_wires, transient;
    Pin probe; double step, stop; SpiceTrace trace;
    int success;
    int automatic;                 /* 1 for a job started by spice_job_start_auto */
    SimAutoOptions auto_options;
    SimAutoResult auto_result;
} SpiceJob;
/* Burn check plus the analysis that matches the circuit; results go to auto_result and, if dynamic, trace. */
int spice_job_start_auto(SpiceJob *job, const Workbench *bench, const char *executable, const SimAutoOptions *options);
int spice_job_start(SpiceJob *job, const Workbench *bench, const char *executable);
int spice_job_start_wires(SpiceJob *job, const Workbench *bench, const char *executable);
int spice_job_start_transient(SpiceJob *job, const Workbench *bench, const char *executable, Pin probe, double step, double stop);
int spice_job_start_transient_current(SpiceJob *job, const Workbench *bench, const char *executable, int component, double step, double stop);
int spice_job_start_transient_wire(SpiceJob *job, const Workbench *bench, const char *executable, int wire, double step, double stop);
int spice_job_start_transient_instrumented(SpiceJob *job, const Workbench *bench, const char *executable, int mode, Pin probe, double step, double stop);
/* Returns 1 after a finished job is joined; 0 otherwise. */
int spice_job_poll(SpiceJob *job);
void spice_job_join(SpiceJob *job);
/* Joins before releasing the worker snapshot; poll/join preserve it for result inspection. */
void spice_job_clear(SpiceJob *job);
#endif
