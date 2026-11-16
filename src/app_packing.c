#include "bench_scope.h"
#include "app_packing.h"
#include "workbench_pack.h"
#include "workbench_file.h"
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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
                strcpy(app->library_names[i], "Arquivo indisponivel");
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
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
        return 1;
    }
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
    int group = bench_pack_selection(&app->bench, &app->selection, app->selection_status,
                                     sizeof(app->selection_status));
    if (!group)
    {
        app->status = app->selection_status;
        return 1;
    }
    bench_history_record(app->history, &before);
    /* The new instance and its library entry share an identity, so editing it later updates that entry. */
    app->bench.groups[group - 1].model = app_packing_new_model(app);
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
    Workbench candidate BENCH_AUTO_CLEAR = {0};
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
    bench_snapshot_move(&app->library_template.bench, &candidate);
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
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
        return;
    }
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
                 "Clique de novo no x para remover \"%.60s\" da biblioteca.", app->library_names[index]);
        app->status = app->selection_status;
        return 0;
    }
    app->library_delete_armed = -1;
    const char *library = app->selftest ? "build/test-blocks" : "blocks";
    char path[128], trash[160], target[224];
    block_path(app, index, path, sizeof(path));
    snprintf(trash, sizeof(trash), "%s/lixeira", library);
    snprintf(target, sizeof(target), "%s/block-%d-%lld.elec", trash, index + 1, (long long)time(NULL));
    if ((_mkdir(trash) != 0 && errno != EEXIST) || rename(path, target) != 0)
    {
        snprintf(app->selection_status, sizeof(app->selection_status), "Não foi possível remover o bloco: %s",
                 strerror(errno));
        app->status = app->selection_status;
        return 0;
    }
    if (app->library_index == index)
        app->packed_placing = 0;
    app_packing_refresh(app);
    app->status = "Bloco removido da biblioteca (cópia em lixeira). Instâncias na bancada continuam.";
    return 1;
}
