#ifndef APP_CONTROLS_H
#define APP_CONTROLS_H
#include "app_state.h"
#include "app_layout.h"

/* All focusable targets come from the same rectangles used by drawing and mouse input. */
#define APP_CONTROL_LIMIT 128
AppControlContext app_controls_context(const AppState *app);
int app_controls_targets(const AppState *app, AppRect targets[APP_CONTROL_LIMIT]);
AppRect app_controls_focus(const AppState *app);
int app_controls_action_enabled(const AppState *app, AppAction action);
int app_controls_graph_visible(const AppState *app);
#endif
