#include "app_controls.h"
#include "app_palette.h"
#include "app_editing.h"
#include "component_registry.h"
#include <assert.h>

AppControlContext app_controls_context(const AppState *app)
{
    if (app->project_overwrite >= 0)
        return APP_UI_REPLACE;
    if (app->editor_confirm_close)
        return APP_UI_EDITOR_CONFIRM;
    if (app->editing >= 0)
        return APP_UI_VALUE;
    if (app->settings)
        return APP_UI_SETTINGS;
    if (app->projects_open)
        return APP_UI_PROJECTS;
    if (app->examples_open)
        return APP_UI_EXAMPLES;
    return app->code_part >= 0 ? APP_UI_CODE : APP_UI_BENCH;
}

int app_controls_graph_visible(const AppState *app)
{
    return app_controls_context(app) == APP_UI_BENCH &&
           (app->live_graph.active || (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->show_trace));
}

int app_controls_action_enabled(const AppState *app, AppAction action)
{
    if (action == APP_ACTION_UNDO)
        return app->history && app->history->undo_count > 0;
    if (action == APP_ACTION_REDO)
        return app->history && app->history->redo_count > 0;
    if (action == APP_ACTION_STEP)
        return !app->sim.sim_running;
    if (action == APP_ACTION_CHIP)
        return app->selected >= 0 || bench_selection_count(&app->selection, &app->bench) > 0;
    return (app->selected >= 0 && app->selected < app->bench.part_count &&
            !bench_is_board(app->bench.parts[app->selected].type)) ||
           (app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count);
}

static void target(AppRect targets[APP_CONTROL_LIMIT], int *count, AppRect rect)
{
    assert(*count < APP_CONTROL_LIMIT);
    targets[(*count)++] = rect;
}

static void inspector_targets(const AppState *app, AppRect *targets, int *count)
{
    int part_selected = app->selected >= 0 && app->selected < app->bench.part_count;
    if (!part_selected && app->selected_wire < 0)
        return;
    target(targets, count, app->inspector_minimized ? app_layout_inspector_tab() : app_layout_inspector_minimize());
    if (app->inspector_minimized)
        return;
    if (!part_selected)
    {
        for (int i = 0; i < BENCH_COLOR_COUNT; ++i)
            target(targets, count, app_layout_color_swatch(i));
        return;
    }
    const Part *part = &app->bench.parts[app->selected];
    if (part->group)
    {
        target(targets, count, app_layout_group_button(0));
        return;
    }
    const ComponentInfo *info = component_info(part->type);
    if (info && (app_selection_value_target(app) >= 0 || part->type == PART_NOTE || part->type == PART_ARDUINO_UNO || part->type == PART_ESP32 || part->type == PART_ATTINY85))
        target(targets, count, app_layout_inspector_value());
    target(targets, count, app_layout_inspector_scale());
    if (!bench_is_board(part->type))
    {
        target(targets, count, app_layout_inspector_rotate());
        target(targets, count, app_layout_inspector_mirror());
    }
    if (part->type == PART_LED)
        for (int i = 0; i < COMPONENT_LED_COLORS; ++i)
            target(targets, count, app_layout_color_swatch(i));
    if (component_environment(part->type))
        target(targets, count, app_layout_environment_slider());
}

int app_controls_targets(const AppState *app, AppRect targets[APP_CONTROL_LIMIT])
{
    int count = 0;
    AppControlContext context = app_controls_context(app);
    if (context == APP_UI_REPLACE)
    {
        target(targets, &count, app_layout_confirm_button(2));
        target(targets, &count, app_layout_confirm_button(0));
        return count;
    }
    if (context == APP_UI_EDITOR_CONFIRM)
    {
        for (int i = 0; i < APP_CONFIRM_BUTTONS; ++i)
            target(targets, &count, app_layout_confirm_button(i));
        return count;
    }
    if (context == APP_UI_VALUE)
    {
        target(targets, &count, app_layout_value_button(1));
        target(targets, &count, app_layout_value_button(0));
        return count;
    }
    if (context == APP_UI_SETTINGS)
    {
        for (int i = 0; i < APP_SETTING_COUNT; ++i)
            target(targets, &count, app_layout_setting((AppSetting)i));
        return count;
    }
    if (context == APP_UI_PROJECTS)
    {
        for (int i = 0; i < APP_PROJECT_SLOTS; ++i)
            target(targets, &count, app_layout_project_slot(i));
        target(targets, &count, app_layout_project_save());
        if (app->project_page > 0)
            target(targets, &count, app_layout_project_previous());
        if ((app->project_page + 1) * PROJECT_SLOTS_PER_PAGE < app->project_slot_count)
            target(targets, &count, app_layout_project_next());
        if (app->saved_slots && app->save_slot < app->project_slot_count && app->saved_slots[app->save_slot])
            target(targets, &count, app_layout_project_load());
        target(targets, &count, app_layout_project_close());
        return count;
    }
    if (context == APP_UI_EXAMPLES)
    {
        for (int i = 0; i < APP_EXAMPLE_CARDS; ++i)
            target(targets, &count, app_layout_example_card(i));
        target(targets, &count, app_layout_examples_close());
        return count;
    }
    if (app->editor_parent)
    {
        for (int i = 0; i < APP_EDITOR_COUNT; ++i)
            target(targets, &count, app_layout_editor_header((AppEditorButton)i));
    }
    else
        for (int i = 0; i < APP_HEADER_PLAY; ++i)
            target(targets, &count, app_layout_header((AppHeaderButton)i));
    target(targets, &count, app_layout_header(APP_HEADER_PLAY));
    if (context == APP_UI_CODE)
    {
        target(targets, &count, app_layout_code_run());
        target(targets, &count, app_layout_code_close());
        return count;
    }
    for (int i = 0; i < APP_ACTION_COUNT; ++i)
        if (app_controls_action_enabled(app, (AppAction)i))
            target(targets, &count, app_layout_action((AppAction)i));
    for (int i = 0; i < APP_PALETTE_TABS; ++i)
        target(targets, &count, app_layout_palette_tab(i));
    target(targets, &count, app_layout_search());
    if (app->palette_search[0])
        target(targets, &count, app_layout_search_clear());
    int visible = palette_filtered_count(app->palette_page, app->palette_search) - app->palette_offset;
    for (int i = 0; i < APP_PALETTE_VISIBLE && i < visible; ++i)
    {
        if (app->palette_page == 2 && !app->palette_search[0] && !app->library_available[i])
            continue;
        target(targets, &count, app_layout_palette_item(i));
        if (app->palette_page == 2 && !app->palette_search[0])
            target(targets, &count, app_layout_palette_delete(i));
    }
    inspector_targets(app, targets, &count);
    if (app_controls_graph_visible(app))
    {
        if (!app->live_graph.active || app->live_graph.probe.kind != SIM_LIVE_PROBE_WIRE_CURRENT)
            target(targets, &count, app_layout_graph_measure());
        target(targets, &count, app_layout_graph_pin());
        target(targets, &count, app_layout_graph_close());
    }
    return count;
}

AppRect app_controls_focus(const AppState *app)
{
    AppRect targets[APP_CONTROL_LIMIT];
    int count = app_controls_targets(app, targets);
    if (app->keyboard_context != app_controls_context(app) || app->keyboard_focus < 0)
        return (AppRect){0};
    for (int i = 0; i < count; ++i)
        if (app_rect_center_x(targets[i]) == app->keyboard_target.x && app_rect_center_y(targets[i]) == app->keyboard_target.y)
            return targets[i];
    return (AppRect){0};
}
