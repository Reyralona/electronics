#include "app_editing.h"
#include "bench_scope.h"
#include "app_editor.h"
#include "ui_text.h"
#include "app_packing.h"
#include "app_layout.h"
#include "workbench_group_edit.h"
#include "workbench_group_name.h"
#include "workbench_pack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fit_draft(AppState *app)
{
    float left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
    for (int i = 0; i < app->bench.part_count; ++i)
    {
        const Part *part = &app->bench.parts[i];
        float width = part->type == PART_BREADBOARD_WIDE ? 440 : bench_is_board(part->type) ? 230 : 55;
        float height = bench_is_board(part->type) ? 115 : 40;
        width *= bench_part_scale(part);
        height *= bench_part_scale(part);
        float x = part->x;
        float y = part->y;
        if (part->group)
        {
            x = app->bench.groups[part->group - 1].x;
            y = app->bench.groups[part->group - 1].y;
            width = bench_group_width(&app->bench, part->group) * .5f + 11;
            height = bench_group_height(&app->bench, part->group) * .5f;
        }
        left = fminf(left, x - width);
        right = fmaxf(right, x + width);
        top = fminf(top, y - height);
        bottom = fmaxf(bottom, y + height);
    }
    float zoom = fminf(1.2f, fminf(980 / (right - left), 510 / (bottom - top)));
    app->view = (BenchView){zoom, 750 - 220 - ((left + right) * .5f - 220) * zoom,
                            365 - 70 - ((top + bottom) * .5f - 70) * zoom};
}

AppState *app_editor_open(AppState *parent)
{
    if (parent->selected < 0 || parent->selected >= parent->bench.part_count ||
        !parent->bench.parts[parent->selected].group)
    {
        parent->status = tr(TXT_APP_SELECT_CHIP_TO_EDIT);
        return NULL;
    }
    AppState *child = malloc(sizeof(*child));
    if (!child)
    {
        parent->status = tr(TXT_APP_EDITOR_NO_MEMORY);
        return NULL;
    }
    app_state_init(child);
    int group = parent->bench.parts[parent->selected].group;
    if (!child->history || !bench_group_extract(&parent->bench, group, &child->bench))
    {
        app_state_dispose(child);
        free(child);
        parent->status = tr(TXT_APP_EDITOR_COPY_FAILED);
        return NULL;
    }
    child->editor_parent = parent;
    child->editor_group = group;
    child->editor_model = parent->bench.groups[group - 1].model;
    child->editor_parent_revision = parent->bench.revision;
    child->editor_initial_revision = child->editor_saved_revision = child->bench.revision;
    child->editor_opened_at = SDL_GetTicks();
    memcpy(child->editor_name, parent->bench.groups[group - 1].name, sizeof(child->editor_name));
    memcpy(child->editor_initial_name, child->editor_name, sizeof(child->editor_name));
    memcpy(child->editor_saved_name, child->editor_name, sizeof(child->editor_name));
    memcpy(child->ngspice, parent->ngspice, sizeof(child->ngspice));
    child->selftest = parent->selftest;
    child->show_grid = parent->show_grid;
    child->grid_spacing = parent->grid_spacing;
    child->snap_grid = parent->snap_grid;
    child->snap_board = parent->snap_board;
    child->dark_theme = parent->dark_theme;
    child->inspector_minimized = parent->inspector_minimized;
    child->gui_font_size = parent->gui_font_size;
    child->show_handles = parent->show_handles;
    child->round_wires = parent->round_wires;
    child->sim.auto_enabled = parent->sim.auto_enabled;
    child->sim.sim_running = 0;
    child->status = tr(TXT_APP_EDITOR_OPENED);
    fit_draft(child);
    parent->editor_child = child;
    return child;
}

int app_editor_dirty(const AppState *app)
{
    return app->editor_parent &&
           (app->bench.revision != app->editor_saved_revision || strcmp(app->editor_name, app->editor_saved_name));
}

void app_editor_report_invalid_name(AppState *app)
{
    snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_NAME_INVALID),
             BENCH_GROUP_NAME_CAPACITY - 1);
    app->status = app->selection_status;
}

int app_editor_set_name(AppState *app, const char *name)
{
    if (!bench_group_name_valid(name))
    {
        app_editor_report_invalid_name(app);
        return 0;
    }
    AppState *parent = app->editor_parent;
    int group = app->editor_group;
    if (!parent || group < 1 || group > WORKBENCH_MAX_COMPONENTS || !parent->bench.groups[group - 1].active)
    {
        app->status = tr(TXT_APP_SELECT_CHIP_TO_RENAME);
        return 0;
    }
    if (parent->bench.revision != app->editor_parent_revision)
    {
        app->status = tr(TXT_ENG_EDIT_BENCH_CHANGED);
        return 0;
    }
    if (!strcmp(app->editor_name, name))
        return 1;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &parent->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 0;
    }
    if (!app_packing_rename_model(app, app->editor_model, app->editor_initial_name, name))
    {
        app->status = app->selection_status;
        return 0;
    }
    bench_group_set_name(&parent->bench, group, name);
    int recorded = bench_history_record(parent->history, &before);
    app->editor_parent_revision = parent->bench.revision;
    memcpy(app->editor_name, name, strlen(name) + 1);
    memcpy(app->editor_initial_name, name, strlen(name) + 1);
    memcpy(app->editor_saved_name, name, strlen(name) + 1);
    if (!recorded)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), "%s %s",
                 tr(TXT_APP_NAME_CHANGED), tr(TXT_APP_HISTORY_FAILED));
        app->status = app->selection_status;
        return 1;
    }
    app->status = tr(TXT_APP_NAME_CHANGED);
    return 1;
}

int app_editor_begin_name(AppState *app, SDL_Window *window)
{
    if (!app->editor_parent)
    {
        app->status = tr(TXT_APP_SELECT_CHIP_TO_RENAME);
        return 0;
    }
    if (!window || !SDL_StartTextInput(window))
    {
        app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
        return 0;
    }
    app->name_target_group = 0;
    app->editing = APP_EDIT_GROUP_NAME;
    app->edit_error = 0;
    app->edit_select_all = 1;
    snprintf(app->edit_text, sizeof(app->edit_text), "%s",
             app->editor_name);
    app->status = tr(TXT_APP_TYPE_CHIP_NAME);
    return 1;
}

/* Library slot this chip saves into (-1 = a new entry). A legacy chip found by its listed name adopts
 * that entry's identity; a chip whose entry was removed keeps its identity and gets a new entry. */
static int editor_library_slot(AppState *app)
{
    int slot = app_packing_model_slot(app, app->editor_model, app->editor_initial_name);
    if (!app->editor_model && slot >= 0)
        app->editor_model = app->library_models[slot];
    if (!app->editor_model)
        app->editor_model = app_packing_new_model(app);
    return slot;
}

/* Saving the model links the outer instance to it; only identity changes, not the circuit. */
static void link_parent_instance(AppState *app)
{
    AppState *parent = app->editor_parent;
    int group = app->editor_group;
    if (parent && group >= 1 && group <= WORKBENCH_MAX_COMPONENTS && parent->bench.groups[group - 1].active)
        parent->bench.groups[group - 1].model = app->editor_model;
}

int app_editor_save(AppState *app)
{
    Workbench model BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&model, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 0;
    }
    BenchSelection all = {0};
    for (int i = 0; i < model.part_count; ++i)
        all.parts[i] = 1;
    int group = bench_pack_selection(&model, &all, app->selection_status, sizeof(app->selection_status));
    if (!group || !bench_group_set_name(&model, group, app->editor_name))
    {
        app->status = app->selection_status;
        return 0;
    }
    int slot = editor_library_slot(app);
    model.groups[group - 1].model = app->editor_model;
    int saved = app_packing_store_model(app, &model, &slot);
    if (saved < 1)
    {
        if (!saved)
            snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_APP_LIBRARY_FULL),
                     BLOCK_LIBRARY_SLOTS);
        app->status = app->selection_status;
        return 0;
    }
    link_parent_instance(app);
    app->editor_saved_revision = app->bench.revision;
    memcpy(app->editor_saved_name, app->editor_name, sizeof(app->editor_saved_name));
    app->status = tr(TXT_APP_CHIP_SAVED);
    return 1;
}

int app_editor_apply(AppState *app)
{
    AppState *parent = app->editor_parent;
    if (!parent)
        return 0;
    if (app->bench.revision == app->editor_initial_revision && !strcmp(app->editor_name, app->editor_initial_name))
    {
        app->editor_close_requested = 1;
        return 1;
    }
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &parent->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 0;
    }
    if (!bench_group_apply(&parent->bench, app->editor_group, app->editor_parent_revision, &app->bench,
                           app->editor_name, app->selection_status, sizeof(app->selection_status)))
    {
        app->status = app->selection_status;
        return 0;
    }
    if (!app_history_record(parent, &before))
    {
        app->status = parent->status;
        return 0;
    }
    sim_live_graph_close(&parent->live_graph);
    parent->selected = bench_group_first(&parent->bench, app->editor_group);
    parent->selected_wire = -1;
    parent->pending.component = -1;
    parent->route_count = 0;
    bench_selection_clear(&parent->selection);
    parent->selection.parts[parent->selected] = 1;
    bench_selection_expand_groups(&parent->selection, &parent->bench);
    parent->status = tr(TXT_APP_CHIP_APPLIED);
    app->editor_close_requested = 1;
    return 1;
}

void app_editor_request_close(AppState *app, int quit)
{
    app->editor_quit_requested = quit;
    app->dragging = app->drag_wire = -1;
    app->selecting_area = app->panning = 0;
    if (app_editor_dirty(app))
        app->editor_confirm_close = 1;
    else
        app->editor_close_requested = 1;
}

void app_editor_confirm(AppState *app, int choice)
{
    if (choice == 2)
    {
        app->editor_confirm_close = 0;
        app->editor_quit_requested = 0;
    }
    else if (choice == 1 || (choice == 0 && app_editor_save(app)))
        app->editor_close_requested = 1;
}

AppState *app_editor_dispose(AppState *active)
{
    AppState *parent = active->editor_parent;
    if (!parent)
        return active;
    spice_job_join(&active->dc_job);
    if (sim_state_has_result(&parent->sim, SIM_RESULT_TRACE) && parent->sim.sim_running)
        parent->sim.trace_play_started += SDL_GetTicks() - active->editor_opened_at;
    parent->editor_child = NULL;
    app_packing_refresh(parent);
    if (active->editor_quit_requested)
        parent->running = 0;
    app_state_dispose(active);
    free(active);
    return parent;
}

AppState *app_editor_route(AppState *active)
{
    if (active->editor_open_requested)
    {
        active->editor_open_requested = 0;
        AppState *child = app_editor_open(active);
        return child ? child : active;
    }
    if (active->editor_parent && !active->running)
    {
        active->running = 1;
        app_editor_request_close(active, 1);
    }
    if (active->editor_parent && active->editor_close_requested)
        return app_editor_dispose(active);
    return active;
}

int app_editor_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    (void)window;
    if (app->editor_confirm_close)
    {
        if (key == SDLK_ESCAPE)
            app_editor_confirm(app, 2);
        return 1;
    }
    if (!(mod & SDL_KMOD_CTRL) && key == SDLK_RETURN && app->selected >= 0 && app->bench.parts[app->selected].group)
    {
        app->editor_open_requested = 1;
        return 1;
    }
    if (!app->editor_parent)
        return 0;
    if (key == SDLK_ESCAPE)
        app_editor_request_close(app, 0);
    else if (key == SDLK_S)
        app_editor_save(app);
    else if ((key == SDLK_RETURN && (mod & SDL_KMOD_CTRL)))
        app_editor_apply(app);
    else if (key == SDLK_F3 || key == SDLK_L)
        app->status = tr(TXT_APP_EDITOR_NO_PROJECTS);
    else
        return 0;
    return 1;
}

int app_editor_click(AppState *app, SDL_Window *window, float x, float y)
{
    if (app->editor_confirm_close)
    {
        int choice = app_layout_confirm_button_at(x, y);
        if (choice >= 0)
            app_editor_confirm(app, choice);
        return 1;
    }
    if (app->editor_parent)
    {
        switch (app_layout_editor_header_at(x, y))
        {
        case APP_EDITOR_APPLY:
            app_editor_apply(app);
            return 1;
        case APP_EDITOR_RENAME:
            app_editor_begin_name(app, window);
            return 1;
        case APP_EDITOR_SAVE:
            app_editor_save(app);
            return 1;
        case APP_EDITOR_BACK:
            app_editor_request_close(app, 0);
            return 1;
        default:
            break;
        }
        /* The editor header replaces the bench header; its free space must not reach bench actions. */
        if (y < APP_HEADER_HEIGHT && x < app_layout_header(APP_HEADER_PLAY).x)
            return 1;
    }
    if (app->inspector_minimized || app->selected < 0 || !app->bench.parts[app->selected].group)
        return 0;
    int button = app_layout_group_button_at(x, y);
    if (button == 0)
        app->editor_open_requested = 1;
    return button >= 0;
}
