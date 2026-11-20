#include "bench_scope.h"
#include "app_commands.h"
#include "ui_text.h"
#include "app_editor.h"
#include "app_editing.h"
#include "app_code_editor.h"
#include "app_packing.h"
#include "app_palette.h"
#include "component_registry.h"
#include "workbench_examples.h"
#include "chip_docs.h"
#include "workbench_file.h"
#include <stdio.h>
#include <string.h>

void app_commands_load_example(AppState *app, int choice)
{
    Workbench example BENCH_AUTO_CLEAR = {0};
    int example_selected;
    app->examples_open = 0;
    if (workbench_example_create(choice, &example, &example_selected))
    {
        if (!app_code_close(app, NULL))
            return;
        if (!app_history_record(app, &app->bench))
            return;
        bench_replace(&app->bench, &example);
        mcu_runtime_reset(app->mcu);
        spice_job_request_reset(&app->dc_job);
        app->packed_placing = 0;
        bench_selection_clear(&app->selection);
        app->example_index = choice;
        app->selected = example_selected;
        app->selected_wire = -1;
        app->dragging = -1;
        app->dragging_environment = -1;
        app->drag_wire = -1;
        app->drag_lead.component = -1;
        app->pending.component = -1;
        app->route_count = 0;
        app->placing = -1;
        app->settings = 0;
        app->view = (BenchView){1, 0, 0};
        sim_state_clear_result(&app->sim);
        memset(app->leds, 0, sizeof(app->leds));
        app->probe_pin = choice == 1 || choice == 2 ? 2 : 0;
        app->probe_current = 0;
        app->transient_step = 1e-5;
        app->transient_stop = .005;
        app->status = workbench_example_description(choice);
    }
    else
        app->status = tr(TXT_APP_EXAMPLE_FAILED);
    return;
}

static void command_example(AppState *app)
{
    app_commands_load_example(app, (app->example_index + 1) % WORKBENCH_EXAMPLE_COUNT);
}

static void command_palette(AppState *app)
{
    app->palette_page = (app->palette_page + 1) % PALETTE_PAGES;
    app->palette_offset = 0;
    app->placing = -1;
    app->packed_placing = 0;
    app_packing_refresh(app);
    app->status = palette_page_name(app->palette_page);
    return;
}

static void command_settings(AppState *app)
{
    app_state_toggle_panel(app, APP_PANEL_SETTINGS);
    app->dragging = -1;
    app->drag_wire = -1;
    app->drag_lead.component = -1;
    return;
}

static void command_home(AppState *app)
{
    app->view = (BenchView){1, 0, 0};
    app->status = tr(TXT_APP_VIEW_RESET);
    return;
}

static void command_history(AppState *app, SDL_Keycode key)
{
    if (!app_code_close(app, NULL))
        return;
    int changed =
        key == SDLK_Z ? bench_history_undo(app->history, &app->bench) : bench_history_redo(app->history, &app->bench);
    if (changed)
    {
        mcu_runtime_reset(app->mcu);
        app->packed_placing = 0;
        bench_selection_clear(&app->selection);
        app->selected = -1;
        app->selected_wire = -1;
        app->dragging = -1;
        app->drag_wire = -1;
        app->drag_lead.component = -1;
        app->pending.component = -1;
        app->route_count = 0;
        memset(app->leds, 0, sizeof(app->leds));
        app->status = tr(key == SDLK_Z ? TXT_APP_UNDONE : TXT_APP_REDONE);
    }
    else
        app->status = tr(key == SDLK_Z ? TXT_APP_NOTHING_TO_UNDO : TXT_APP_NOTHING_TO_REDO);
    return;
}

static void command_cancel(AppState *app)
{
    app->dragging_environment = -1;
    app->selecting_area = 0;
    app->packed_placing = 0;
    bench_selection_clear(&app->selection);
    app->pending.component = -1;
    app->route_count = 0;
    app->placing = -1;
    app->dragging = -1;
    app->drag_wire = -1;
    app->drag_lead.component = -1;
    app->selected = -1;
    app->selected_wire = -1;
    app->status = tr(TXT_APP_SELECTION_CANCELED);
}

static void command_playback(AppState *app)
{
    app->sim.sim_running = !app->sim.sim_running;
    if (app->sim.sim_running)
        app->live_last_scheduled = SDL_GetTicks();
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.sim_running)
    {
        if (app->sim.trace_index >= app->sim.trace.count - 1)
            app->sim.trace_index = 0;
        app->sim.trace_play_start_index = app->sim.trace_index;
        app->sim.trace_play_started = SDL_GetTicks();
        app->status = tr(TXT_APP_PLAYING_CURVE);
        if (app->sim.trace_automatic)
            app->status = tr(TXT_APP_SIM_RUNNING);
    }
    else if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
        app->status = tr(TXT_APP_PLAYBACK_PAUSED);
    else
        app->status = tr(app->sim.sim_running ? TXT_APP_SIM_RUNNING : TXT_APP_SIM_PAUSED);
}

static void command_recalculate(AppState *app)
{
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic)
    {
        sim_state_clear_result(&app->sim);
        app->show_trace = 0;
    }
    app->sim.auto_requested_valid = 0;
    app->sim.bench_changed_at = 0;
    app->sim.sim_running = 1;
    app->status = tr(TXT_APP_RECALCULATING);
}

void app_commands_close_graph(AppState *app)
{
    if (app->live_graph.active)
    {
        sim_live_graph_close(&app->live_graph);
        app->status = tr(TXT_APP_GRAPH_CLOSED);
        return;
    }
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic)
    {
        command_recalculate(app);
        app->status = tr(TXT_APP_GRAPH_CLOSED_RESUMED);
        return;
    }
    app->show_trace = 0;
    app->status = tr(TXT_APP_GRAPH_CLOSED);
}

static void command_rotate(AppState *app)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (bench_rotate_part(&app->bench, app->selected))
    {
        if (!app->bench.parts[app->selected].group)
            bench_snap_to_breadboard(&app->bench, app->selected);
        if (!app_history_record(app, &before))
            return;
        app->dragging = -1;
        app->pending.component = -1;
        app->route_count = 0;
        app->status = tr(TXT_APP_PART_ROTATED);
    }
    else
        app->status = tr(TXT_APP_BOARD_NO_ROTATE);
}

static void command_mirror(AppState *app)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (!bench_mirror_part(&app->bench, app->selected))
    {
        app->status = tr(TXT_APP_BOARD_NO_MIRROR);
        return;
    }
    if (!app->bench.parts[app->selected].group)
        bench_snap_to_breadboard(&app->bench, app->selected);
    if (!app_history_record(app, &before))
        return;
    app->dragging = -1;
    app->pending.component = -1;
    app->route_count = 0;
    app->status = tr(TXT_APP_PART_MIRRORED);
}

static void command_graph(AppState *app)
{
    app->show_trace = !app->show_trace;
    app->status = app->show_trace ? tr(TXT_APP_GRAPH_SHOWN) : tr(TXT_APP_GRAPH_HIDDEN);
}

/* T keeps the running simulation and graphs it live; Shift+T asks for the full curve from t = 0. */
static int wants_live_graph(const AppState *app, int full_graph)
{
    return !full_graph && app->sim.auto_enabled && sim_auto_uses_live_session(&app->bench);
}

static void open_live_graph(AppState *app, SimLiveProbe probe)
{
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace_automatic)
    {
        sim_state_clear_result(&app->sim);
        app->sim.auto_requested_valid = 0;
        app->sim.sim_running = 1;
    }
    app->trace_pending_kind = 0;
    sim_live_graph_open(&app->live_graph, probe, &app->bench);
    app->show_trace = 1;
    app->status = tr(TXT_APP_LIVE_GRAPH_OPENED);
}

static void command_trace_wire(AppState *app, int full_graph)
{
    if (wants_live_graph(app, full_graph))
    {
        open_live_graph(app, (SimLiveProbe){SIM_LIVE_PROBE_WIRE_CURRENT, {app->selected_wire, 0}, 0});
        return;
    }
    sim_live_graph_close(&app->live_graph);
    if (app->dc_job.thread)
    {
        if (app->dc_job.automatic && app->dc_job.continuous)
        {
            app->sim.sim_running = 0;
            app->trace_pending_kind = 2;
            app->trace_pending_index = app->selected_wire;
            app->trace_pending_revision = app->bench.revision;
            app->status = tr(TXT_APP_CALCULATING_WIRE_CURRENT);
        }
        else
            app->status = tr(TXT_APP_CALCULATION_BUSY);
    }
    else if (spice_job_start_transient_wire(&app->dc_job, &app->bench, app->ngspice, app->selected_wire,
                                            app->transient_step, app->transient_stop))
    {
        sim_state_clear_result(&app->sim);
        app->status = tr(TXT_APP_CALCULATING_WIRE_CURRENT);
    }
    else
        app->status = tr(TXT_APP_CALCULATION_START_FAILED);
}

static int start_part_trace(AppState *app)
{
    if (app->show_current)
        return spice_job_start_transient_instrumented(
            &app->dc_job, &app->bench, app->ngspice, app->probe_current ? 2 : 1,
            (Pin){app->selected, app->probe_current ? 0 : app->probe_pin}, app->transient_step, app->transient_stop);
    if (app->probe_current)
        return spice_job_start_transient_current(&app->dc_job, &app->bench, app->ngspice, app->selected,
                                                 app->transient_step, app->transient_stop);
    return spice_job_start_transient(&app->dc_job, &app->bench, app->ngspice, (Pin){app->selected, app->probe_pin},
                                     app->transient_step, app->transient_stop);
}

/* Reason a part cannot be graphed with the current probe choice; 0 when it can. */
static int part_trace_problem(const AppState *app, TextId *problem)
{
    PartType type = app->bench.parts[app->selected].type;
    if (!app->probe_current && app->probe_pin >= bench_pin_count(type))
        *problem = TXT_APP_NO_SUCH_TERMINAL;
    else if (app->probe_current && (is_gate(type) || component_has_flag(type, COMPONENT_DIP)))
        *problem = TXT_APP_GATE_CURRENT_UNAVAILABLE;
    else if (app->probe_current && type == PART_SWITCH)
        *problem = TXT_APP_SWITCH_CURRENT_UNAVAILABLE;
    else
        return 0;
    return 1;
}

static SimLiveProbe part_live_probe(const AppState *app)
{
    if (app->probe_current)
        return (SimLiveProbe){SIM_LIVE_PROBE_PART_CURRENT, {app->selected, 0}, 0};
    return (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {app->selected, app->probe_pin}, 0};
}

static void command_trace_part(AppState *app, int full_graph)
{
    TextId problem;
    if (part_trace_problem(app, &problem))
    {
        app->status = tr(problem);
        return;
    }
    if (wants_live_graph(app, full_graph))
    {
        open_live_graph(app, part_live_probe(app));
        return;
    }
    sim_live_graph_close(&app->live_graph);
    if (app->dc_job.thread)
    {
        if (app->dc_job.automatic && app->dc_job.continuous)
        {
            app->sim.sim_running = 0;
            app->trace_pending_kind = 1;
            app->trace_pending_index = app->selected;
            app->trace_pending_pin = app->probe_pin;
            app->trace_pending_current = app->probe_current;
            app->trace_pending_revision = app->bench.revision;
            app->status = tr(TXT_APP_CALCULATING_CURVE);
        }
        else
            app->status = tr(TXT_APP_CALCULATION_BUSY);
    }
    else if (start_part_trace(app))
    {
        sim_state_clear_result(&app->sim);
        app->status = tr(TXT_APP_CALCULATING_CURVE);
    }
    else
        app->status = tr(TXT_APP_CALCULATION_START_FAILED);
}

static void command_trace_step(AppState *app, SDL_Keycode key)
{
    if (key == SDLK_N && app->sim.trace_index + 1 < app->sim.trace.count)
        ++app->sim.trace_index;
    if (key == SDLK_B && app->sim.trace_index > 0)
        --app->sim.trace_index;
    app->status = sim_state_trace_text(&app->sim);
}

static void command_logic_step(AppState *app)
{
    if (bench_boolean_supported(&app->bench))
    {
        bench_simulate(&app->bench, app->leds);
        app->status = tr(TXT_APP_STEP_DONE);
    }
    else
        app->status = tr(TXT_APP_STEP_UNAVAILABLE);
}

static void command_dc(AppState *app)
{
    if (app->dc_job.thread)
        app->status = tr(TXT_APP_CALCULATION_BUSY);
    else if ((app->show_current ? spice_job_start_wires : spice_job_start)(&app->dc_job, &app->bench, app->ngspice))
    {
        sim_state_clear_result(&app->sim);
        app->status = tr(TXT_APP_CALCULATING_DC);
    }
    else
        app->status = tr(TXT_APP_CALCULATION_START_FAILED);
}

static void command_edit_value(AppState *app, SDL_Window *window)
{
    PartType type = app->bench.parts[app->selected].type;
    const ComponentInfo *info = component_info(type);
    if (info && info->primary && app_selection_value_target(app) >= 0)
    {
        if (SDL_StartTextInput(window))
        {
            app->editing = app->selected;
            app->edit_error = 0;
            app->edit_select_all = 1;
            snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", app->bench.parts[app->selected].value);
            app->status = tr(TXT_APP_TYPE_VALUE);
        }
        else
            app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
    }
    else
        app->status = tr(TXT_APP_NO_VALUE);
}

static void command_scale_value(AppState *app, SDL_Keycode key)
{
    double scale = key == SDLK_MINUS ? .5 : 2.0;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (app_selection_set_value(app, app->selected, app->bench.parts[app->selected].value * scale))
    {
        if (!app_history_record(app, &before))
            return;
        app->status = tr(TXT_APP_VALUE_CHANGED);
    }
    else
        app->status = tr(TXT_APP_VALUE_OUT_OF_RANGE);
}

static void command_delete_part(AppState *app)
{
    if (!app_history_record(app, &app->bench))
        return;
    bench_remove(&app->bench, app->selected);
    app->packed_placing = 0;
    bench_selection_clear(&app->selection);
    app->selected = -1;
    app->selected_wire = -1;
    app->pending.component = -1;
    memset(app->leds, 0, sizeof(app->leds));
    app->status = tr(TXT_APP_PART_DELETED);
}

static void command_delete_wire(AppState *app)
{
    if (!app_history_record(app, &app->bench))
        return;
    bench_remove_wire(&app->bench, app->selected_wire);
    bench_selection_clear(&app->selection);
    if (bench_prune_junctions(&app->bench))
        app->selected = -1;
    app->selected_wire = -1;
    app->status = tr(TXT_APP_WIRE_DELETED);
}

static void command_selection(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    if (app->bench.parts[app->selected].group && key != SDLK_DELETE && key != SDLK_T && key != SDLK_R &&
        key != SDLK_M)
    {
        app->status = tr(TXT_APP_CHIP_ACTION_UNAVAILABLE);
        return;
    }
    switch (key)
    {
    case SDLK_R:
        command_rotate(app);
        break;
    case SDLK_M:
        command_mirror(app);
        break;
    case SDLK_I:
        if (chip_doc(app->bench.parts[app->selected].type))
            app->doc_type = app->bench.parts[app->selected].type;
        else
            app->status = tr(TXT_DOC_NONE);
        break;
    case SDLK_T:
        if (!bench_is_board(app->bench.parts[app->selected].type))
            command_trace_part(app, (mod & SDL_KMOD_SHIFT) != 0);
        break;
    case SDLK_E:
        command_edit_value(app, window);
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_MINUS:
        command_scale_value(app, key);
        break;
    case SDLK_DELETE:
        command_delete_part(app);
        break;
    }
}

void app_commands_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    if (app->project_overwrite >= 0)
    {
        if (key == SDLK_ESCAPE)
            app_projects_confirm(app, 0);
        return;
    }
    if (app->settings)
    {
        if (key == SDLK_ESCAPE || key == SDLK_F1)
            app->settings = 0;
        else if (key == SDLK_F2)
            app_state_toggle_panel(app, APP_PANEL_EXAMPLES);
        else if (key == SDLK_F3)
            app_editing_key(app, key, mod);
        else if ((mod & SDL_KMOD_CTRL) && (key == SDLK_S || key == SDLK_L))
            app_editing_key(app, key, mod);
        return;
    }
    if (app->projects_open)
    {
        if (key == SDLK_F1)
            command_settings(app);
        else if (key == SDLK_F2)
            app_state_toggle_panel(app, APP_PANEL_EXAMPLES);
        else
            app_editing_key(app, key, mod);
        return;
    }
    if (app->doc_type >= 0)
    {
        if (key == SDLK_ESCAPE || key == SDLK_I)
            app->doc_type = -1;
        return;
    }
    if (app->examples_open)
    {
        if (key == SDLK_ESCAPE || key == SDLK_F2)
            app->examples_open = 0;
        else if (key == SDLK_F1)
            command_settings(app);
        else if (key == SDLK_F3)
            app_editing_key(app, key, mod);
        return;
    }
    if (app_editor_key(app, window, key, mod))
        return;
    if (!app->projects_open && app_packing_key(app, key, mod))
        return;
    if (app_editing_key(app, key, mod))
        return;
    if ((mod & SDL_KMOD_CTRL) && (key == SDLK_Z || key == SDLK_Y))
    {
        command_history(app, key);
        return;
    }
    switch (key)
    {
    case SDLK_F2:
        if (mod & SDL_KMOD_CTRL)
            command_example(app);
        else
            app_state_toggle_panel(app, APP_PANEL_EXAMPLES);
        return;
    case SDLK_TAB:
        command_palette(app);
        return;
    case SDLK_F1:
        command_settings(app);
        return;
    case SDLK_HOME:
        command_home(app);
        return;
    case SDLK_X:
        app->xray = !app->xray;
        app->status = app->xray ? tr(TXT_APP_TRANSPARENCY_ON)
                                : tr(TXT_APP_TRANSPARENCY_OFF);
        return;
    case SDLK_ESCAPE:
        command_cancel(app);
        return;
    case SDLK_SPACE:
        if (mod & SDL_KMOD_CTRL)
        {
            mcu_runtime_reset(app->mcu);
            spice_job_request_reset(&app->dc_job);
            sim_state_clear_result(&app->sim);
            app->sim.auto_requested_valid = 0;
            app->sim.sim_running = 1;
            app->live_last_scheduled = SDL_GetTicks();
            app->show_trace = 0;
            app->status = tr(TXT_APP_RESETTING);
        }
        else
            command_playback(app);
        return;
    case SDLK_P:
        if (app->sim.auto_enabled)
            command_recalculate(app);
        else
            command_dc(app);
        return;
    case SDLK_G:
        if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
            command_graph(app);
        return;
    case SDLK_N:
    case SDLK_B:
        if (!app->sim.sim_running && sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
            command_trace_step(app, key);
        else if (key == SDLK_N && !app->sim.sim_running)
            command_logic_step(app);
        return;
    }
    if (key == SDLK_T && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
        command_trace_wire(app, (mod & SDL_KMOD_SHIFT) != 0);
    else if (app->selected >= 0)
        command_selection(app, window, key, mod);
    else if (key == SDLK_DELETE && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
        command_delete_wire(app);
}
