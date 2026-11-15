#ifndef APP_STATE_H
#define APP_STATE_H
#include "sim_state.h"
#include "spice_job.h"
#include "workbench_history.h"
#include "workbench_view.h"

/* Owned by the application thread. The worker receives its own snapshot. */
typedef struct
{
    Workbench bench;
    Pin pending;
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
} AppState;

void app_state_init(AppState *app);
#endif
