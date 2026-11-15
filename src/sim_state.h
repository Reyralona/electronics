#ifndef SIM_STATE_H
#define SIM_STATE_H
#include <stdint.h>
#include "workbench.h"
#include "spice_process.h"

/* Results and playback state. No renderer or window dependency. */
typedef struct
{
    int sim_running;
    int dc_wire_valid;
    Workbench dc_snapshot;
    int dc_valid;
    double dc_current;
    char dc_status[160];
    double dc_led_currents[WORKBENCH_MAX_COMPONENTS];
    double dc_wire_currents[WORKBENCH_MAX_WIRES];
    SpiceTrace trace;
    Workbench trace_snapshot;
    int trace_valid, trace_index, trace_pin, trace_current, trace_wire;
    char trace_status[128];
    uint64_t trace_play_started;
    int trace_play_start_index;
    int auto_enabled, auto_failed, auto_requested_valid, auto_show_current;
    double auto_step, auto_stop;
    Workbench auto_requested, frame_bench, result_shape;
    uint64_t bench_changed_at;
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS], burn_reason[WORKBENCH_MAX_COMPONENTS];
    double burn_value[WORKBENCH_MAX_COMPONENTS];
    int burnt_count, trace_periodic, trace_automatic;
    char auto_status[200];
} SimState;
Pin sim_state_probe(const Workbench *bench, int selected, int probe_pin);
/* Transitions return a status only when the presentation needs to change.
 * The caller supplies monotonic time; no window, renderer or clock API is needed. */
const char *sim_state_invalidate(SimState *sim, const Workbench *bench);
const char *sim_state_advance(SimState *sim, uint64_t now);
void sim_state_leds(const SimState *sim, const Workbench *bench, int leds[]);
#endif
