#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H
#include "app_state.h"
/* Window is only needed for native text entry; editing/model commands can be tested without one. */
void app_commands_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod);
#endif
