#ifndef APP_PACKING_H
#define APP_PACKING_H
#include "app_state.h"
int app_packing_store_model(AppState *app, const Workbench *model, int *slot);
int app_packing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod);
void app_packing_refresh(AppState *app);
void app_packing_choose(AppState *app, int index);
void app_packing_place(AppState *app, float x, float y);
#endif
