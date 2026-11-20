#ifndef SIM_STATE_H
#define SIM_STATE_H
#include <stdint.h>
#include "workbench.h"
#include "spice_process.h"
#include "component_devices.h"
#include "sim_auto.h"

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
    double live_rate; /* Simulated seconds per wall second, estimated from accepted samples. */
    uint64_t live_sample_at;
    int dc_wire_valid;
    SimResultKind result_kind;
    uint64_t result_revision;
    double dc_current;
    char dc_status[160];
    double dc_led_currents[WORKBENCH_MAX_COMPONENTS];
    double dc_wire_currents[WORKBENCH_MAX_WIRES];
    double dc_elements[WORKBENCH_MAX_COMPONENTS][COMPONENT_MAX_ELEMENTS];
    /* Instrument and motor history derived from accepted live samples (simulated time). */
    float scope_history[WORKBENCH_MAX_COMPONENTS][COMPONENT_SCOPE_HISTORY];
    int scope_count[WORKBENCH_MAX_COMPONENTS];
    double motor_angle[WORKBENCH_MAX_COMPONENTS];
    double element_time;
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
    unsigned char alert[WORKBENCH_MAX_COMPONENTS], alert_pin[WORKBENCH_MAX_COMPONENTS];
    double alert_value[WORKBENCH_MAX_COMPONENTS];
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
/* Status line for the trace sample at trace_index, written to trace_status. */
const char *sim_state_trace_text(SimState *sim);
void sim_state_leds(const SimState *sim, const Workbench *bench, int leds[]);
/* Stores device measurements of an accepted sample and advances scope history and motor angles. */
void sim_state_accept_elements(SimState *sim, const Workbench *bench,
                               const double elements[WORKBENCH_MAX_COMPONENTS][COMPONENT_MAX_ELEMENTS], double time);

/* Live graph: samples of one point gathered while the continuous session keeps running. */
#define SIM_LIVE_GRAPH_CAPACITY 4096
typedef struct
{
    int active;
    SimLiveProbe probe;
    int valid; /* 0 when the last batch had no solver vector for the point */
    int part_count, wire_count; /* assembly size when opened; a change closes the graph */
    int start, count;
    double time[SIM_LIVE_GRAPH_CAPACITY], value[SIM_LIVE_GRAPH_CAPACITY];
} SimLiveGraph;

void sim_live_graph_open(SimLiveGraph *graph, SimLiveProbe probe, const Workbench *bench);
void sim_live_graph_close(SimLiveGraph *graph);
/* Appends a batch; samples older than the stored ones (a restarted session) clear the graph first. */
void sim_live_graph_append(SimLiveGraph *graph, const SimLiveSamples *samples);
/* index 0 is the oldest stored sample. */
void sim_live_graph_sample(const SimLiveGraph *graph, int index, double *time, double *value);
/* The assembly changed size, so the probed index may now name another part or wire. */
int sim_live_graph_stale(const SimLiveGraph *graph, const Workbench *bench);
#endif
