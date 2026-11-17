#ifndef APP_SIMULATION_H
#define APP_SIMULATION_H
#include "app_state.h"

/* Poll the worker, invalidate stale results and advance playback on the UI thread. */
void app_simulation_update(AppState *app);
/* Requested simulated duration for one asynchronous live batch. */
double app_simulation_live_span(uint64_t previous_ms, uint64_t now_ms, double step, double speed);
#endif
