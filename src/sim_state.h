#ifndef SIM_STATE_H
#define SIM_STATE_H
#include <stdint.h>
#include "workbench.h"
#include "spice_process.h"

typedef enum
{
    SIM_RESULT_NONE,
    SIM_RESULT_DC,
    SIM_RESULT_TRACE
} SimResultKind;

/* Results and playback state. No renderer or window dependency. */
typedef struct
{
    int sim_running;
    int live_mode;
    int display_pending; /* Last compatible sample remains visible while controls update. */
    double live_time;
    int dc_wire_valid;
    SimResultKind result_kind;
    uint64_t result_revision;
    double dc_current;
    char dc_status[160];
    double dc_led_currents[WORKBENCH_MAX_COMPONENTS];
    double dc_wire_currents[WORKBENCH_MAX_WIRES];
    SpiceTrace trace;
    int trace_index, trace_pin, trace_current, trace_wire;
    char trace_status[128];
    uint64_t trace_play_started;
    int trace_play_start_index;
    int auto_enabled, auto_failed, auto_requested_valid, auto_show_current;
    double auto_step, auto_stop;
    uint64_t requested_revision, frame_revision, burn_revision;
    uint64_t bench_changed_at;
    unsigned char burnt[WORKBENCH_MAX_COMPONENTS], burn_reason[WORKBENCH_MAX_COMPONENTS];
    double burn_value[WORKBENCH_MAX_COMPONENTS];
    int burnt_count, trace_periodic, trace_automatic;
    char auto_status[200];
} SimState;
/* Exactly one result kind is valid. A failed result or an edit can clear it. */
int sim_state_has_result(const SimState *sim, SimResultKind kind);
void sim_state_set_result(SimState *sim, SimResultKind kind, int success, uint64_t revision);
void sim_state_clear_result(SimState *sim);
void sim_state_discard_result(SimState *sim, SimResultKind kind);
Pin sim_state_probe(const Workbench *bench, int selected, int probe_pin);
/* Transitions return a status only when the presentation needs to change.
 * The caller supplies monotonic time; no window, renderer or clock API is needed. */
const char *sim_state_invalidate(SimState *sim, const Workbench *bench);
const char *sim_state_advance(SimState *sim, uint64_t now);
void sim_state_leds(const SimState *sim, const Workbench *bench, int leds[]);
#endif
