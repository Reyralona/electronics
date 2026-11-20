#ifndef APP_EDITING_H
#define APP_EDITING_H
#include "app_state.h"
int app_editing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod);
int app_selection_value_target(const AppState *app);
int app_selection_same_type(const AppState *app);
int app_selection_set_value(AppState *app, int target, double value);
int app_selection_set_scale(AppState *app, float scale);
void app_projects_refresh(AppState *app);
void app_projects_save(AppState *app);
void app_projects_confirm(AppState *app, int replace);
void app_projects_load(AppState *app);
/* Records undo; failure restores a prepared before-snapshot and reports the cause. */
int app_history_record(AppState *app, Workbench *before);
#endif
