#ifndef APP_CODE_EDITOR_H
#define APP_CODE_EDITOR_H
#include "app_state.h"
#include "app_layout.h"
#include <SDL3/SDL.h>
/* Side panel that edits a microcontroller program. The draft lives in AppState::code_text and reaches
 * the part (and the running board) only through app_code_apply, which records undo history. */
#define APP_CODE_LINE_HEIGHT 18.0f

int app_code_editable(const AppState *app, int part);
void app_code_open(AppState *app, SDL_Window *window, int part);
/* Applies a changed draft, then closes. */
void app_code_close(AppState *app, SDL_Window *window);
/* Stores the draft in the part and restarts that board; returns 0 when nothing changed. */
int app_code_apply(AppState *app);
/* Returns 1 when the event belonged to the editor. */
int app_code_event(AppState *app, SDL_Window *window, const SDL_Event *event);
/* Closes the editor when its part was deleted or replaced. */
void app_code_validate(AppState *app, SDL_Window *window);
int app_code_visible_lines(void);
/* Fixed-pitch advance of one character in the code face, in interface pixels. */
float app_code_advance(void);
int app_code_completions(const AppState *app, int *matches, int capacity);
/* Row of the completion popup, placed under the cursor line. */
AppRect app_code_completion_item(const AppState *app, int index);
#endif
