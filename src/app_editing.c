#include "bench_scope.h"
#include "app_editing.h"
#include "ui_text.h"
#include "app_settings.h"
#include "app_simulation.h"
#include "app_code_editor.h"
#include "mcu_board.h"
#include "workbench_file.h"
#include "component_registry.h"
#include <direct.h>
#include <io.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

int app_selection_same_type(const AppState *app)
{
    int target = app->selected;
    if (target < 0 || target >= app->bench.part_count || app->bench.parts[target].group)
        return 0;
    PartType type = app->bench.parts[target].type;
    for (int i = 0; i < app->bench.part_count; ++i)
        if (app->selection.parts[i] && (app->bench.parts[i].type != type || app->bench.parts[i].group))
            return 0;
    return 1;
}

int app_selection_value_target(const AppState *app)
{
    if (!app_selection_same_type(app))
        return -1;
    const ComponentInfo *info = component_info(app->bench.parts[app->selected].type);
    return info && info->primary ? app->selected : -1;
}

int app_selection_set_value(AppState *app, int target, double value)
{
    if (target != app_selection_value_target(app) || target < 0 ||
        !component_value_valid(app->bench.parts[target].type, value))
        return 0;
    for (int i = 0; i < app->bench.part_count; ++i)
        if (i == target || app->selection.parts[i])
            bench_set_value(&app->bench, i, value);
    return 1;
}

int app_selection_set_scale(AppState *app, float scale)
{
    if (!app_selection_same_type(app) || !isfinite(scale) || scale < PART_SCALE_MIN || scale > PART_SCALE_MAX)
        return 0;
    for (int i = 0; i < app->bench.part_count; ++i)
        if ((i == app->selected || app->selection.parts[i]) && !bench_set_scale(&app->bench, i, scale))
            return 0;
    return 1;
}

static void reset_interaction(AppState *app)
{
    app->selected = -1;
    app->selected_wire = -1;
    app->dragging = -1;
    app->drag_wire = -1;
    app->drag_lead.component = -1;
    app->drag_port.component = -1;
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
    if (!app_history_record(app, &app->bench))
        return;
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
    if (!app_history_record(app, &before))
        return;
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
        app_state_toggle_panel(app, APP_PANEL_PROJECTS);
        app_projects_refresh(app);
        if (app->projects_open)
            app->project_page = app->save_slot / PROJECT_SLOTS_PER_PAGE;
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
        if (key >= SDLK_1 && key <= SDLK_6)
        {
            int slot = app->project_page * PROJECT_SLOTS_PER_PAGE + (int)(key - SDLK_1);
            if (slot < app->project_slot_count)
                app->save_slot = slot;
        }
        if (key == SDLK_PAGEUP && app->project_page > 0)
            app->project_page--;
        if (key == SDLK_PAGEDOWN && (app->project_page + 1) * PROJECT_SLOTS_PER_PAGE < app->project_slot_count)
            app->project_page++;
        if (key == SDLK_PAGEUP || key == SDLK_PAGEDOWN)
            app->save_slot = app->project_page * PROJECT_SLOTS_PER_PAGE;
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
    const char *directory = app->selftest ? "build/test-slots" : "saves";
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s/slot-*.elec", directory);
    struct _finddata_t entry;
    intptr_t search = _findfirst(pattern, &entry);
    int highest = 0;
    if (search != -1)
    {
        do
        {
            int slot = 0;
            int length = 0;
            sscanf(entry.name, "slot-%d.elec%n", &slot, &length);
            if (length > 0 && !entry.name[length] && slot > highest)
                highest = slot;
        } while (_findnext(search, &entry) == 0);
        _findclose(search);
    }
    int count = highest + 1;
    if (app->save_slot >= count)
        count = app->save_slot + 1;
    if (count < 1)
        count = 1;
    int *saved = calloc((size_t)count, sizeof(*saved));
    int *parts = malloc((size_t)count * sizeof(*parts));
    int *wires = malloc((size_t)count * sizeof(*wires));
    if (!saved || !parts || !wires)
    {
        free(saved);
        free(parts);
        free(wires);
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    free(app->saved_slots);
    free(app->project_parts);
    free(app->project_wires);
    app->saved_slots = saved;
    app->project_parts = parts;
    app->project_wires = wires;
    app->project_slot_count = count;
    if (app->save_slot >= count)
        app->save_slot = count - 1;
    for (int i = 0; i < count; ++i)
    {
        char path[128];
        slot_path(app, i, path, sizeof(path));
        struct _stat metadata;
        app->saved_slots[i] = _stat(path, &metadata) == 0 || errno != ENOENT;
        app->project_parts[i] = -1;
        app->project_wires[i] = -1;
        if (!app->saved_slots[i])
            continue;
        Workbench saved BENCH_AUTO_CLEAR = {0};
        WorkbenchSettings settings;
        if (workbench_load_project(path, &saved, &settings))
        {
            app->project_parts[i] = saved.part_count;
            app->project_wires[i] = saved.wire_count;
        }
    }
}

static void save_project_slot(AppState *app, int slot)
{
    app_simulation_flush(app);
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
    slot_path(app, slot, path, sizeof(path));
    int saved = workbench_save_project(path, &app->bench, &settings);
    snprintf(app->selection_status, sizeof(app->selection_status),
             tr(saved ? TXT_APP_PROJECT_SAVED : TXT_APP_PROJECT_SAVE_FAILED), slot + 1);
    app->status = app->selection_status;
    app_projects_refresh(app);
    if (saved)
        app->project_thumbnail_pending = slot;
}

void app_projects_save(AppState *app)
{
    app_projects_refresh(app);
    if (app->save_slot < 0 || app->save_slot >= app->project_slot_count || app->project_overwrite >= 0)
        return;
    if (app->saved_slots[app->save_slot])
    {
        app->project_overwrite = app->save_slot;
        app->status = tr(TXT_UI_REPLACE_NOTE);
        return;
    }
    save_project_slot(app, app->save_slot);
}

void app_projects_confirm(AppState *app, int replace)
{
    int slot = app->project_overwrite;
    app->project_overwrite = -1;
    if (slot < 0 || slot >= app->project_slot_count)
        return;
    if (replace)
        save_project_slot(app, slot);
    else
        app->status = tr(TXT_APP_EDIT_CANCELED);
}

void app_projects_load(AppState *app)
{
    if (app->project_overwrite >= 0 || app->save_slot < 0 || app->save_slot >= app->project_slot_count)
        return;
    char path[128];
    Workbench loaded BENCH_AUTO_CLEAR = {0};
    WorkbenchSettings settings;
    slot_path(app, app->save_slot, path, sizeof(path));
    if (!workbench_load_project(path, &loaded, &settings))
    {
        app->status = tr(TXT_APP_PROJECT_LOAD_FAILED);
        return;
    }
    if (!app_code_close(app, NULL))
        return;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (!bench_history_record(app->history, &before))
    {
        app->status = tr(TXT_APP_HISTORY_FAILED);
        return;
    }
    uint64_t revision = app->bench.revision + 1;
    bench_snapshot_move(&app->bench, &loaded);
    app->bench.revision = revision;
    mcu_runtime_reset(app->mcu);
    spice_job_request_reset(&app->dc_job);
    reset_interaction(app);
    bench_selection_clear(&app->selection);
    app_settings_apply_project(app, &settings);
    bench_view_fit_workbench(&app->view, &app->bench);
    sim_state_clear_result(&app->sim);
    memset(app->leds, 0, sizeof(app->leds));
    app->projects_open = 0;
    app->status = tr(TXT_APP_PROJECT_LOADED);
}

int app_code_editable(const AppState *app, int part)
{
    return part >= 0 && part < app->bench.part_count && !app->bench.parts[part].group &&
           mcu_board(app->bench.parts[part].type) != NULL;
}

int app_code_apply(AppState *app)
{
    if (!app_code_editable(app, app->code_part))
        return 0;
    const Part *part = &app->bench.parts[app->code_part];
    if (!strcmp(mcu_program_text(part), app->code_text))
    {
        app->code_dirty = 0;
        return 0;
    }
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench) || !bench_set_code(&app->bench, app->code_part, app->code_text))
    {
        app->status = tr(TXT_MCU_CODE_NO_MEMORY);
        return 0;
    }
    if (!app_history_record(app, &before))
        return 0;
    app->code_dirty = 0;
    app->status = tr(TXT_MCU_CODE_SENT);
    return 1;
}

int app_code_close(AppState *app, SDL_Window *window)
{
    if (app->code_part < 0)
        return 1;
    int applied = app->code_dirty && app_code_apply(app);
    if (app->code_dirty && !applied)
        return 0;
    app->code_part = -1;
    app->code_dirty = 0;
    if (window)
        SDL_StopTextInput(window);
    app->status = applied ? tr(TXT_MCU_CODE_APPLIED) : tr(TXT_MCU_EDITOR_CLOSED);
    return 1;
}


int app_history_record(AppState *app, Workbench *before)
{
    if (bench_history_record(app->history, before))
        return 1;
    if (before != &app->bench)
        bench_snapshot_move(&app->bench, before);
    app->status = tr(TXT_APP_HISTORY_FAILED);
    return 0;
}
