#include "app_layout.h"

#define PALETTE_ITEM_TOP 140.0f
#define PALETTE_ITEM_PITCH 72.0f
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
        {500, 0, 180, APP_HEADER_HEIGHT},
        {700, 0, 150, APP_HEADER_HEIGHT},
        {880, 0, 150, APP_HEADER_HEIGHT},
        {1040, 0, 240, APP_HEADER_HEIGHT},
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
    return (AppRect){12 + tab * 70.0f, 80, 62, 44};
}

int app_layout_palette_tab_at(float x, float y)
{
    if (x < 0 || x >= APP_PALETTE_WIDTH || y < 80 || y >= 125)
        return -1;
    if (x < 78)
        return 0;
    return x < 148 ? 1 : 2;
}

AppRect app_layout_palette_item(int slot)
{
    if (slot < 0 || slot >= APP_PALETTE_VISIBLE)
        return NO_RECT;
    return (AppRect){14, PALETTE_ITEM_TOP + slot * PALETTE_ITEM_PITCH, 192, 62};
}

int app_layout_palette_item_at(float x, float y)
{
    if (x < 0 || x >= APP_PALETTE_WIDTH || y < PALETTE_ITEM_TOP)
        return -1;
    int slot = (int)((y - PALETTE_ITEM_TOP) / PALETTE_ITEM_PITCH);
    return slot < APP_PALETTE_VISIBLE ? slot : -1;
}

AppRect app_layout_settings_panel(void)
{
    return (AppRect){260, 84, 980, 558};
}

AppRect app_layout_setting(AppSetting setting)
{
    static const AppRect rows[APP_SETTING_COUNT] = {
        [APP_SETTING_DARK_THEME] = {260, 340, 540, 45},
        [APP_SETTING_FONT_SMALLER] = {500, 410, 40, 40},
        [APP_SETTING_FONT_LARGER] = {550, 410, 40, 40},
        [APP_SETTING_SHOW_GRID] = {851, 120, 389, 40},
        [APP_SETTING_ROUND_WIRES] = {851, 200, 389, 40},
        [APP_SETTING_HANDLES] = {851, 160, 389, 40},
        [APP_SETTING_SNAP_GRID] = {260, 130, 540, 40},
        [APP_SETTING_GRID_SPACING] = {260, 170, 540, 40},
        [APP_SETTING_SNAP_BOARD] = {260, 210, 540, 40},
        [APP_SETTING_SHOW_CURRENT] = {851, 240, 389, 40},
        [APP_SETTING_PROBE_MEASURE] = {851, 354, 389, 36},
        [APP_SETTING_PROBE_PIN] = {851, 390, 389, 35},
        [APP_SETTING_STEP] = {851, 430, 389, 35},
        [APP_SETTING_STOP] = {851, 470, 389, 35},
    };
    if (setting < 0 || setting >= APP_SETTING_COUNT)
        return NO_RECT;
    return rows[setting];
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
    return (AppRect){860 + index * 50.0f, 320, 50, 32};
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
    return (AppRect){300, 130, 680, 438};
}

AppRect app_layout_project_slot(int slot)
{
    if (slot < 0 || slot >= APP_PROJECT_SLOTS)
        return NO_RECT;
    return (AppRect){330 + (slot % 3) * 200.0f, 200 + (slot / 3) * 90.0f, 200, 90};
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
    return (AppRect){340, 500, 250, 45};
}

AppRect app_layout_project_load(void)
{
    return (AppRect){640, 500, 250, 45};
}

AppRect app_layout_project_close(void)
{
    return NO_RECT;
}

AppRect app_layout_inspector(float height)
{
    return (AppRect){840, 80, 420, height};
}

AppRect app_layout_inspector_rotate(void)
{
    return (AppRect){1180, 86, 72, 26};
}

AppRect app_layout_inspector_value(void)
{
    return NO_RECT;
}

AppRect app_layout_inspector_scale(void)
{
    return (AppRect){850, 154, 330, 28};
}

AppRect app_layout_environment_slider(void)
{
    return (AppRect){840, 245, 350, 33};
}

AppRect app_layout_environment_track(void)
{
    return (AppRect){850, 254, 330, 6};
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
        {350, 0, 140, APP_HEADER_HEIGHT},
        {500, 0, 180, APP_HEADER_HEIGHT},
        {700, 0, 150, APP_HEADER_HEIGHT},
        {880, 0, 150, APP_HEADER_HEIGHT},
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
    return (AppRect){850 + index * 200.0f, 116, 194, 29};
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
    return (AppRect){360 + index * 190.0f, 370, 180, 45};
}

int app_layout_confirm_button_at(float x, float y)
{
    for (int index = 0; index < APP_CONFIRM_BUTTONS; ++index)
        if (app_rect_contains(app_layout_confirm_button(index), x, y))
            return index;
    return -1;
}
