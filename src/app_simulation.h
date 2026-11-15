#ifndef APP_SIMULATION_H
#define APP_SIMULATION_H
#include "app_state.h"

/* Poll the worker, invalidate stale results and advance playback on the UI thread. */
void app_simulation_update(AppState *app);
#endif
