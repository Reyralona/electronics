#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H
#include "app_state.h"
#include "workbench_file.h"
/* Single place that maps AppState fields to WorkbenchSettings. */
WorkbenchSettings app_settings_capture(const AppState *app);
/* Projects carry bench-related settings; appearance (theme, text size) belongs to the app. */
void app_settings_apply_project(AppState *app, const WorkbenchSettings *settings);
void app_settings_apply_all(AppState *app, const WorkbenchSettings *settings);
void app_settings_reset(AppState *app);
#endif
