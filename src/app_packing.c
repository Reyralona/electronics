#include "app_editing.h"
#include "bench_scope.h"
#include "app_packing.h"
#include "ui_text.h"
#include "workbench_pack.h"
#include "workbench_group_name.h"
#include "workbench_file.h"
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

enum { TRASH_NAME_ATTEMPTS = 100 };

static void block_path(const AppState *app, int index, char *path, size_t capacity)
{
    snprintf(path, capacity, "%s/block-%d.elec", app->selftest ? "build/test-blocks" : "blocks", index + 1);
}

/* Free name in the trash: deletions of the same slot within one second get a counter suffix. */
static int trash_target(const char *trash, int index, char *target, size_t capacity)
{
    long long stamp = (long long)time(NULL);
    struct _stat existing;
    snprintf(target, capacity, "%s/block-%d-%lld.elec", trash, index + 1, stamp);
    for (int attempt = 1; _stat(target, &existing) == 0; ++attempt)
    {
        if (attempt >= TRASH_NAME_ATTEMPTS)
        {
            errno = EEXIST;
            return 0;
        }
        snprintf(target, capacity, "%s/block-%d-%lld-%d.elec", trash, index + 1, stamp, attempt);
    }
    return 1;
}

void app_packing_refresh(AppState *app)
{
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
    {
        char path[128];
        block_path(app, i, path, sizeof(path));
        struct _stat metadata;
        app->library_available[i] = _stat(path, &metadata) == 0 || errno != ENOENT;
        snprintf(app->library_names[i], sizeof(app->library_names[i]), tr(TXT_CHIP_DEFAULT_NAME), i + 1);
        app->library_models[i] = 0;
        if (app->library_available[i])
        {
            Workbench candidate BENCH_AUTO_CLEAR = {0};
            WorkbenchSettings settings;
            if (workbench_load_project(path, &candidate, &settings) && candidate.part_count && candidate.parts[0].group)
            {
                const BenchGroup *group = &candidate.groups[candidate.parts[0].group - 1];
                memcpy(app->library_names[i], group->name, sizeof(app->library_names[i]));
                app->library_models[i] = group->model;
            }
            else
                snprintf(app->library_names[i], sizeof(app->library_names[i]), "%s",
                         tr(TXT_APP_CHIP_FILE_UNAVAILABLE));
        }
    }
}

static int model_listed(const AppState *app, uint64_t model)
{
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app->library_available[i] && app->library_models[i] == model)
            return 1;
    return 0;
}

uint64_t app_packing_new_model(AppState *app)
{
    static uint64_t sequence;
    app_packing_refresh(app);
    uint64_t model;
    do
        model = ((uint64_t)time(NULL) << 16) + (++sequence & 0xffff);
    while (!model || model_listed(app, model));
    return model;
}

/* Legacy chips (no identity) match a listed block only by a name no other block shares. */
static int unique_name_slot(const AppState *app, const char *name)
{
    int found = -1;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app->library_available[i] && !strcmp(app->library_names[i], name))
        {
            if (found >= 0)
                return -1;
            found = i;
        }
    return found;
}

int app_packing_model_slot(AppState *app, uint64_t model, const char *legacy_name)
{
    app_packing_refresh(app);
    if (!model)
        return legacy_name ? unique_name_slot(app, legacy_name) : -1;
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
        if (app->library_available[i] && app->library_models[i] == model)
            return i;
    return -1;
}

int app_packing_rename_model(AppState *app, uint64_t model, const char *legacy_name, const char *name)
{
    int slot = app_packing_model_slot(app, model, legacy_name);
    if (slot < 0)
        return 1;
    char path[128];
    block_path(app, slot, path, sizeof(path));
    Workbench saved BENCH_AUTO_CLEAR = {0};
    WorkbenchSettings settings;
    if (!workbench_load_project(path, &saved, &settings) || !saved.part_count || !saved.parts[0].group ||
        !bench_group_set_name(&saved, saved.parts[0].group, name))
    {
        snprintf(app->selection_status, sizeof(app->selection_status), "%s: %s",
                 tr(TXT_APP_CHIP_FILE_UNAVAILABLE), path);
        return 0;
    }
    if (!workbench_save_project(path, &saved, &settings))
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_CHIP_SAVE_FAILED), path);
        return 0;
    }
    app_packing_refresh(app);
    return 1;
}

int app_packing_store_model(AppState *app, const Workbench *model, int *slot)
{
    const char *directory = app->selftest ? "build/test-blocks" : "blocks";
    if (_mkdir(directory) != 0 && errno != EEXIST)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_LIBRARY_FOLDER_FAILED),
                 strerror(errno));
        return -1;
    }
    app_packing_refresh(app);
    int target = *slot;
    if (target < 0)
        for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
            if (!app->library_available[i])
            {
                target = i;
                break;
            }
    if (target < 0 || target >= BLOCK_LIBRARY_SLOTS)
        return 0;
    char path[128];
    block_path(app, target, path, sizeof(path));
    WorkbenchSettings settings = workbench_settings_default();
    if (!workbench_save_project(path, model, &settings))
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_CHIP_SAVE_FAILED), path);
        return -1;
    }
    *slot = target;
    app_packing_refresh(app);
    return 1;
}

static int store_block(AppState *app)
{
    if (!bench_selection_copy(&app->bench, &app->selection, &app->library_template))
        return -1;
    int slot = -1;
    return app_packing_store_model(app, &app->library_template.bench, &slot);
}

int app_packing_key(AppState *app, SDL_Keycode key, SDL_Keymod mod)
{
    if (key != SDLK_G || !(mod & SDL_KMOD_CTRL))
        return 0;
    if (!bench_selection_count(&app->selection, &app->bench) && app->selected >= 0)
        app->selection.parts[app->selected] = 1;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 1;
    }
    if (mod & SDL_KMOD_SHIFT)
    {
        if (bench_unpack_selection(&app->bench, &app->selection))
        {
            if (!app_history_record(app, &before))
                return 1;
            app->status = tr(TXT_APP_CHIP_UNPACKED);
        }
        else
            app->status = tr(TXT_APP_SELECT_CHIP_TO_UNPACK);
        return 1;
    }
    int group = bench_pack_selection(&app->bench, &app->selection, app->selection_status,
                                     sizeof(app->selection_status));
    if (!group)
    {
        app->status = app->selection_status;
        return 1;
    }
    if (!app_history_record(app, &before))
        return 1;
    /* The new instance and its library entry share an identity, so editing it later updates that entry. */
    app->bench.groups[group - 1].model = app_packing_new_model(app);
    int stored = store_block(app);
    if (stored < 0)
        app->status = app->selection_status;
    else if (!stored)
        app->status = tr(TXT_APP_CHIP_CREATED_NOT_SAVED);
    else
        app->status = tr(TXT_APP_CHIP_CREATED);
    app->palette_page = 2;
    app->pending.component = -1;
    app->route_count = 0;
    app->dragging = -1;
    app->drag_wire = -1;
    return 1;
}

void app_packing_choose(AppState *app, int index)
{
    if (index < 0 || index >= BLOCK_LIBRARY_SLOTS)
        return;
    char path[128];
    WorkbenchSettings settings;
    Workbench candidate BENCH_AUTO_CLEAR = {0};
    app->packed_placing = 0;
    block_path(app, index, path, sizeof(path));
    if (!workbench_load_project(path, &candidate, &settings) || !candidate.part_count || !candidate.parts[0].group)
    {
        app->status = tr(TXT_APP_CHIP_FILE_MISSING);
        return;
    }
    for (int i = 0; i < candidate.part_count; ++i)
        if (candidate.parts[i].group != candidate.parts[0].group)
        {
            app->status = tr(TXT_APP_CHIP_FILE_INVALID);
            return;
        }
    bench_snapshot_move(&app->library_template.bench, &candidate);
    app->library_template.valid = 1;
    app->library_index = index;
    app->packed_placing = 1;
    app->placing = -1;
    app->pending.component = -1;
    app->route_count = 0;
    app->status = tr(TXT_APP_CLICK_TO_PLACE_CHIP);
}

void app_packing_place(AppState *app, float x, float y)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (!bench_selection_paste(&app->bench, &app->selection, &app->library_template, x, y))
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_NO_ROOM), WORKBENCH_MAX_COMPONENTS,
                 WORKBENCH_MAX_WIRES);
        app->status = app->selection_status;
        return;
    }
    if (!app_history_record(app, &before))
        return;
    app->selected = before.part_count;
    app->selected_wire = -1;
    app->packed_placing = 0;
    app->status = tr(TXT_APP_CHIP_PLACED);
}

/* Two-step removal from the library: the first request arms the slot, the second moves the file to
 * <library>/lixeira so it can still be recovered by hand. Placed instances are independent copies. */
int app_packing_delete(AppState *app, int index)
{
    if (index < 0 || index >= BLOCK_LIBRARY_SLOTS || !app->library_available[index])
        return 0;
    if (app->library_delete_armed != index)
    {
        app->library_delete_armed = index;
        snprintf(app->selection_status, sizeof(app->selection_status),
                 tr(TXT_APP_CONFIRM_REMOVE_CHIP), app->library_names[index]);
        app->status = app->selection_status;
        return 0;
    }
    app->library_delete_armed = -1;
    const char *library = app->selftest ? "build/test-blocks" : "blocks";
    char path[128], trash[160], target[224];
    block_path(app, index, path, sizeof(path));
    snprintf(trash, sizeof(trash), "%s/lixeira", library);
    int trash_ready = _mkdir(trash) == 0 || errno == EEXIST;
    if (!trash_ready || !trash_target(trash, index, target, sizeof(target)) || rename(path, target) != 0)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_CHIP_REMOVE_FAILED),
                 strerror(errno));
        app->status = app->selection_status;
        return 0;
    }
    if (app->library_index == index)
        app->packed_placing = 0;
    app_packing_refresh(app);
    app->status = tr(TXT_APP_CHIP_REMOVED);
    return 1;
}
