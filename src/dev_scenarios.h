#ifndef DEV_SCENARIOS_H
#define DEV_SCENARIOS_H
#include "app_state.h"

/* Return nonzero on setup failure, matching the executable exit status. */
int dev_scenario_prepare(AppState *app, int argc, char **argv);
int dev_scenario_analyze(AppState *app, char **argv);
void dev_scenario_queue(AppState *app, SDL_Window *window);
void dev_scenario_validate(AppState *app);
#endif
