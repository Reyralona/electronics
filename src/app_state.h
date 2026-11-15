#ifndef APP_STATE_H
#define APP_STATE_H
#include "sim_state.h"
#include "spice_job.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "workbench_selection.h"

#define PROJECT_SLOT_COUNT 9
#define BLOCK_LIBRARY_SLOTS 8
#define APP_EDIT_SCALE (WORKBENCH_MAX_COMPONENTS + 4)
#define APP_EDIT_GRID (WORKBENCH_MAX_COMPONENTS + 3)
#define APP_EDIT_GROUP_NAME (WORKBENCH_MAX_COMPONENTS + 2)

/* Owned by the application thread. The worker receives its own snapshot. */
typedef struct AppState
{
    Workbench bench;
    Pin pending, hovered_pin;
    uint64_t hover_revision;
    int selected, placing, dragging, running;
    WirePoint route[WIRE_MAX_KNEES];
    int route_count, drag_wire, drag_knee;
    int settings, show_grid, show_handles, round_wires, show_current;
    int wire_color, selected_wire, palette_page, example_index;
    int leds[WORKBENCH_MAX_COMPONENTS];
    float offset_x, offset_y;
    SpiceJob dc_job;
    int probe_pin, show_trace, probe_current;
    double transient_step, transient_stop;
    double grid_spacing;
    int snap_grid, snap_board;
    BenchHistory *history;
    int move_recorded;
    BenchView view;
    int panning, editing;
    char edit_text[BENCH_GROUP_NAME_CAPACITY];
    int name_target_group, edit_select_all;
    int selftest, selftest_phase, exit_code;
    const char *status;
    int capture, auto_capture;
    char ngspice[1024];
    SimState sim;
    BenchSelection selection;
    BenchClipboard clipboard;
    int selecting_area, projects_open, save_slot;
    int saved_slots[PROJECT_SLOT_COUNT];
    WirePoint area_start, area_end, mouse_world, mouse_screen;
    int mouse_position_valid;
    char selection_status[160];
    BenchClipboard library_template;
    int packed_placing, library_index;
    int library_available[BLOCK_LIBRARY_SLOTS];
    char library_names[BLOCK_LIBRARY_SLOTS][BENCH_GROUP_NAME_CAPACITY];
    struct AppState *editor_parent, *editor_child;
    int editor_open_requested, editor_close_requested, editor_quit_requested, editor_confirm_close;
    int editor_group, editor_library_slot;
    uint64_t editor_parent_revision, editor_initial_revision, editor_saved_revision, editor_opened_at;
    char editor_name[BENCH_GROUP_NAME_CAPACITY];
    char editor_initial_name[BENCH_GROUP_NAME_CAPACITY], editor_saved_name[BENCH_GROUP_NAME_CAPACITY];
} AppState;

void app_state_init(AppState *app);
#endif
