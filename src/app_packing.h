#ifndef APP_PACKING_H
#define APP_PACKING_H
#include "app_state.h"
/* Writes model into *slot, or into the first free slot when *slot < 0 (then updates *slot).
 * Returns 1 when saved, 0 when the library is full, -1 on failure (message in selection_status). */
int app_packing_store_model(AppState *app, const Workbench *model, int *slot);
/* A model identity no listed block uses. */
uint64_t app_packing_new_model(AppState *app);
/* Slot listing model, or -1. Without identity (legacy chip) a unique legacy_name match is used. */
int app_packing_model_slot(AppState *app, uint64_t model, const char *legacy_name);
int app_packing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod);
void app_packing_refresh(AppState *app);
void app_packing_choose(AppState *app, int index);
void app_packing_place(AppState *app, float x, float y);
int app_packing_delete(AppState *app, int index);
#endif
