#include "app_layout.h"

#define PALETTE_ITEM_TOP 172.0f
#define PALETTE_ITEM_PITCH 64.0f
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
#define INSPECTOR_X 840.0f
#define INSPECTOR_W 420.0f
#define INSPECTOR_ROW_X 852.0f
#define INSPECTOR_ROW_W 396.0f
#define CLOSE_BUTTON 36.0f
#define NO_RECT ((AppRect){0, 0, 0, 0})

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
    return buttons[button];
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
    return (AppRect){14 + (tab % 3) * 64.0f, 80 + (tab / 3) * 42.0f, 64, 38};
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
    return (AppRect){14, PALETTE_ITEM_TOP + slot * PALETTE_ITEM_PITCH, 186, 58};
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
    return (AppRect){250, 84, 900, 560};
}

static AppRect settings_row(float column, int row)
{
    return (AppRect){column, SETTINGS_FIRST_ROW + row * SETTINGS_ROW_PITCH, SETTINGS_COLUMN, SETTINGS_ROW_H};
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
        return settings_row(SETTINGS_RIGHT, 2);
    case APP_SETTING_PROBE_MEASURE:
        return settings_row(SETTINGS_RIGHT, 5);
    case APP_SETTING_PROBE_PIN:
        return settings_row(SETTINGS_RIGHT, 6);
    case APP_SETTING_STEP:
        return settings_row(SETTINGS_RIGHT, 7);
    case APP_SETTING_STOP:
        return settings_row(SETTINGS_RIGHT, 8);
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

AppRect app_layout_wire_swatch(int index)
{
    if (index < 0 || index >= APP_WIRE_SWATCHES)
        return NO_RECT;
    AppRect row = settings_row(SETTINGS_RIGHT, 0);
    return (AppRect){row.x + index * 52.0f, row.y + 26, 44, 30};
}

int app_layout_wire_swatch_at(float x, float y)
{
    for (int index = 0; index < APP_WIRE_SWATCHES; ++index)
        if (app_rect_contains(app_layout_wire_swatch(index), x, y))
            return index;
    return -1;
}

AppRect app_layout_projects_panel(void)
{
    return (AppRect){300, 110, 680, 450};
}

AppRect app_layout_project_slot(int slot)
{
    if (slot < 0 || slot >= APP_PROJECT_SLOTS)
        return NO_RECT;
    return (AppRect){330 + (slot % 3) * 210.0f, 196 + (slot / 3) * 92.0f, 198, 80};
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
    return (AppRect){330, 486, 300, 44};
}

AppRect app_layout_project_load(void)
{
    return (AppRect){650, 486, 300, 44};
}

AppRect app_layout_project_close(void)
{
    return close_button(app_layout_projects_panel());
}

AppRect app_layout_inspector(float height)
{
    return (AppRect){INSPECTOR_X, 80, INSPECTOR_W, height};
}

AppRect app_layout_inspector_rotate(void)
{
    return (AppRect){1172, 90, 76, 30};
}

AppRect app_layout_inspector_value(void)
{
    return (AppRect){INSPECTOR_ROW_X, 132, INSPECTOR_ROW_W, 30};
}

AppRect app_layout_inspector_scale(void)
{
    return (AppRect){INSPECTOR_ROW_X, 168, INSPECTOR_ROW_W, 30};
}

AppRect app_layout_environment_slider(void)
{
    return (AppRect){INSPECTOR_ROW_X - 10, 268, INSPECTOR_ROW_W + 20, 32};
}

AppRect app_layout_environment_track(void)
{
    return (AppRect){INSPECTOR_ROW_X + 4, 281, INSPECTOR_ROW_W - 8, 6};
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
    return buttons[button];
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
    if (index < 0 || index > 1)
        return NO_RECT;
    return (AppRect){INSPECTOR_ROW_X + index * 202.0f, 132, 194, 32};
}

int app_layout_group_button_at(float x, float y)
{
    for (int index = 0; index < 2; ++index)
        if (app_rect_contains(app_layout_group_button(index), x, y))
            return index;
    return -1;
}

AppRect app_layout_confirm_panel(void)
{
    return (AppRect){320, 250, 640, 190};
}

AppRect app_layout_confirm_button(int index)
{
    if (index < 0 || index >= APP_CONFIRM_BUTTONS)
        return NO_RECT;
    return (AppRect){350 + index * 196.0f, 370, 184, 44};
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
    return (AppRect){270, 84, 960, 560};
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
