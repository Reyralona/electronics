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

static void test_nested_editor(void)
{
    static AppState root;
    app_state_init(&root);
    root.selftest = 1;
    root.sim.sim_running = 0;
    assert(bench_add(&root.bench, PART_RESISTOR, 400, 200) == 0);
    assert(bench_add(&root.bench, PART_SOURCE, 100, 200) == 1);
    assert(bench_toggle_wire(&root.bench, (Pin){1, 0}, (Pin){0, 0}) == 1);
    assert(bench_toggle_wire(&root.bench, (Pin){0, 1}, (Pin){1, 1}) == 1);
    root.selection.parts[0] = 1;
    int inner = bench_pack_selection(&root.bench, &root.selection, root.selection_status, sizeof(root.selection_status));
    int outer = bench_pack_selection(&root.bench, &root.selection, root.selection_status, sizeof(root.selection_status));
    assert(inner && outer && inner != outer);
    root.selected = 0;
    Workbench before = {0};
    assert(bench_snapshot_clone(&before, &root.bench));
    AppState *child = app_editor_open(&root);
    assert(child && child->bench.parts[0].group == inner);
    child->selected = 0;
    assert(app_editor_key(child, NULL, SDLK_RETURN, 0));
    AppState *grandchild = app_editor_route(child);
    assert(grandchild != child && grandchild->editor_parent == child);
    assert(bench_set_value(&grandchild->bench, 0, 2200));
    app_editor_request_close(grandchild, 0);
    assert(grandchild->editor_confirm_close);
    app_editor_confirm(grandchild, 1);
    assert(app_editor_route(grandchild) == child);
    assert(child->bench.parts[0].value == 1000 && root.bench.parts[0].value == 1000);
    grandchild = app_editor_open(child);
    assert(grandchild && bench_set_value(&grandchild->bench, 0, 3300));
    assert(app_editor_apply(grandchild));
    assert(app_editor_route(grandchild) == child);
    assert(child->bench.parts[0].value == 3300 && root.bench.parts[0].value == 1000);
    assert(app_editor_apply(child));
    assert(app_editor_route(child) == &root);
    int resistor = bench_group_first(&root.bench, outer);
    assert(resistor >= 0 && root.bench.parts[resistor].value == 3300 && root.bench.wire_count == 2);
    assert(bench_history_undo(root.history, &root.bench));
    assert(root.bench.parts[0].value == before.parts[0].value && root.bench.parts[0].group == outer);
    bench_snapshot_clear(&before);
    app_state_dispose(&root);
}

static void apply_keeps_history_and_closes_old_probe(void)
{
    static AppState root;
    app_state_init(&root);
    root.selftest = 1;
    bench_add(&root.bench, PART_RESISTOR, 100, 100);
    bench_add(&root.bench, PART_SOURCE, 300, 100);
    root.selection.parts[0] = 1;
    assert(bench_pack_selection(&root.bench, &root.selection, root.selection_status, sizeof(root.selection_status)));
    root.selected = 0;
    sim_live_graph_open(&root.live_graph, (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {1, 0}, .001}, &root.bench);
    AppState *child = app_editor_open(&root);
    assert(child);
    assert(app_editor_apply(child));
    assert(root.live_graph.active); /* Inspecting without edits keeps the probe. */
    assert(app_editor_route(child) == &root);
    child = app_editor_open(&root);
    assert(bench_set_value(&child->bench, 0, 200));
    BenchHistory *history = root.history;
    root.history = NULL; /* Inject an unavailable history store. */
    uint64_t revision = root.bench.revision;
    assert(!app_editor_apply(child));
    assert(!child->editor_close_requested && root.live_graph.active);
    assert(root.bench.revision == revision && root.bench.parts[0].value == 1000);
    root.history = history;
    assert(app_editor_apply(child));
    assert(!root.live_graph.active && root.bench.parts[0].type == PART_SOURCE);
    assert(app_editor_route(child) == &root);
    app_state_dispose(&root);
}

int main(void)
{
    apply_keeps_history_and_closes_old_probe();
    test_nested_editor();
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
    assert(!strcmp(root.bench.groups[0].name, child->editor_name));
    before = root.bench;
    history = root.history->undo_count;
    assert(bench_set_value(&child->bench, 0, 2200));
    for (int i = 1; i <= BLOCK_LIBRARY_SLOTS; ++i)
    {
        char path[128];
        snprintf(path, sizeof(path), "build/test-blocks/block-%d.elec", i);
        remove(path);
    }
    assert(app_editor_save(child) && !app_editor_dirty(child));
    /* Saving links the outer instance to its library entry; the circuit itself is untouched. */
    assert(root.bench.groups[0].model && root.bench.groups[0].model == child->editor_model);
    before.groups[0].model = root.bench.groups[0].model;
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
