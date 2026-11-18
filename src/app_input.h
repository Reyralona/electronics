#ifndef APP_INPUT_H
#define APP_INPUT_H
#include "app_state.h"
void app_input_poll(AppState *app, SDL_Window *window);
int app_input_apply_scale(AppState *app, SDL_Window *window);
#endif
