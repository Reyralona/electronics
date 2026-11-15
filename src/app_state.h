#ifndef APP_STATE_H
#define APP_STATE_H
#include "sim_state.h"
#include "spice_job.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "workbench_selection.h"

#define PROJECT_SLOT_COUNT 9
#define BLOCK_LIBRARY_SLOTS 8

/* Owned by the application thread. The worker receives its own snapshot. */
typedef struct
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
    BenchHistory *history;
    int move_recorded;
    BenchView view;
    int panning, editing;
    char edit_text[64];
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
} AppState;

void app_state_init(AppState *app);
#endif
