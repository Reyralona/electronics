#include "app_editing.h"
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
                      ? "Selecao copiada com seus fios internos. Ctrl+V cola na posicao do mouse."
                      : "Selecione componentes para copiar.";
}

static void delete_selection(AppState *app)
{
    if (!bench_selection_count(&app->selection, &app->bench))
        return;
    bench_history_record(app->history, &app->bench);
    bench_selection_delete(&app->bench, &app->selection);
    reset_interaction(app);
    app->status = "Selecao e fios ligados a ela removidos. Ctrl+Z desfaz.";
}

static void paste_selection(AppState *app)
{
    Workbench before = app->bench;
    if (!bench_selection_paste(&app->bench, &app->selection, &app->clipboard, app->mouse_world.x, app->mouse_world.y))
    {
        app->status = app->clipboard.valid ? "Nao cabe: limite atual de 64 componentes ou 128 fios."
                                           : "Copie uma selecao com Ctrl+C antes de colar.";
        return;
    }
    bench_history_record(app->history, &before);
    reset_interaction(app);
    app->selected = before.part_count;
    app->mouse_world.x += 24;
    app->mouse_world.y += 24;
    app->status = "Selecao colada. Arraste para mover; Ctrl+Z desfaz.";
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
        app->status = "Todos os componentes selecionados. Ctrl+C/X/V; Delete remove.";
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
        snprintf(app->selection_status, sizeof(app->selection_status), "Falha ao criar slots: %s", strerror(errno));
        app->status = app->selection_status;
        return;
    }
    WorkbenchSettings settings = {app->show_grid,     app->show_handles,   app->round_wires,
                                  app->show_current,  app->wire_color,     app->probe_pin,
                                  app->probe_current, app->transient_step, app->transient_stop,
                                  app->grid_spacing, app->snap_grid, app->snap_board};
    slot_path(app, app->save_slot, path, sizeof(path));
    int saved = workbench_save_project(path, &app->bench, &settings);
    snprintf(app->selection_status, sizeof(app->selection_status),
             saved ? "Montagem e configuracoes salvas no slot %d."
                   : "Falha ao gravar slot %d; arquivo anterior preservado.",
             app->save_slot + 1);
    app->status = app->selection_status;
    app_projects_refresh(app);
}

void app_projects_load(AppState *app)
{
    char path[128];
    Workbench before = app->bench;
    WorkbenchSettings settings;
    slot_path(app, app->save_slot, path, sizeof(path));
    if (!workbench_load_project(path, &app->bench, &settings))
    {
        app->status = "Slot vazio ou invalido; montagem atual preservada.";
        return;
    }
    bench_history_record(app->history, &before);
    reset_interaction(app);
    bench_selection_clear(&app->selection);
    app->show_grid = settings.show_grid;
    app->show_handles = settings.show_handles;
    app->round_wires = settings.round_wires;
    app->show_current = settings.show_current;
    app->wire_color = settings.wire_color;
    app->probe_pin = settings.probe_pin;
    app->probe_current = settings.probe_current;
    app->transient_step = settings.transient_step;
    app->transient_stop = settings.transient_stop;
        app->grid_spacing = settings.grid_spacing;
        app->snap_grid = settings.snap_grid;
        app->snap_board = settings.snap_board;
    sim_state_clear_result(&app->sim);
    memset(app->leds, 0, sizeof(app->leds));
    app->projects_open = 0;
    app->status = "Slot carregado; Ctrl+Z restaura a montagem anterior.";
}
