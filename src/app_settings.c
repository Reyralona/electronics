#include "app_settings.h"
#include "app_preferences.h"
#include <direct.h>
#include <errno.h>

WorkbenchSettings app_settings_capture(const AppState *app)
{
    WorkbenchSettings settings = workbench_settings_default();
    settings.show_grid = app->show_grid;
    settings.show_handles = app->show_handles;
    settings.round_wires = app->round_wires;
    settings.show_current = app->show_current;
    settings.wire_color = app->wire_color;
    settings.probe_pin = app->probe_pin;
    settings.probe_current = app->probe_current;
    settings.transient_step = app->transient_step;
    settings.transient_stop = app->transient_stop;
    settings.grid_spacing = app->grid_spacing;
    settings.snap_grid = app->snap_grid;
    settings.snap_board = app->snap_board;
    settings.dark_theme = app->dark_theme;
    settings.gui_font_size = app->gui_font_size;
    settings.show_pin_labels = app->show_pin_labels;
    return settings;
}

void app_settings_apply_project(AppState *app, const WorkbenchSettings *settings)
{
    app->show_grid = settings->show_grid;
    app->show_handles = settings->show_handles;
    app->round_wires = settings->round_wires;
    app->show_current = settings->show_current;
    app->wire_color = settings->wire_color;
    app->probe_pin = settings->probe_pin;
    app->probe_current = settings->probe_current;
    app->transient_step = settings->transient_step;
    app->transient_stop = settings->transient_stop;
    app->grid_spacing = settings->grid_spacing;
    app->snap_grid = settings->snap_grid;
    app->snap_board = settings->snap_board;
}

void app_settings_apply_all(AppState *app, const WorkbenchSettings *settings)
{
    app_settings_apply_project(app, settings);
    app->dark_theme = settings->dark_theme;
    app->gui_font_size = settings->gui_font_size;
    app->show_pin_labels = settings->show_pin_labels;
}

void app_settings_reset(AppState *app)
{
    WorkbenchSettings defaults = workbench_settings_default();
    app_settings_apply_all(app, &defaults);
}

#define PREFERENCES_DIRECTORY "saves"
#define PREFERENCES_PATH PREFERENCES_DIRECTORY "/preferences.txt"

void app_settings_load_preferences(AppState *app)
{
    WorkbenchSettings settings = app_settings_capture(app);
    if (app_preferences_load(PREFERENCES_PATH, &settings))
        app_settings_apply_all(app, &settings);
    app->preferences_saved = app_settings_capture(app);
    app->preferences_enabled = 1;
}

void app_settings_autosave(AppState *app)
{
    if (!app->preferences_enabled || app->editing >= 0)
        return;
    WorkbenchSettings current = app_settings_capture(app);
    if (app_preferences_equal(&current, &app->preferences_saved))
        return;
    /* Remember the attempt either way: a failing disk must not be retried every frame. */
    app->preferences_saved = current;
    if ((_mkdir(PREFERENCES_DIRECTORY) != 0 && errno != EEXIST) ||
        !app_preferences_save(PREFERENCES_PATH, &current))
        app->status = "Não foi possível salvar as preferências; a sessão continua com os valores atuais.";
}
