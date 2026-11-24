#ifndef APP_LAYOUT_H
#define APP_LAYOUT_H
/* Interface geometry in the fixed 1280x720 interface space, shared by rendering, input and the
 * GUI self-test. Pure module: no SDL/OpenGL. Change a rectangle here, never in callers. */
#include "workbench.h"
typedef struct
{
    float x, y, w, h;
} AppRect;

int app_rect_contains(AppRect rect, float x, float y);
float app_rect_center_x(AppRect rect);
float app_rect_center_y(AppRect rect);
/* Centered, proportional viewport in physical pixels; input uses window coordinates. */
AppRect app_layout_viewport(int width, int height);
WirePoint app_layout_to_ui(int width, int height, float x, float y);
WirePoint app_layout_from_ui(int width, int height, float x, float y);

/* Panels are designed on a 1280 x 720 canvas. The real canvas is the window divided by the interface
 * scale, never smaller: extra width and height go to the workspace, and panels follow their anchor. */
#define APP_DESIGN_WIDTH 1280.0f
#define APP_DESIGN_HEIGHT 720.0f
#define APP_FOOTER_HEIGHT 60.0f
#define APP_HEADER_HEIGHT 68.0f
#define APP_PALETTE_WIDTH 220.0f
#define APP_PALETTE_TABS 12
/* Catalog cards that fit the canvas height (7 on the design canvas). */
int app_layout_palette_visible(void);
#define APP_PALETTE_VISIBLE app_layout_palette_visible()

typedef enum
{
    APP_HEADER_PROJECTS,
    APP_HEADER_EXAMPLES,
    APP_HEADER_SETTINGS,
    APP_HEADER_PLAY,
    APP_HEADER_COUNT
} AppHeaderButton;
AppRect app_layout_header(AppHeaderButton button);
/* Returns APP_HEADER_COUNT when no button is hit. */
AppHeaderButton app_layout_header_at(float x, float y);

typedef enum
{
    APP_ACTION_UNDO,
    APP_ACTION_REDO,
    APP_ACTION_STEP,
    APP_ACTION_CHIP,
    APP_ACTION_GRAPH,
    APP_ACTION_CURVE,
    APP_ACTION_COUNT
} AppAction;
AppRect app_layout_action(AppAction action);
AppRect app_layout_search(void);
AppRect app_layout_search_clear(void);
AppRect app_layout_value_panel(void);
AppRect app_layout_value_button(int confirm);
AppRect app_layout_graph_measure(void);
AppRect app_layout_graph_pin(void);

AppRect app_layout_palette_tab(int tab);
int app_layout_palette_tab_at(float x, float y);
AppRect app_layout_palette_item(int slot);
int app_layout_palette_item_at(float x, float y);
AppRect app_layout_palette_scrollbar(void);
/* Remove button on a saved block card (Blocos tab). */
AppRect app_layout_palette_delete(int slot);
/* Palette swatches of the LED and wire panels (same row, same order: BenchColor). */
AppRect app_layout_color_swatch(int color);
int app_layout_color_swatch_at(float x, float y);
/* Wire panel: the inspector shown while a wire is selected. */
AppRect app_layout_wire_inspector(void);
/* Microcontroller code editor (right side panel). */
#define APP_CODE_GUTTER 36.0f
AppRect app_layout_code_panel(void);
AppRect app_layout_code_run(void);
AppRect app_layout_code_close(void);
/* Graph window (T): static curve or live graph, with its close button. */
AppRect app_layout_graph_panel(void);
AppRect app_layout_graph_close(void);
AppRect app_layout_code_text(void);
AppRect app_layout_code_serial(void);

typedef enum
{
    APP_SETTING_DARK_THEME,
    APP_SETTING_FONT_SMALLER,
    APP_SETTING_FONT_LARGER,
    APP_SETTING_SHOW_GRID,
    APP_SETTING_ROUND_WIRES,
    APP_SETTING_HANDLES,
    APP_SETTING_SNAP_GRID,
    APP_SETTING_GRID_SPACING,
    APP_SETTING_SNAP_BOARD,
    APP_SETTING_SHOW_CURRENT,
    APP_SETTING_SIM_SPEED_SLOWER,
    APP_SETTING_SIM_SPEED_FASTER,
    APP_SETTING_PROBE_MEASURE,
    APP_SETTING_PROBE_PIN,
    APP_SETTING_STEP,
    APP_SETTING_STOP,
    APP_SETTING_PIN_LABELS,
    APP_SETTING_CONNECTION_PINS,
    APP_SETTING_LANGUAGE_PT_BR,
    APP_SETTING_LANGUAGE_EN_US,
    APP_SETTING_RESET,
    APP_SETTING_CLOSE,
    APP_SETTING_COUNT
} AppSetting;
AppRect app_layout_settings_panel(void);
AppRect app_layout_setting(AppSetting setting);
/* Returns APP_SETTING_COUNT when no setting is hit. */
AppSetting app_layout_setting_at(float x, float y);
/* Section heading above a row: column 0 = left, 1 = right. */
AppRect app_layout_settings_section(int column, int row);

#define APP_PROJECT_SLOTS 6
AppRect app_layout_projects_panel(void);
AppRect app_layout_project_slot(int slot);
int app_layout_project_slot_at(float x, float y);
AppRect app_layout_project_save(void);
AppRect app_layout_project_load(void);
AppRect app_layout_project_close(void);
AppRect app_layout_project_previous(void);
AppRect app_layout_project_next(void);

AppRect app_layout_inspector(float height);
/* Header button that collapses the inspector into the small tab below. */
AppRect app_layout_inspector_minimize(void);
/* Collapsed inspector: a small tab with the part name; clicking it expands the panel again. */
AppRect app_layout_inspector_tab(void);
AppRect app_layout_inspector_rotate(void);
AppRect app_layout_inspector_mirror(void);
AppRect app_layout_inspector_doc(void);
AppRect app_layout_doc_panel(void);
AppRect app_layout_doc_close(void);
AppRect app_layout_doc_package(void);
AppRect app_layout_doc_pin_list(void);
AppRect app_layout_inspector_value(void);
AppRect app_layout_inspector_scale(void);
AppRect app_layout_environment_slider(void);
AppRect app_layout_environment_track(void);
/* Normalized slider position for a pointer x coordinate, clamped to 0..1. */
double app_layout_environment_position(float x);
typedef enum
{
    APP_EDITOR_APPLY,
    APP_EDITOR_RENAME,
    APP_EDITOR_SAVE,
    APP_EDITOR_BACK,
    APP_EDITOR_COUNT
} AppEditorButton;
AppRect app_layout_editor_header(AppEditorButton button);
AppEditorButton app_layout_editor_header_at(float x, float y);
/* Group inspector: 0 = view interior. */
AppRect app_layout_group_button(int index);
int app_layout_group_button_at(float x, float y);
/* Unsaved-editor dialog: 0 = save and return, 1 = discard, 2 = cancel. */
#define APP_CONFIRM_BUTTONS 3
AppRect app_layout_confirm_panel(void);
AppRect app_layout_confirm_button(int index);
int app_layout_confirm_button_at(float x, float y);
/* Examples dialog: one card per example, three columns. */
#define APP_EXAMPLE_CARDS 18
AppRect app_layout_examples_panel(void);
AppRect app_layout_example_card(int index);
int app_layout_example_card_at(float x, float y);
AppRect app_layout_examples_close(void);
/* Recomputes the canvas for a window of this size (any unit) and scale (device units per canvas unit). */
void app_layout_set_canvas(float window_width, float window_height, float scale);
float app_layout_canvas_width(void);
float app_layout_canvas_height(void);
/* How far the graph panel moved from its design position (right and bottom anchored). */
WirePoint app_layout_graph_offset(void);
#endif
