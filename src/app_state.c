#include "app_state.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>

void app_state_init(AppState *app)
{
    memset(app, 0, sizeof(*app));
    app->pending = (Pin){-1, -1};
    app->hovered_pin = (Pin){-1, -1};
    app->selected = -1;
    app->held_button = -1;
    app->dragging_environment = -1;
    app->drag_lead.component = -1;
    app->drag_port.component = -1;
    app->name_target_pin = (Pin){-1, -1};
    app->mouse_world = (WirePoint){500, 300};
    app->placing = -1;
    app->dragging = -1;
    app->running = 1;
    app->drag_wire = -1;
    app->drag_knee = -1;
    app->show_grid = 1;
    app->gui_font_size = 15;
    app->grid_spacing = 24;
    app->snap_board = 1;
    app->show_handles = 1;
    app->round_wires = 1;
    app->selected_wire = -1;
    app->wire_color = BENCH_DEFAULT_WIRE_COLOR;
    app->example_index = -1;
    app->show_trace = 1;
    app->transient_step = 1e-5;
    app->transient_stop = .005;
    app->simulation_speed = 1.0;
    app->history = calloc(1, sizeof(*app->history));
    app->view = (BenchView){1, 0, 0};
    app->editing = -1;
    app->project_overwrite = -1;
    app->keyboard_focus = -1;
    app->name_target_group = -1;
    app->library_delete_armed = -1;
    app->code_part = -1;
    app->mcu = mcu_runtime_create();
    app->status = tr(TXT_APP_START);
    app->sim.sim_running = 1;
    app->sim.trace_wire = -1;
}

void app_state_dispose(AppState *app)
{
    if (!app)
        return;
    mcu_runtime_destroy(app->mcu);
    app->mcu = NULL;
    spice_job_clear(&app->dc_job);
    bench_history_clear(app->history);
    free(app->history);
    app->history = NULL;
    bench_snapshot_clear(&app->bench);
    bench_snapshot_clear(&app->clipboard.bench);
    bench_snapshot_clear(&app->library_template.bench);
}

static int *panel_flag(AppState *app, AppPanel panel)
{
    if (panel == APP_PANEL_PROJECTS)
        return &app->projects_open;
    if (panel == APP_PANEL_EXAMPLES)
        return &app->examples_open;
    return &app->settings;
}

int app_state_toggle_panel(AppState *app, AppPanel panel)
{
    int open = !*panel_flag(app, panel);
    app->projects_open = 0;
    app->examples_open = 0;
    app->settings = 0;
    *panel_flag(app, panel) = open;
    return open;
}
