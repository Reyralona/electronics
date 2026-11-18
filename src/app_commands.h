#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H
#include "app_state.h"
/* Window is only needed for native text entry; editing/model commands can be tested without one. */
void app_commands_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod);
/* Replaces the bench with example index (undoable) and closes the examples dialog. */
void app_commands_load_example(AppState *app, int index);
/* Close button of the graph window. Closing a full curve also resumes the paused live session. */
void app_commands_close_graph(AppState *app);
#endif
