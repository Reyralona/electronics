#ifndef APP_EDITOR_H
#define APP_EDITOR_H
#include "app_state.h"
AppState *app_editor_open(AppState *parent);
AppState *app_editor_route(AppState *active);
AppState *app_editor_dispose(AppState *active);
int app_editor_dirty(const AppState *app);
int app_editor_save(AppState *app);
int app_editor_apply(AppState *app);
int app_editor_set_name(AppState *app, const char *name);
int app_editor_begin_name(AppState *app, SDL_Window *window);
void app_editor_request_close(AppState *app, int quit);
void app_editor_confirm(AppState *app, int choice);
int app_editor_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod);
int app_editor_click(AppState *app, SDL_Window *window, float x, float y);
#endif
