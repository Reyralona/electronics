#ifndef SPICE_TRACE_H
#define SPICE_TRACE_H
#include <stdint.h>
#include "spice_process.h"
/* Replay one complete trace span in five wall-clock seconds; never extrapolate. */
int spice_trace_replay_index(const SpiceTrace *trace, int start_index, uint64_t elapsed_ms, int *finished);
int spice_trace_nearest(const SpiceTrace *trace, double fraction);
#endif
