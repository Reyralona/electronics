#include "app_state.h"
#include <stdlib.h>
#include <string.h>

void app_state_init(AppState *app)
{
    memset(app, 0, sizeof(*app));
    app->pending = (Pin){-1, -1};
    app->hovered_pin = (Pin){-1, -1};
    app->selected = -1;
    app->mouse_world = (WirePoint){500, 300};
    app->placing = -1;
    app->dragging = -1;
    app->running = 1;
    app->drag_wire = -1;
    app->drag_knee = -1;
    app->show_grid = 1;
    app->grid_spacing = 24;
    app->snap_board = 1;
    app->show_handles = 1;
    app->round_wires = 1;
    app->selected_wire = -1;
    app->example_index = -1;
    app->show_trace = 1;
    app->transient_step = 1e-5;
    app->transient_stop = .005;
    app->history = calloc(1, sizeof(*app->history));
    app->view = (BenchView){1, 0, 0};
    app->editing = -1;
    app->name_target_group = -1;
    app->editor_library_slot = -1;
    app->status = "Escolha um componente na biblioteca e clique na bancada.";
    app->sim.sim_running = 1;
    app->sim.trace_wire = -1;
}

void app_state_dispose(AppState *app)
{
    if (!app)
        return;
    spice_job_clear(&app->dc_job);
    bench_history_clear(app->history);
    free(app->history);
    app->history = NULL;
    bench_snapshot_clear(&app->bench);
    bench_snapshot_clear(&app->clipboard.bench);
    bench_snapshot_clear(&app->library_template.bench);
}
