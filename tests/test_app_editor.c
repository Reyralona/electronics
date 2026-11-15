#include "app_editor.h"
#include "app_simulation.h"
#include "workbench_group_name.h"
#include "workbench_pack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int SDLCALL completed(void *unused)
{
    (void)unused;
    return 0;
}

static void completed_result(AppState *app, double current)
{
    app->dc_job.snapshot = app->bench;
    app->dc_job.success = 1;
    app->dc_job.current = current;
    app->dc_job.thread = SDL_CreateThread(completed, "editor-test", NULL);
    assert(app->dc_job.thread);
    SDL_SetAtomicInt(&app->dc_job.done, 1);
}

int main(void)
{
    static AppState root;
    app_state_init(&root);
    assert(root.history);
    root.selftest = 1;
    root.sim.sim_running = 0;
    bench_add(&root.bench, PART_SOURCE, 300, 200);
    bench_add(&root.bench, PART_RESISTOR, 500, 200);
    bench_add(&root.bench, PART_LED, 700, 200);
    bench_toggle_wire(&root.bench, (Pin){0, 0}, (Pin){1, 0});
    bench_toggle_wire(&root.bench, (Pin){1, 1}, (Pin){2, 0});
    bench_toggle_wire(&root.bench, (Pin){2, 1}, (Pin){0, 1});
    root.selection.parts[1] = root.selection.parts[2] = 1;
    assert(bench_pack_selection(&root.bench, &root.selection, root.selection_status, sizeof(root.selection_status)));
    root.selected = 1;
    Workbench before = root.bench;
    BenchView view = root.view;
    int selected = root.selected, history = root.history->undo_count;
    completed_result(&root, .001);
    AppState *child = app_editor_open(&root);
    assert(child && child->history != root.history && !child->dc_job.thread);
    assert(!app_editor_dirty(child));
    assert(bench_set_value(&child->bench, 0, 2200));
    child->show_grid = 0;
    completed_result(child, .002);
    app_simulation_update(child);
    assert(child->sim.dc_current == .002 && root.sim.dc_current == 0 && root.dc_job.thread);
    assert(memcmp(&before, &root.bench, sizeof(before)) == 0);
    assert(!memcmp(&view, &root.view, sizeof(view)) && selected == root.selected && root.show_grid);
    assert(root.history->undo_count == history);
    app_editor_request_close(child, 0);
    assert(child->editor_confirm_close && !child->editor_close_requested);
    app_editor_confirm(child, 2);
    assert(!child->editor_confirm_close && !child->editor_close_requested);
    app_editor_request_close(child, 0);
    app_editor_confirm(child, 1);
    assert(app_editor_route(child) == &root && !root.editor_child);
    assert(!memcmp(&before, &root.bench, sizeof(before)) && root.dc_job.thread);
    app_simulation_update(&root);
    assert(root.sim.dc_current == .001 && !root.dc_job.thread);

    child = app_editor_open(&root);
    assert(child && app_editor_set_name(child, "Indicador \xc3\xa1rvore"));
    assert(bench_set_value(&child->bench, 0, 2200));
    for (int i = 1; i <= BLOCK_LIBRARY_SLOTS; ++i)
    {
        char path[128];
        snprintf(path, sizeof(path), "build/test-blocks/block-%d.elec", i);
        remove(path);
    }
    assert(app_editor_save(child) && !app_editor_dirty(child));
    assert(!memcmp(&before, &root.bench, sizeof(before)));
    assert(app_editor_apply(child));
    assert(app_editor_route(child) == &root);
    assert(!strcmp(root.bench.groups[0].name, "Indicador \xc3\xa1rvore"));
    assert(root.bench.parts[1].value == 2200 && root.history->undo_count == history + 1);
    assert(bench_history_undo(root.history, &root.bench));
    assert(root.bench.parts[1].value == 1000);
    assert(!strcmp(root.bench.groups[0].name, before.groups[0].name));
    remove("build/test-blocks/block-1.elec");
    app_state_dispose(&root);
    assert(!root.history && root.bench.part_count == 0 && !root.dc_job.thread);
    app_state_dispose(&root);
    return 0;
}
