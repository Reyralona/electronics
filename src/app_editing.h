#ifndef APP_EDITING_H
#define APP_EDITING_H
#include "app_state.h"
int app_editing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod);
void app_projects_refresh(AppState *app);
void app_projects_save(AppState *app);
void app_projects_load(AppState *app);
#endif
