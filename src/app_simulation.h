#ifndef APP_SIMULATION_H
#define APP_SIMULATION_H
#include "app_state.h"

/* Poll the worker, invalidate stale results and advance playback on the UI thread. */
void app_simulation_update(AppState *app);
/* Requested simulated duration for one asynchronous live batch. step is the live solver step;
 * observed_rate (simulated/real, 0 when unknown) shortens batches of slow circuits. */
double app_simulation_live_span(uint64_t previous_ms, uint64_t now_ms, double step, double speed,
                                double observed_rate);
void app_simulation_flush(AppState *app);
#endif
