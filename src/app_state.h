#ifndef APP_STATE_H
#define APP_STATE_H
#include "sim_state.h"
#include "spice_job.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "workbench_selection.h"
#include "workbench_file.h"
#include "mcu_runtime.h"
#define MCU_CODE_CAPACITY 16384

#define PROJECT_SLOTS_PER_PAGE 6
#define BLOCK_LIBRARY_SLOTS 8
#define APP_EDIT_STEP WORKBENCH_MAX_COMPONENTS
#define APP_EDIT_STOP (WORKBENCH_MAX_COMPONENTS + 1)
#define APP_FONT_MIN 12
#define APP_FONT_MAX 20
#define APP_EDIT_SCALE (WORKBENCH_MAX_COMPONENTS + 4)
#define APP_EDIT_NOTE (WORKBENCH_MAX_COMPONENTS + 6)
#define APP_EDIT_PORT_NAME (WORKBENCH_MAX_COMPONENTS + 5) /* name of chip terminal name_target_pin */
#define APP_EDIT_GRID (WORKBENCH_MAX_COMPONENTS + 3)
#define APP_EDIT_GROUP_NAME (WORKBENCH_MAX_COMPONENTS + 2)

typedef enum
{
    APP_UI_BENCH,
    APP_UI_REPLACE,
    APP_UI_EDITOR_CONFIRM,
    APP_UI_VALUE,
    APP_UI_SETTINGS,
    APP_UI_PROJECTS,
    APP_UI_EXAMPLES,
    APP_UI_CODE
} AppControlContext;

/* Owned by the application thread. The worker receives its own snapshot. */
typedef struct AppState
{
    Workbench bench;
    Pin pending, hovered_pin, drag_lead;
    Pin drag_port; /* Chip terminal being rearranged with Alt+drag; component -1 = none. */
    Pin name_target_pin; /* Chip terminal being named (APP_EDIT_PORT_NAME). */
    size_t drag_lead_point;
    uint64_t hover_revision;
    int selected, placing, dragging, running;
    WirePoint route[WIRE_MAX_KNEES];
    int route_count, drag_wire, drag_knee, dragging_selection_knee;
    int settings, show_grid, show_handles, round_wires, show_current;
    int wire_color, selected_wire, palette_page, example_index;
    int palette_offset, held_button, dragging_environment;
    char palette_search[96];
    int palette_search_active;
    int palette_search_select_all;
    int keyboard_focus;
    AppControlContext keyboard_context;
    WirePoint keyboard_target;
    int leds[WORKBENCH_MAX_COMPONENTS];
    float offset_x, offset_y;
    SpiceJob dc_job;
    int probe_pin, show_trace, probe_current;
    int trace_pending_kind, trace_pending_index, trace_pending_pin, trace_pending_current;
    uint64_t trace_pending_revision;
    double transient_step, transient_stop;
    double simulation_speed;
    double grid_spacing;
    int snap_grid, snap_board;
    int dark_theme, gui_font_size, show_pin_labels;
    int xray; /* X: transparent part bodies; covered terminals become reachable. */
    int inspector_minimized; /* Inspector shown as a small name tab; kept across selections. */
    BenchHistory *history;
    int move_recorded;
    int drag_snap_mode; /* 0 automatic, 1 grid only, 2 free. */
    uint64_t live_last_scheduled;
    BenchView view;
    int panning, editing;
    char edit_text[BENCH_GROUP_NAME_CAPACITY];
    int name_target_group, edit_select_all;
    int edit_error;
    int selftest, selftest_phase, exit_code;
    const char *status;
    int capture, auto_capture;
    /* Root-context preferences are persisted only for interactive sessions (not tests/captures). */
    int preferences_enabled;
    WorkbenchSettings preferences_saved;
    char ngspice[1024];
    SimState sim;
    SimLiveGraph live_graph; /* T while the live session runs: graph fed by the running simulation. */
    BenchSelection selection;
    BenchClipboard clipboard;
    int selecting_area, projects_open, save_slot, examples_open;
    int *saved_slots;
    int project_overwrite; /* Slot presented by the confirmation; -1 when no confirmation is open. */
    int *project_parts, *project_wires;
    int project_slot_count, project_page;
    int project_thumbnail_pending;
    unsigned project_thumbnail_revision;
    int project_thumbnail_render;
    WirePoint area_start, area_end, mouse_world, mouse_screen;
    uint64_t area_scroll_ticks; /* Last edge auto-scroll step while selecting an area; 0 = idle. */
    int mouse_position_valid;
    char selection_status[160];
    BenchClipboard library_template;
    int packed_placing, library_index, library_delete_armed;
    int library_available[BLOCK_LIBRARY_SLOTS];
    char library_names[BLOCK_LIBRARY_SLOTS][BENCH_GROUP_NAME_CAPACITY];
    struct AppState *editor_parent, *editor_child;
    int editor_open_requested, editor_close_requested, editor_quit_requested, editor_confirm_close;
    uint64_t library_models[BLOCK_LIBRARY_SLOTS]; /* Model identity of each listed block (0 = legacy file). */
    int editor_group;
    uint64_t editor_model; /* Library model this editor saves into; 0 until known. */
    uint64_t editor_parent_revision, editor_initial_revision, editor_saved_revision, editor_opened_at;
    char editor_name[BENCH_GROUP_NAME_CAPACITY];
    char editor_initial_name[BENCH_GROUP_NAME_CAPACITY], editor_saved_name[BENCH_GROUP_NAME_CAPACITY];
    /* Microcontrollers: programs run on the wall clock; the code editor edits one board at a time. */
    McuRuntime *mcu;
    uint64_t mcu_last_ticks;
    int code_part, code_cursor, code_scroll, code_dirty;
    int completion_selected, completion_hidden;
    char code_text[MCU_CODE_CAPACITY];
} AppState;

void app_state_init(AppState *app);
/* End any editor child first; this releases this context only. */
void app_state_dispose(AppState *app);
/* Overlay panels (projects, examples, settings): at most one is open. */
typedef enum
{
    APP_PANEL_PROJECTS,
    APP_PANEL_EXAMPLES,
    APP_PANEL_SETTINGS
} AppPanel;
/* Opens panel and closes the others, or closes it if it was already open. Returns 1 if now open. */
int app_state_toggle_panel(AppState *app, AppPanel panel);
#endif
