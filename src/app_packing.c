#include "app_packing.h"
#include "workbench_pack.h"
#include "workbench_file.h"
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void block_path(const AppState *app, int index, char *path, size_t capacity)
{
    snprintf(path, capacity, "%s/block-%d.elec", app->selftest ? "build/test-blocks" : "blocks", index + 1);
}

void app_packing_refresh(AppState *app)
{
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
    {
        char path[128];
        block_path(app, i, path, sizeof(path));
        struct _stat metadata;
        app->library_available[i] = _stat(path, &metadata) == 0 || errno != ENOENT;
        snprintf(app->library_names[i], sizeof(app->library_names[i]), "Bloco %d", i + 1);
        if (app->library_available[i])
        {
            Workbench candidate = {0};
            WorkbenchSettings settings;
            if (workbench_load_project(path, &candidate, &settings) && candidate.part_count && candidate.parts[0].group)
                memcpy(app->library_names[i], candidate.groups[candidate.parts[0].group - 1].name,
                       sizeof(app->library_names[i]));
            else
                strcpy(app->library_names[i], "Arquivo indisponivel");
        }
    }
}

int app_packing_store_model(AppState *app, const Workbench *model, int *slot)
{
    const char *directory = app->selftest ? "build/test-blocks" : "blocks";
    if (_mkdir(directory) != 0 && errno != EEXIST)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), "Falha ao criar biblioteca: %s",
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
        snprintf(app->selection_status, sizeof(app->selection_status), "Falha ao gravar bloco: %s", path);
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
    Workbench before = app->bench;
    if (mod & SDL_KMOD_SHIFT)
    {
        if (bench_unpack_selection(&app->bench, &app->selection))
        {
            bench_history_record(app->history, &before);
            app->status = "Interior restaurado. Os fios externos permanecem ligados aos terminais originais.";
        }
        else
            app->status = "Selecione um bloco para desempacotar.";
        return 1;
    }
    if (!bench_pack_selection(&app->bench, &app->selection, app->selection_status, sizeof(app->selection_status)))
    {
        app->status = app->selection_status;
        return 1;
    }
    bench_history_record(app->history, &before);
    int stored = store_block(app);
    if (stored < 0)
        app->status = app->selection_status;
    else if (!stored)
        app->status = "Bloco criado; biblioteca cheia (8 slots). Ctrl+C/V ainda copia o bloco.";
    else
        app->status = "Bloco criado e salvo na aba Blocos [Tab]. Ctrl+Shift+G desempacota na bancada.";
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
    Workbench candidate = {0};
    app->packed_placing = 0;
    block_path(app, index, path, sizeof(path));
    if (!workbench_load_project(path, &candidate, &settings) || !candidate.part_count || !candidate.parts[0].group)
    {
        app->status = "Bloco ausente ou invalido. Selecione pecas e use Ctrl+G para criar um.";
        return;
    }
    for (int i = 0; i < candidate.part_count; ++i)
        if (candidate.parts[i].group != candidate.parts[0].group)
        {
            app->status = "Biblioteca invalida: o arquivo precisa conter um unico bloco.";
            return;
        }
    app->library_template.bench = candidate;
    app->library_template.valid = 1;
    app->library_index = index;
    app->packed_placing = 1;
    app->placing = -1;
    app->pending.component = -1;
    app->route_count = 0;
    app->status = "Clique na bancada para colocar uma instancia independente do bloco.";
}

void app_packing_place(AppState *app, float x, float y)
{
    Workbench before = app->bench;
    const Workbench *model = &app->library_template.bench;
    int group = model->parts[0].group;
    if (group)
    {
        x += model->parts[0].x - model->groups[group - 1].x;
        y += model->parts[0].y - model->groups[group - 1].y;
    }
    if (!bench_selection_paste(&app->bench, &app->selection, &app->library_template, x, y))
    {
        app->status = "Instancia nao cabe: o limite atual conta 64 pecas internas e 128 fios.";
        return;
    }
    bench_history_record(app->history, &before);
    app->selected = before.part_count;
    app->selected_wire = -1;
    app->packed_placing = 0;
    app->status = "Bloco colocado. Conecte seus terminais; Ctrl+Shift+G desempacota na bancada.";
}
