#include "bench_scope.h"
#include "app_editing.h"
#include "ui_text.h"
#include "app_settings.h"
#include "workbench_file.h"
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void reset_interaction(AppState *app)
{
    app->selected = -1;
    app->selected_wire = -1;
    app->dragging = -1;
    app->drag_wire = -1;
    app->drag_lead.component = -1;
    app->placing = -1;
    app->packed_placing = 0;
    app->pending.component = -1;
    app->route_count = 0;
    app->selecting_area = 0;
}

static void copy_selection(AppState *app)
{
    if (!bench_selection_count(&app->selection, &app->bench) && app->selected >= 0)
        app->selection.parts[app->selected] = 1;
    app->status = bench_selection_copy(&app->bench, &app->selection, &app->clipboard)
                      ? tr(TXT_APP_SELECTION_COPIED)
                      : tr(TXT_APP_SELECT_TO_COPY);
}

static void delete_selection(AppState *app)
{
    if (!bench_selection_count(&app->selection, &app->bench))
        return;
    bench_history_record(app->history, &app->bench);
    bench_selection_delete(&app->bench, &app->selection);
    reset_interaction(app);
    app->status = tr(TXT_APP_SELECTION_DELETED);
}

static void paste_selection(AppState *app)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (!bench_selection_paste(&app->bench, &app->selection, &app->clipboard, app->mouse_world.x, app->mouse_world.y))
    {
        app->status = tr(TXT_APP_NOTHING_TO_PASTE);
        if (!app->clipboard.valid)
            return;
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_NO_ROOM), WORKBENCH_MAX_COMPONENTS,
                 WORKBENCH_MAX_WIRES);
        app->status = app->selection_status;
        return;
    }
    bench_history_record(app->history, &before);
    reset_interaction(app);
    app->selected = before.part_count;
    app->mouse_world.x += 24;
    app->mouse_world.y += 24;
    app->status = tr(TXT_APP_SELECTION_PASTED);
}

int app_editing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod)
{
    if (key == SDLK_F3)
    {
        app->projects_open = !app->projects_open;
        app_projects_refresh(app);
        reset_interaction(app);
        return 1;
    }
    if (key == SDLK_ESCAPE && app->projects_open)
    {
        app->projects_open = 0;
        return 1;
    }
    if ((mod & SDL_KMOD_CTRL) && key == SDLK_S)
    {
        app_projects_save(app);
        return 1;
    }
    if ((mod & SDL_KMOD_CTRL) && key == SDLK_L)
    {
        app_projects_load(app);
        return 1;
    }
    if (app->projects_open)
    {
        if (key >= SDLK_1 && key <= SDLK_9)
            app->save_slot = (int)(key - SDLK_1);
        return 1;
    }
    if ((mod & SDL_KMOD_CTRL) && key == SDLK_A)
    {
        for (int i = 0; i < app->bench.part_count; ++i)
            app->selection.parts[i] = 1;
        app->selected = app->bench.part_count ? 0 : -1;
        app->status = tr(TXT_APP_ALL_SELECTED);
        return 1;
    }
    if ((mod & SDL_KMOD_CTRL) && (key == SDLK_C || key == SDLK_X))
    {
        copy_selection(app);
        if (key == SDLK_X)
            delete_selection(app);
        return 1;
    }
    if ((mod & SDL_KMOD_CTRL) && key == SDLK_V)
    {
        paste_selection(app);
        return 1;
    }
    if (key == SDLK_DELETE && bench_selection_count(&app->selection, &app->bench) > 1)
    {
        delete_selection(app);
        return 1;
    }
    return 0;
}

static void slot_path(const AppState *app, int slot, char *path, size_t capacity)
{
    snprintf(path, capacity, "%s/slot-%d.elec", app->selftest ? "build/test-slots" : "saves", slot + 1);
}

void app_projects_refresh(AppState *app)
{
    for (int i = 0; i < PROJECT_SLOT_COUNT; ++i)
    {
        char path[128];
        slot_path(app, i, path, sizeof(path));
        FILE *file = fopen(path, "r");
        app->saved_slots[i] = file != NULL;
        if (file)
            fclose(file);
    }
}

void app_projects_save(AppState *app)
{
    char path[128];
    const char *directory = app->selftest ? "build/test-slots" : "saves";
    if (_mkdir(directory) != 0 && errno != EEXIST)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_PROJECT_FOLDER_FAILED),
                 strerror(errno));
        app->status = app->selection_status;
        return;
    }
    WorkbenchSettings settings = app_settings_capture(app);
    slot_path(app, app->save_slot, path, sizeof(path));
    int saved = workbench_save_project(path, &app->bench, &settings);
    snprintf(app->selection_status, sizeof(app->selection_status),
             tr(saved ? TXT_APP_PROJECT_SAVED : TXT_APP_PROJECT_SAVE_FAILED), app->save_slot + 1);
    app->status = app->selection_status;
    app_projects_refresh(app);
}

void app_projects_load(AppState *app)
{
    char path[128];
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    WorkbenchSettings settings;
    slot_path(app, app->save_slot, path, sizeof(path));
    if (!workbench_load_project(path, &app->bench, &settings))
    {
        app->status = tr(TXT_APP_PROJECT_LOAD_FAILED);
        return;
    }
    bench_history_record(app->history, &before);
    spice_job_request_reset(&app->dc_job);
    reset_interaction(app);
    bench_selection_clear(&app->selection);
    app_settings_apply_project(app, &settings);
    sim_state_clear_result(&app->sim);
    memset(app->leds, 0, sizeof(app->leds));
    app->projects_open = 0;
    app->status = tr(TXT_APP_PROJECT_LOADED);
}
