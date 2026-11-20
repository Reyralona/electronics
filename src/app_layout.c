#include "app_layout.h"
#include "workbench_view.h"
#include <math.h>

/* Category tabs: rows of three under the header; search below them, then the part list. */
#define PALETTE_TAB_TOP 80.0f
#define PALETTE_TAB_PITCH 42.0f
#define PALETTE_TAB_ROWS ((APP_PALETTE_TABS + 2) / 3)
#define PALETTE_SEARCH_TOP (PALETTE_TAB_TOP + PALETTE_TAB_ROWS * PALETTE_TAB_PITCH + 6)
#define PALETTE_ITEM_TOP (PALETTE_SEARCH_TOP + 40)
#define PALETTE_ITEM_PITCH 59.0f
#define APP_PALETTE_BOTTOM_MARGIN 8.0f
#define HEADER_BUTTON_Y 14.0f
#define HEADER_BUTTON_H 40.0f
#define SETTINGS_LEFT 280.0f
#define SETTINGS_RIGHT 720.0f
#define SETTINGS_COLUMN 400.0f
#define SETTINGS_FIRST_ROW 176.0f
#define SETTINGS_ROW_PITCH 40.0f
#define SETTINGS_ROW_H 34.0f
#define STEPPER_W 34.0f
#define STEPPER_VALUE_W 56.0f
/* Compact inspector, right-aligned at the same edge as before (x 1260). */
#define INSPECTOR_RIGHT (1260.0f + right_shift())
#define INSPECTOR_W 320.0f
#define INSPECTOR_X (INSPECTOR_RIGHT - INSPECTOR_W)
#define INSPECTOR_TOP 80.0f
#define INSPECTOR_PAD 10.0f
#define INSPECTOR_ROW_X (INSPECTOR_X + INSPECTOR_PAD)
#define INSPECTOR_ROW_W (INSPECTOR_W - 2 * INSPECTOR_PAD)
#define INSPECTOR_ROW_H 26.0f
#define INSPECTOR_HEADER_BUTTON 26.0f
#define INSPECTOR_HEADER_Y (INSPECTOR_TOP + 7)
#define INSPECTOR_ROTATE_W 64.0f
#define INSPECTOR_MIRROR_W 78.0f
#define INSPECTOR_TAB_W 180.0f
#define INSPECTOR_TAB_H 32.0f
#define INSPECTOR_SWATCH_W 22.0f
#define INSPECTOR_SWATCH_H 20.0f
#define INSPECTOR_SWATCH_PITCH 27.0f
#define INSPECTOR_WIRE_HEIGHT 200.0f
#define CLOSE_BUTTON 36.0f
#define FLAG_W 92.0f
#define FLAG_H 32.0f
#define FLAG_GAP 10.0f
#define NO_RECT ((AppRect){0, 0, 0, 0})

static float canvas_width = APP_DESIGN_WIDTH, canvas_height = APP_DESIGN_HEIGHT;

/* Offsets of the anchors from the design canvas: centered dialogs, right column, bottom row. */
static float center_shift_x(void)
{
    return (canvas_width - APP_DESIGN_WIDTH) / 2;
}

static float center_shift_y(void)
{
    return (canvas_height - APP_DESIGN_HEIGHT) / 2;
}

static float right_shift(void)
{
    return canvas_width - APP_DESIGN_WIDTH;
}

static float bottom_shift(void)
{
    return canvas_height - APP_DESIGN_HEIGHT;
}

static AppRect shifted(AppRect rect, float dx, float dy)
{
    return (AppRect){rect.x + dx, rect.y + dy, rect.w, rect.h};
}

static AppRect centered(AppRect rect)
{
    return shifted(rect, center_shift_x(), center_shift_y());
}

void app_layout_set_canvas(float window_width, float window_height, float scale)
{
    if (!(scale > 0) || !isfinite(window_width) || !isfinite(window_height))
        return;
    canvas_width = fmaxf(APP_DESIGN_WIDTH, floorf(window_width / scale));
    canvas_height = fmaxf(APP_DESIGN_HEIGHT, floorf(window_height / scale));
    bench_view_set_area(canvas_width - BENCH_VIEW_LEFT, canvas_height - BENCH_VIEW_TOP - APP_FOOTER_HEIGHT);
}

float app_layout_canvas_width(void)
{
    return canvas_width;
}

float app_layout_canvas_height(void)
{
    return canvas_height;
}

int app_layout_palette_visible(void)
{
    return (int)((canvas_height - PALETTE_ITEM_TOP - APP_PALETTE_BOTTOM_MARGIN) / PALETTE_ITEM_PITCH);
}

WirePoint app_layout_graph_offset(void)
{
    return (WirePoint){right_shift(), bottom_shift()};
}

int app_rect_contains(AppRect rect, float x, float y)
{
    return rect.w > 0 && rect.h > 0 && x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

float app_rect_center_x(AppRect rect)
{
    return rect.x + rect.w / 2;
}

float app_rect_center_y(AppRect rect)
{
    return rect.y + rect.h / 2;
}

AppRect app_layout_viewport(int width, int height)
{
    if (width <= 0 || height <= 0)
        return NO_RECT;
    /* Uniform scale; only a window smaller than the canvas at this scale leaves margins. */
    float scale = fminf(width / canvas_width, height / canvas_height);
    float w = canvas_width * scale, h = canvas_height * scale;
    return (AppRect){(width - w) / 2, (height - h) / 2, w, h};
}

WirePoint app_layout_to_ui(int width, int height, float x, float y)
{
    AppRect viewport = app_layout_viewport(width, height);
    if (viewport.w <= 0 || viewport.h <= 0)
        return (WirePoint){-1, -1};
    return (WirePoint){(x - viewport.x) * canvas_width / viewport.w, (y - viewport.y) * canvas_height / viewport.h};
}

WirePoint app_layout_from_ui(int width, int height, float x, float y)
{
    AppRect viewport = app_layout_viewport(width, height);
    return (WirePoint){viewport.x + x * viewport.w / canvas_width, viewport.y + y * viewport.h / canvas_height};
}

AppRect app_layout_header(AppHeaderButton button)
{
    static const AppRect buttons[APP_HEADER_COUNT] = {
        {500, HEADER_BUTTON_Y, 132, HEADER_BUTTON_H},
        {642, HEADER_BUTTON_Y, 132, HEADER_BUTTON_H},
        {784, HEADER_BUTTON_Y, 176, HEADER_BUTTON_H},
        {1040, 12, 222, 44},
    };
    if (button < 0 || button >= APP_HEADER_COUNT)
        return NO_RECT;
    /* The navigation stays centered; the simulation switch stays at the right edge. */
    return button == APP_HEADER_COUNT - 1 ? shifted(buttons[button], right_shift(), 0)
                                          : shifted(buttons[button], center_shift_x(), 0);
}

AppHeaderButton app_layout_header_at(float x, float y)
{
    for (int button = 0; button < APP_HEADER_COUNT; ++button)
        if (app_rect_contains(app_layout_header((AppHeaderButton)button), x, y))
            return (AppHeaderButton)button;
    return APP_HEADER_COUNT;
}

AppRect app_layout_palette_tab(int tab)
{
    if (tab < 0 || tab >= APP_PALETTE_TABS)
        return NO_RECT;
    return (AppRect){14 + (tab % 3) * 64.0f, PALETTE_TAB_TOP + (tab / 3) * PALETTE_TAB_PITCH, 64, 38};
}

int app_layout_palette_tab_at(float x, float y)
{
    for (int tab = 0; tab < APP_PALETTE_TABS; ++tab)
        if (app_rect_contains(app_layout_palette_tab(tab), x, y))
            return tab;
    return -1;
}

AppRect app_layout_palette_item(int slot)
{
    if (slot < 0 || slot >= APP_PALETTE_VISIBLE)
        return NO_RECT;
    return (AppRect){14, PALETTE_ITEM_TOP + slot * PALETTE_ITEM_PITCH, 186, 53};
}

int app_layout_palette_item_at(float x, float y)
{
    if (x < 0 || x >= APP_PALETTE_WIDTH || y < PALETTE_ITEM_TOP)
        return -1;
    int slot = (int)((y - PALETTE_ITEM_TOP) / PALETTE_ITEM_PITCH);
    return slot < APP_PALETTE_VISIBLE ? slot : -1;
}

AppRect app_layout_palette_scrollbar(void)
{
    return (AppRect){205, PALETTE_ITEM_TOP, 5, APP_PALETTE_VISIBLE * PALETTE_ITEM_PITCH - 8};
}

AppRect app_layout_settings_panel(void)
{
    return centered((AppRect){250, 84, 900, 560});
}

static AppRect settings_row(float column, int row)
{
    return centered((AppRect){column, SETTINGS_FIRST_ROW + row * SETTINGS_ROW_PITCH, SETTINGS_COLUMN, SETTINGS_ROW_H});
}

/* Stepper buttons sit at the right edge of a row: 0 = rightmost (+), 1 = left of the value (-). */
static AppRect row_button(AppRect row, int from_right)
{
    float x = row.x + row.w - STEPPER_W - from_right * (STEPPER_W + STEPPER_VALUE_W);
    return (AppRect){x, row.y, STEPPER_W, row.h};
}

static AppRect close_button(AppRect panel)
{
    return (AppRect){panel.x + panel.w - CLOSE_BUTTON - 16, panel.y + 14, CLOSE_BUTTON, CLOSE_BUTTON};
}

/* Language flags sit in the dialog header, left of the close button: 0 = pt-BR, 1 = en-US. */
static AppRect language_flag(AppRect panel, int index)
{
    AppRect close = close_button(panel);
    float right = close.x - 16 - (1 - index) * (FLAG_W + FLAG_GAP);
    return (AppRect){right - FLAG_W, close.y + (close.h - FLAG_H) * .5f, FLAG_W, FLAG_H};
}

AppRect app_layout_setting(AppSetting setting)
{
    AppRect panel = app_layout_settings_panel();
    switch (setting)
    {
    case APP_SETTING_DARK_THEME:
        return settings_row(SETTINGS_LEFT, 0);
    case APP_SETTING_FONT_SMALLER:
        return row_button(settings_row(SETTINGS_LEFT, 1), 1);
    case APP_SETTING_FONT_LARGER:
        return row_button(settings_row(SETTINGS_LEFT, 1), 0);
    case APP_SETTING_SHOW_GRID:
        return settings_row(SETTINGS_LEFT, 2);
    case APP_SETTING_ROUND_WIRES:
        return settings_row(SETTINGS_LEFT, 3);
    case APP_SETTING_HANDLES:
        return settings_row(SETTINGS_LEFT, 4);
    case APP_SETTING_PIN_LABELS:
        return settings_row(SETTINGS_LEFT, 5);
    case APP_SETTING_SNAP_GRID:
        return settings_row(SETTINGS_LEFT, 7);
    case APP_SETTING_GRID_SPACING:
        return settings_row(SETTINGS_LEFT, 8);
    case APP_SETTING_SNAP_BOARD:
        return settings_row(SETTINGS_LEFT, 9);
    case APP_SETTING_SHOW_CURRENT:
        return settings_row(SETTINGS_RIGHT, 0);
    case APP_SETTING_SIM_SPEED_SLOWER:
        return row_button(settings_row(SETTINGS_RIGHT, 1), 1);
    case APP_SETTING_SIM_SPEED_FASTER:
        return row_button(settings_row(SETTINGS_RIGHT, 1), 0);
    case APP_SETTING_PROBE_MEASURE:
        return settings_row(SETTINGS_RIGHT, 4);
    case APP_SETTING_PROBE_PIN:
        return settings_row(SETTINGS_RIGHT, 5);
    case APP_SETTING_STEP:
        return settings_row(SETTINGS_RIGHT, 6);
    case APP_SETTING_STOP:
        return settings_row(SETTINGS_RIGHT, 7);
    case APP_SETTING_LANGUAGE_PT_BR:
        return language_flag(panel, 0);
    case APP_SETTING_LANGUAGE_EN_US:
        return language_flag(panel, 1);
    case APP_SETTING_RESET:
        return (AppRect){panel.x + panel.w - 236, panel.y + panel.h - 54, 220, 36};
    case APP_SETTING_CLOSE:
        return close_button(panel);
    default:
        return NO_RECT;
    }
}

AppRect app_layout_settings_section(int column, int row)
{
    AppRect anchor = settings_row(column ? SETTINGS_RIGHT : SETTINGS_LEFT, row);
    return (AppRect){anchor.x, anchor.y - 30, SETTINGS_COLUMN, 26};
}

AppSetting app_layout_setting_at(float x, float y)
{
    for (int setting = 0; setting < APP_SETTING_COUNT; ++setting)
        if (app_rect_contains(app_layout_setting((AppSetting)setting), x, y))
            return (AppSetting)setting;
    return APP_SETTING_COUNT;
}

AppRect app_layout_projects_panel(void)
{
    return centered((AppRect){175, 64, 930, 592});
}

AppRect app_layout_project_slot(int slot)
{
    if (slot < 0 || slot >= APP_PROJECT_SLOTS)
        return NO_RECT;
    return centered((AppRect){201 + (slot % 3) * 305.0f, 161 + (slot / 3) * 205.0f, 288, 190});
}

int app_layout_project_slot_at(float x, float y)
{
    for (int slot = 0; slot < APP_PROJECT_SLOTS; ++slot)
        if (app_rect_contains(app_layout_project_slot(slot), x, y))
            return slot;
    return -1;
}

AppRect app_layout_project_save(void)
{
    return centered((AppRect){201, 584, 314, 44});
}

AppRect app_layout_project_load(void)
{
    return centered((AppRect){527, 584, 314, 44});
}

AppRect app_layout_project_previous(void)
{
    return centered((AppRect){863, 574, 90, 44});
}

AppRect app_layout_project_next(void)
{
    return centered((AppRect){967, 574, 90, 44});
}

AppRect app_layout_project_close(void)
{
    return close_button(app_layout_projects_panel());
}

AppRect app_layout_inspector(float height)
{
    return (AppRect){INSPECTOR_X, INSPECTOR_TOP, INSPECTOR_W, height};
}

AppRect app_layout_inspector_minimize(void)
{
    float x = INSPECTOR_RIGHT - INSPECTOR_PAD - INSPECTOR_HEADER_BUTTON;
    return (AppRect){x, INSPECTOR_HEADER_Y, INSPECTOR_HEADER_BUTTON, INSPECTOR_HEADER_BUTTON};
}

AppRect app_layout_inspector_tab(void)
{
    return (AppRect){INSPECTOR_RIGHT - INSPECTOR_TAB_W, INSPECTOR_TOP, INSPECTOR_TAB_W, INSPECTOR_TAB_H};
}

AppRect app_layout_inspector_rotate(void)
{
    float x = app_layout_inspector_minimize().x - 6 - INSPECTOR_ROTATE_W;
    return (AppRect){x, INSPECTOR_HEADER_Y, INSPECTOR_ROTATE_W, INSPECTOR_HEADER_BUTTON};
}

AppRect app_layout_inspector_mirror(void)
{
    float x = app_layout_inspector_rotate().x - 6 - INSPECTOR_MIRROR_W;
    return (AppRect){x, INSPECTOR_HEADER_Y, INSPECTOR_MIRROR_W, INSPECTOR_HEADER_BUTTON};
}

AppRect app_layout_inspector_doc(void)
{
    float x = app_layout_inspector_mirror().x - 6 - INSPECTOR_MIRROR_W;
    return (AppRect){x, INSPECTOR_HEADER_Y, INSPECTOR_MIRROR_W, INSPECTOR_HEADER_BUTTON};
}

/* IC sheet: zoomed package on the left, pin list on the right, pin explanation under the package. */
AppRect app_layout_doc_panel(void)
{
    return centered((AppRect){250, 84, 1000, 600});
}

AppRect app_layout_doc_close(void)
{
    return close_button(app_layout_doc_panel());
}

AppRect app_layout_doc_package(void)
{
    AppRect panel = app_layout_doc_panel();
    return (AppRect){panel.x + 30, panel.y + 150, 520, 300};
}

AppRect app_layout_doc_pin_list(void)
{
    AppRect panel = app_layout_doc_panel();
    return (AppRect){panel.x + 580, panel.y + 120, panel.w - 610, panel.h - 150};
}

AppRect app_layout_inspector_value(void)
{
    return (AppRect){INSPECTOR_ROW_X, INSPECTOR_TOP + 38, INSPECTOR_ROW_W, INSPECTOR_ROW_H};
}

AppRect app_layout_inspector_scale(void)
{
    return (AppRect){INSPECTOR_ROW_X, INSPECTOR_TOP + 68, INSPECTOR_ROW_W, INSPECTOR_ROW_H};
}

AppRect app_layout_environment_slider(void)
{
    return (AppRect){INSPECTOR_ROW_X - 6, INSPECTOR_TOP + 150, INSPECTOR_ROW_W + 12, 28};
}

AppRect app_layout_environment_track(void)
{
    return (AppRect){INSPECTOR_ROW_X + 4, INSPECTOR_TOP + 161, INSPECTOR_ROW_W - 8, 6};
}

double app_layout_environment_position(float x)
{
    AppRect track = app_layout_environment_track();
    double position = (x - track.x) / track.w;
    return position < 0 ? 0 : position > 1 ? 1 : position;
}

AppRect app_layout_editor_header(AppEditorButton button)
{
    static const AppRect buttons[APP_EDITOR_COUNT] = {
        {360, HEADER_BUTTON_Y, 150, HEADER_BUTTON_H},
        {520, HEADER_BUTTON_Y, 130, HEADER_BUTTON_H},
        {660, HEADER_BUTTON_Y, 150, HEADER_BUTTON_H},
        {820, HEADER_BUTTON_Y, 140, HEADER_BUTTON_H},
    };
    if (button < 0 || button >= APP_EDITOR_COUNT)
        return NO_RECT;
    return shifted(buttons[button], center_shift_x(), 0);
}

AppEditorButton app_layout_editor_header_at(float x, float y)
{
    for (int button = 0; button < APP_EDITOR_COUNT; ++button)
        if (app_rect_contains(app_layout_editor_header((AppEditorButton)button), x, y))
            return (AppEditorButton)button;
    return APP_EDITOR_COUNT;
}

AppRect app_layout_group_button(int index)
{
    if (index != 0)
        return NO_RECT;
    return (AppRect){INSPECTOR_ROW_X, INSPECTOR_TOP + 38, INSPECTOR_ROW_W, 28};
}

int app_layout_group_button_at(float x, float y)
{
    return app_rect_contains(app_layout_group_button(0), x, y) ? 0 : -1;
}

AppRect app_layout_confirm_panel(void)
{
    return centered((AppRect){320, 250, 640, 190});
}

AppRect app_layout_confirm_button(int index)
{
    if (index < 0 || index >= APP_CONFIRM_BUTTONS)
        return NO_RECT;
    return centered((AppRect){350 + index * 196.0f, 370, 184, 44});
}

int app_layout_confirm_button_at(float x, float y)
{
    for (int index = 0; index < APP_CONFIRM_BUTTONS; ++index)
        if (app_rect_contains(app_layout_confirm_button(index), x, y))
            return index;
    return -1;
}

#define EXAMPLE_COLUMNS 3
#define EXAMPLE_CARD_W 296.0f
#define EXAMPLE_CARD_H 72.0f
#define EXAMPLE_GAP 8.0f

AppRect app_layout_examples_panel(void)
{
    return centered((AppRect){270, 84, 960, 560});
}

AppRect app_layout_example_card(int index)
{
    if (index < 0 || index >= APP_EXAMPLE_CARDS)
        return NO_RECT;
    AppRect panel = app_layout_examples_panel();
    int column = index % EXAMPLE_COLUMNS, row = index / EXAMPLE_COLUMNS;
    return (AppRect){panel.x + 30 + column * (EXAMPLE_CARD_W + EXAMPLE_GAP), panel.y + 82 + row * (EXAMPLE_CARD_H + EXAMPLE_GAP),
                     EXAMPLE_CARD_W, EXAMPLE_CARD_H};
}

int app_layout_example_card_at(float x, float y)
{
    for (int index = 0; index < APP_EXAMPLE_CARDS; ++index)
        if (app_rect_contains(app_layout_example_card(index), x, y))
            return index;
    return -1;
}

AppRect app_layout_examples_close(void)
{
    return close_button(app_layout_examples_panel());
}

AppRect app_layout_palette_delete(int slot)
{
    AppRect card = app_layout_palette_item(slot);
    if (card.w <= 0)
        return card;
    return (AppRect){card.x + card.w - 30, card.y + (card.h - 24) / 2, 24, 24};
}

AppRect app_layout_color_swatch(int color)
{
    if (color < 0 || color >= BENCH_COLOR_COUNT)
        return NO_RECT;
    return (AppRect){INSPECTOR_ROW_X + color * INSPECTOR_SWATCH_PITCH, INSPECTOR_TOP + 146, INSPECTOR_SWATCH_W,
                     INSPECTOR_SWATCH_H};
}

int app_layout_color_swatch_at(float x, float y)
{
    for (int color = 0; color < BENCH_COLOR_COUNT; ++color)
        if (app_rect_contains(app_layout_color_swatch(color), x, y))
            return color;
    return -1;
}

AppRect app_layout_wire_inspector(void)
{
    return app_layout_inspector(INSPECTOR_WIRE_HEIGHT);
}

AppRect app_layout_code_panel(void)
{
    return (AppRect){836 + right_shift(), 76, 436, 578 + bottom_shift()};
}

AppRect app_layout_code_run(void)
{
    AppRect panel = app_layout_code_panel();
    return (AppRect){panel.x + panel.w - 200, panel.y + 14, 132, 34};
}

AppRect app_layout_code_close(void)
{
    return close_button(app_layout_code_panel());
}

AppRect app_layout_graph_panel(void)
{
    return shifted((AppRect){840, 384, 400, 244}, right_shift(), bottom_shift());
}

AppRect app_layout_graph_close(void)
{
    return close_button(app_layout_graph_panel());
}

AppRect app_layout_code_text(void)
{
    AppRect panel = app_layout_code_panel();
    return (AppRect){panel.x + 12, panel.y + 90, panel.w - 24, 360 + bottom_shift()};
}

AppRect app_layout_code_serial(void)
{
    AppRect panel = app_layout_code_panel();
    return (AppRect){panel.x + 12, panel.y + 458 + bottom_shift(), panel.w - 24, panel.h - 470 - bottom_shift()};
}

AppRect app_layout_action(AppAction action)
{
    if (action < 0 || action >= APP_ACTION_COUNT)
        return NO_RECT;
    return (AppRect){234 + action * 116.0f, 78, 108, 32};
}

AppRect app_layout_search(void)
{
    return (AppRect){14, PALETTE_SEARCH_TOP, 186, 32};
}

AppRect app_layout_search_clear(void)
{
    return (AppRect){172, PALETTE_SEARCH_TOP, 28, 32};
}

AppRect app_layout_value_panel(void)
{
    return centered((AppRect){410, 234, 460, 222});
}

AppRect app_layout_value_button(int confirm)
{
    return centered((AppRect){434 + (confirm ? 0 : 218), 392, 194, 40});
}

AppRect app_layout_graph_measure(void)
{
    return shifted((AppRect){840, 348, 192, 30}, right_shift(), bottom_shift());
}

AppRect app_layout_graph_pin(void)
{
    return shifted((AppRect){1040, 348, 200, 30}, right_shift(), bottom_shift());
}
