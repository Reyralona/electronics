#ifndef APP_LAYOUT_H
#define APP_LAYOUT_H
/* Interface geometry in the fixed 1280x720 interface space, shared by rendering, input and the
 * GUI self-test. Pure module: no SDL/OpenGL. Change a rectangle here, never in callers. */
typedef struct
{
    float x, y, w, h;
} AppRect;

int app_rect_contains(AppRect rect, float x, float y);
float app_rect_center_x(AppRect rect);
float app_rect_center_y(AppRect rect);

#define APP_HEADER_HEIGHT 68.0f
#define APP_PALETTE_WIDTH 220.0f
#define APP_PALETTE_TABS 3
#define APP_PALETTE_VISIBLE 8

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

AppRect app_layout_palette_tab(int tab);
int app_layout_palette_tab_at(float x, float y);
AppRect app_layout_palette_item(int slot);
int app_layout_palette_item_at(float x, float y);
AppRect app_layout_palette_scrollbar(void);

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
    APP_SETTING_PROBE_MEASURE,
    APP_SETTING_PROBE_PIN,
    APP_SETTING_STEP,
    APP_SETTING_STOP,
    APP_SETTING_RESET,
    APP_SETTING_CLOSE,
    APP_SETTING_COUNT
} AppSetting;
#define APP_WIRE_SWATCHES 6
AppRect app_layout_settings_panel(void);
AppRect app_layout_setting(AppSetting setting);
/* Returns APP_SETTING_COUNT when no setting is hit. */
AppSetting app_layout_setting_at(float x, float y);
/* Section heading above a row: column 0 = left, 1 = right. */
AppRect app_layout_settings_section(int column, int row);
AppRect app_layout_wire_swatch(int index);
int app_layout_wire_swatch_at(float x, float y);

#define APP_PROJECT_SLOTS 9
AppRect app_layout_projects_panel(void);
AppRect app_layout_project_slot(int slot);
int app_layout_project_slot_at(float x, float y);
AppRect app_layout_project_save(void);
AppRect app_layout_project_load(void);
AppRect app_layout_project_close(void);

AppRect app_layout_inspector(float height);
AppRect app_layout_inspector_rotate(void);
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
/* Group inspector: 0 = view interior, 1 = rename. */
AppRect app_layout_group_button(int index);
int app_layout_group_button_at(float x, float y);
/* Unsaved-editor dialog: 0 = save and return, 1 = discard, 2 = cancel. */
#define APP_CONFIRM_BUTTONS 3
AppRect app_layout_confirm_panel(void);
AppRect app_layout_confirm_button(int index);
int app_layout_confirm_button_at(float x, float y);
#endif
