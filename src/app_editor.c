#include "bench_scope.h"
#include "app_editor.h"
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
        float width = part->type == PART_BREADBOARD ? 230 : 55;
        float height = part->type == PART_BREADBOARD ? 115 : 40;
        width *= bench_part_scale(part);
        height *= bench_part_scale(part);
        left = fminf(left, part->x - width);
        right = fmaxf(right, part->x + width);
        top = fminf(top, part->y - height);
        bottom = fmaxf(bottom, part->y + height);
    }
    float zoom = fminf(1.2f, fminf(980 / (right - left), 510 / (bottom - top)));
    app->view = (BenchView){zoom, 750 - 220 - ((left + right) * .5f - 220) * zoom,
                            365 - 70 - ((top + bottom) * .5f - 70) * zoom};
}

AppState *app_editor_open(AppState *parent)
{
    if (parent->editor_parent || parent->selected < 0 || parent->selected >= parent->bench.part_count ||
        !parent->bench.parts[parent->selected].group)
    {
        parent->status = "Selecione um bloco na bancada para abrir o editor isolado.";
        return NULL;
    }
    AppState *child = malloc(sizeof(*child));
    if (!child)
    {
        parent->status = "Memoria insuficiente para abrir o editor isolado.";
        return NULL;
    }
    app_state_init(child);
    int group = parent->bench.parts[parent->selected].group;
    if (!child->history || !bench_group_extract(&parent->bench, group, &child->bench))
    {
        app_state_dispose(child);
        free(child);
        parent->status = "Nao foi possivel copiar o interior para o editor isolado.";
        return NULL;
    }
    child->editor_parent = parent;
    child->editor_group = group;
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
    child->gui_font_size = parent->gui_font_size;
    child->show_handles = parent->show_handles;
    child->round_wires = parent->round_wires;
    child->sim.auto_enabled = parent->sim.auto_enabled;
    child->sim.sim_running = 0;
    child->status = "EDITOR ISOLADO: a bancada externa esta preservada. Salvar modelo e aplicar sao acoes separadas.";
    fit_draft(child);
    parent->editor_child = child;
    return child;
}

int app_editor_dirty(const AppState *app)
{
    return app->editor_parent &&
           (app->bench.revision != app->editor_saved_revision || strcmp(app->editor_name, app->editor_saved_name));
}

int app_editor_set_name(AppState *app, const char *name)
{
    if (!bench_group_name_valid(name))
    {
        snprintf(app->selection_status, sizeof(app->selection_status),
                 "Nome invalido: use UTF-8 sem controles e ate %d bytes.", BENCH_GROUP_NAME_CAPACITY - 1);
        app->status = app->selection_status;
        return 0;
    }
    if (app->editor_parent)
        memcpy(app->editor_name, name, strlen(name) + 1);
    else
    {
        int group = app->name_target_group;
        Workbench before BENCH_AUTO_CLEAR = {0};
        if (!bench_snapshot_clone(&before, &app->bench))
        {
            app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
            return 0;
        }
        if (!bench_group_set_name(&app->bench, group, name))
        {
            app->status = "Selecione um bloco valido para renomear.";
            return 0;
        }
        if (app->bench.revision != before.revision)
            bench_history_record(app->history, &before);
    }
    app->status = "Nome atualizado. Salve para conservar a alteracao.";
    return 1;
}

int app_editor_begin_name(AppState *app, SDL_Window *window)
{
    int group = app->selected >= 0 && app->selected < app->bench.part_count ? app->bench.parts[app->selected].group : 0;
    if (!app->editor_parent && !group)
    {
        app->status = "Selecione um bloco para renomear.";
        return 0;
    }
    if (!window || !SDL_StartTextInput(window))
    {
        app->status = "Nao foi possivel iniciar a edicao do nome.";
        return 0;
    }
    app->name_target_group = app->editor_parent ? 0 : group;
    app->editing = APP_EDIT_GROUP_NAME;
    app->edit_select_all = 1;
    snprintf(app->edit_text, sizeof(app->edit_text), "%s",
             app->editor_parent ? app->editor_name : app->bench.groups[group - 1].name);
    app->status = "Digite o nome do chip. Enter confirma; Esc cancela.";
    return 1;
}

int app_editor_save(AppState *app)
{
    Workbench model BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&model, &app->bench))
    {
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
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
    int saved = app_packing_store_model(app, &model, &app->editor_library_slot);
    if (saved < 1)
    {
        app->status = saved < 0 ? app->selection_status : "Biblioteca cheia (8 slots); modelo nao foi salvo.";
        return 0;
    }
    app->editor_saved_revision = app->bench.revision;
    memcpy(app->editor_saved_name, app->editor_name, sizeof(app->editor_saved_name));
    app->status = "Modelo salvo na biblioteca. A bancada externa continua preservada.";
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
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
        return 0;
    }
    if (!bench_group_apply(&parent->bench, app->editor_group, app->editor_parent_revision, &app->bench,
                           app->editor_name, app->selection_status, sizeof(app->selection_status)))
    {
        app->status = app->selection_status;
        return 0;
    }
    bench_history_record(parent->history, &before);
    parent->selected = bench_group_first(&parent->bench, app->editor_group);
    parent->selected_wire = -1;
    parent->pending.component = -1;
    parent->route_count = 0;
    bench_selection_clear(&parent->selection);
    parent->selection.parts[parent->selected] = 1;
    bench_selection_expand_groups(&parent->selection, &parent->bench);
    parent->status = "Modelo aplicado nesta instancia. Ctrl+Z desfaz; outras instancias foram preservadas.";
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
    if (app->editor_confirm_close)
    {
        if (key == SDLK_ESCAPE)
            app_editor_confirm(app, 2);
        return 1;
    }
    if (key == SDLK_F4)
    {
        app_editor_begin_name(app, window);
        return 1;
    }
    if (!app->editor_parent && key == SDLK_RETURN && app->selected >= 0 && app->bench.parts[app->selected].group)
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
        app->status = "No editor use Salvar modelo; slots e arquivos da bancada externa ficam separados.";
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
        return y < APP_HEADER_HEIGHT && x < app_layout_header(APP_HEADER_PLAY).x;
    }
    if (app->selected < 0 || !app->bench.parts[app->selected].group)
        return 0;
    int button = app_layout_group_button_at(x, y);
    if (button == 0)
        app->editor_open_requested = 1;
    else if (button == 1)
        app_editor_begin_name(app, window);
    return button >= 0;
}
