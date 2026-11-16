#include "bench_scope.h"
#include "bench_path.h"
#include "component_registry.h"
#include "workbench_grid.h"
#include "app_input.h"
#include "app_editor.h"
#include "workbench_group_name.h"
#include "app_commands.h"
#include "app_editing.h"
#include "app_packing.h"
#include "workbench_pack.h"
#include "app_layout.h"
#include "app_code_editor.h"
#include "component_devices.h"
#include "app_settings.h"
#include "ui_text.h"
#include "app_palette.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_trace.h"
#include <math.h>
_Static_assert(APP_EXAMPLE_CARDS == WORKBENCH_EXAMPLE_COUNT, "one examples card per example");
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Limits of the typed settings, shown in their error messages. */
enum
{
    GRID_SPACING_MIN = 1,
    GRID_SPACING_MAX = 10000,
    TRACE_MAX_POINTS = 500
};

/* Formats a status into selection_status, the buffer for messages with values. */
static void report(AppState *app, TextId format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(app->selection_status, sizeof(app->selection_status), tr(format), arguments);
    va_end(arguments);
    app->status = app->selection_status;
}

static int hit_wire(const Workbench *bench, float x, float y, float tolerance, int rounded)
{
    for (int i = bench->wire_count - 1; i >= 0; --i)
    {
        const Wire *wire = &bench->wires[i];
        if (bench_group_wire_internal(bench, wire))
            continue;
        WirePoint start, end, projected;
        int slot;
        bench_terminal_position(bench, wire->a, &start.x, &start.y);
        bench_terminal_position(bench, wire->b, &end.x, &end.y);
        if (wire_pick_segment(wire, start, end, rounded ? 18.0f : 0, (WirePoint){x, y}, tolerance, &slot, &projected))
            return i;
    }
    return -1;
}

static void input_insert_knee(AppState *app, float x, float y)
{
    int index = app->selected_wire, slot;
    const Wire *wire = &app->bench.wires[index];
    WirePoint start, end, projected;
    bench_terminal_position(&app->bench, wire->a, &start.x, &start.y);
    bench_terminal_position(&app->bench, wire->b, &end.x, &end.y);
    if (!wire_pick_segment(wire, start, end, app->round_wires ? 18.0f : 0,
                           (WirePoint){x, y}, 7.0f / app->view.zoom, &slot, &projected))
        return;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    if (!bench_insert_knee(&app->bench, index, slot, projected))
    {
        report(app, TXT_APP_BEND_LIMIT, WIRE_MAX_KNEES);
        return;
    }
    bench_history_record(app->history, &before);
    app->show_handles = 1;
    app->status = tr(TXT_APP_BEND_ADDED);
}

static int part_at(const Workbench *bench, float x, float y, float zoom)
{
    for (int g = WORKBENCH_MAX_COMPONENTS - 1; g >= 0; --g)
        if (bench->groups[g].active)
        {
            WirePoint local = bench_group_to_local(&bench->groups[g], (WirePoint){x, y});
            if (fabsf(local.x) < bench_group_width(bench, g + 1) * .5f + 11 &&
                fabsf(local.y) < bench_group_height(bench, g + 1) * .5f)
                return bench_group_first(bench, g + 1);
        }

    int pass, i;
    for (pass = 0; pass < 2; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            if (part->group)
                continue;
            int board = part->type == PART_BREADBOARD;
            if (board != pass)
                continue;
            WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
            float half_width, half_height;
            component_body_extent(part->type, &half_width, &half_height);
            float minimum_hit = 6 / (zoom * bench_part_scale(part));
            half_width = fmaxf(half_width, minimum_hit);
            half_height = fmaxf(half_height, minimum_hit);
            if (fabsf(local.x) < half_width && fabsf(local.y) < half_height)
                return i;
        }
    return -1;
}

static void input_text(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    (void)window;

    if (app->editing == APP_EDIT_GROUP_NAME)
    {
        const char *input = event->text.text;
        size_t before = app->edit_select_all ? 0 : strlen(app->edit_text);
        if ((!bench_group_name_valid(input) && strspn(input, " ") != strlen(input)) ||
            before + strlen(input) >= sizeof(app->edit_text))
        {
            app_editor_report_invalid_name(app);
            return;
        }
        memcpy(app->edit_text + before, input, strlen(input) + 1);
        app->edit_select_all = 0;
        return;
    }
    const char *p = event->text.text;
    size_t length = strlen(app->edit_text);
    while (*p && length + 1 < sizeof(app->edit_text))
    {
        if ((unsigned char)*p >= 32 && (unsigned char)*p < 127)
            app->edit_text[length++] = *p;
        ++p;
    }
    app->edit_text[length] = '\0';
}

static void input_edit_key(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    if (app->editing == APP_EDIT_GROUP_NAME)
    {
        if (event->key.key == SDLK_ESCAPE)
        {
            app->editing = -1;
            app->name_target_group = -1;
            SDL_StopTextInput(window);
            app->status = tr(TXT_APP_RENAME_CANCELED);
        }
        else if (event->key.key == SDLK_A && (event->key.mod & SDL_KMOD_CTRL))
            app->edit_select_all = 1;
        else if (event->key.key == SDLK_BACKSPACE)
        {
            size_t length = strlen(app->edit_text);
            if (app->edit_select_all)
                length = 0;
            else if (length)
            {
                --length;
                while (length && ((unsigned char)app->edit_text[length] & 0xc0) == 0x80)
                    --length;
            }
            app->edit_text[length] = 0;
            app->edit_select_all = 0;
        }
        else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
        {
            if (app_editor_set_name(app, app->edit_text))
            {
                app->editing = -1;
                app->name_target_group = -1;
                SDL_StopTextInput(window);
            }
        }
        return;
    }
    if (event->key.key == SDLK_ESCAPE)
    {
        app->editing = -1;
        SDL_StopTextInput(window);
        app->status = tr(TXT_APP_EDIT_CANCELED);
    }
    else if (event->key.key == SDLK_BACKSPACE)
    {
        size_t length = strlen(app->edit_text);
        if (length)
            app->edit_text[length - 1] = '\0';
    }
    else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
    {
        double value;
        Workbench before BENCH_AUTO_CLEAR = {0};
        if (!bench_snapshot_clone(&before, &app->bench))
        {
            app->status = tr(TXT_APP_OUT_OF_MEMORY);
            return;
        }
        if (app->editing == APP_EDIT_SCALE)
        {
            if (app->selected >= 0 && bench_parse_value(app->edit_text, &value) &&
                bench_set_scale(&app->bench, app->selected, (float)(value / 100)))
            {
                if (before.revision != app->bench.revision)
                    bench_history_record(app->history, &before);
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = tr(TXT_APP_SIZE_CHANGED);
            }
            else
                report(app, TXT_APP_SIZE_RANGE, PART_SCALE_MIN * 100.0, PART_SCALE_MAX * 100.0);
        }
        else if (app->editing == APP_EDIT_GRID)
        {
            if (bench_parse_value(app->edit_text, &value) && value >= GRID_SPACING_MIN &&
                value <= GRID_SPACING_MAX)
            {
                app->grid_spacing = value;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = tr(TXT_APP_GRID_SPACING_CHANGED);
            }
            else
                report(app, TXT_APP_GRID_SPACING_RANGE, GRID_SPACING_MIN, GRID_SPACING_MAX);
        }
        else if (app->editing >= WORKBENCH_MAX_COMPONENTS)
        {
            double step = app->transient_step, stop = app->transient_stop;
            int parsed = bench_parse_value(app->edit_text, &value);
            if (parsed && app->editing == APP_EDIT_STEP)
                step = value;
            else if (parsed)
                stop = value;
            if (parsed && step <= stop && stop / step <= TRACE_MAX_POINTS * (1.0 + 1e-12))
            {
                app->transient_step = step;
                app->transient_stop = stop;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = tr(TXT_APP_TIME_CHANGED);
            }
            else
                report(app, TXT_APP_TIME_RANGE, TRACE_MAX_POINTS);
        }
        else if (bench_parse_value(app->edit_text, &value) && bench_set_value(&app->bench, app->editing, value))
        {
            if (before.parts[app->editing].value != value)
                bench_history_record(app->history, &before);
            app->editing = -1;
            SDL_StopTextInput(window);
            app->status = tr(TXT_APP_VALUE_CHANGED);
        }
        else
            app->status = tr(TXT_APP_VALUE_INVALID);
    }
}

static void input_zoom(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    float x, y, direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        x = event->wheel.mouse_x * 1280.0f / w;
        y = event->wheel.mouse_y * 720.0f / h;
        if (x < 220 && y >= 140 && direction != 0)
        {
            int last = palette_count(app->palette_page) - 8;
            app->palette_offset += direction < 0 ? 1 : -1;
            if (app->palette_offset < 0)
                app->palette_offset = 0;
            if (app->palette_offset > last)
                app->palette_offset = last;
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0 && (SDL_GetModState() & SDL_KMOD_CTRL))
        {
            if (app->selected >= 0 && app->bench.parts[app->selected].group)
            {
                int group = app->bench.parts[app->selected].group;
                Workbench before BENCH_AUTO_CLEAR = {0};
                if (!bench_snapshot_clone(&before, &app->bench))
                {
                    app->status = tr(TXT_APP_OUT_OF_MEMORY);
                    return;
                }
                float width = bench_group_width(&app->bench, group);
                float height = bench_group_height(&app->bench, group);
                float factor = direction > 0 ? 1.1f : 1 / 1.1f;
                if (SDL_GetModState() & SDL_KMOD_SHIFT)
                    height = fminf(10000, fmaxf(44, height * factor));
                else
                    width = fminf(10000, fmaxf(64, width * factor));
                if (!bench_group_resize(&app->bench, group, width, height))
                    app->status = tr(TXT_APP_CHIP_SIZE_INVALID);
                else if (!bench_history_record(app->history, &before))
                {
                    bench_replace(&app->bench, &before);
                    app->status = tr(TXT_APP_HISTORY_FAILED);
                }
                else
                    app->status = tr(TXT_APP_CHIP_RESIZED);
                return;
            }
            if (app->selected < 0)
            {
                app->status = tr(TXT_APP_SELECT_PART_FOR_SIZE);
                return;
            }
            Workbench before BENCH_AUTO_CLEAR = {0};
            if (!bench_snapshot_clone(&before, &app->bench))
            {
                app->status = tr(TXT_APP_OUT_OF_MEMORY);
                return;
            }
            float scale = bench_part_scale(&app->bench.parts[app->selected]);
            scale *= direction > 0 ? 1.1f : 1 / 1.1f;
            scale = fminf(PART_SCALE_MAX, fmaxf(PART_SCALE_MIN, scale));
            if (bench_set_scale(&app->bench, app->selected, scale) && app->bench.revision != before.revision)
            {
                if (bench_history_record(app->history, &before))
                    app->status = tr(TXT_APP_SIZE_CHANGED);
                else
                {
                    bench_replace(&app->bench, &before);
                    app->status = tr(TXT_APP_HISTORY_FAILED);
                }
            }
            bench_snapshot_clear(&before);
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0 && (SDL_GetModState() & SDL_KMOD_SHIFT) &&
            app->selected >= 0 && (app->bench.parts[app->selected].type == PART_POTENTIOMETER ||
                                   component_environment(app->bench.parts[app->selected].type)))
        {
            Workbench before BENCH_AUTO_CLEAR = {0};
            if (!bench_snapshot_clone(&before, &app->bench))
            {
                app->status = tr(TXT_APP_OUT_OF_MEMORY);
                return;
            }
            double position = app->bench.parts[app->selected].control + (direction > 0 ? .05 : -.05);
            position = fmin(1, fmax(0, position));
            if (bench_set_control(&app->bench, app->selected, position) && app->bench.revision != before.revision)
                if (!bench_history_record(app->history, &before))
                {
                    bench_replace(&app->bench, &before);
                    app->status = tr(TXT_APP_HISTORY_FAILED);
                    return;
                }
            app->status = tr(TXT_APP_CONTROL_HINT);
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0)
            bench_view_zoom_at(&app->view, direction > 0 ? 1.2f : 1.0f / 1.2f, x, y);
    }
}

static void input_rotate(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    /* Right click rotates the part under the cursor, the same as selecting it and pressing R. */
    int w, h, hit;
    float x, y;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    x = event->button.x * 1280.0f / w;
    y = event->button.y * 720.0f / h;
    if (x < 220 || y < 70 || y > 660)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    hit = part_at(&app->bench, x, y, app->view.zoom);
    if (hit >= 0 && app->bench.parts[hit].type != PART_BREADBOARD)
    {
        SDL_Event shortcut = {0};
        app->selected = hit;
        app->selected_wire = -1;
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_R;
        SDL_PushEvent(&shortcut);
    }
}

static void input_pan_start(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0 && event->button.x * 1280.0f / w >= 220 && event->button.y * 720.0f / h >= 70 &&
        event->button.y * 720.0f / h < 660)
        app->panning = 1;
}

static void input_pan(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        app->view.pan_x += event->motion.xrel * 1280.0f / w;
        app->view.pan_y += event->motion.yrel * 720.0f / h;
    }
}

static void start_settings_edit(AppState *app, SDL_Window *window, int target, double value)
{
    if (!SDL_StartTextInput(window))
        return;
    app->editing = target;
    snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", value);
}

static void choose_wire_color(AppState *app, int chosen)
{
    app->wire_color = chosen;
    int selected = app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count;
    if (!selected || app->bench.wires[app->selected_wire].color == chosen)
    {
        app->status = tr(TXT_APP_NEW_WIRE_COLOR);
        return;
    }
    bench_history_record(app->history, &app->bench);
    bench_set_wire_color(&app->bench, app->selected_wire, chosen);
    app->status = tr(TXT_APP_WIRE_COLOR_CHANGED);
}

static void toggle_wire_current(AppState *app)
{
    app->show_current = !app->show_current;
    if (app->show_current && sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.trace.measures_wires)
        app->status = tr(TXT_APP_SELECT_WIRE_FOR_CURRENT);
}

static void cycle_probe_pin(AppState *app)
{
    const int quad_gate_pins = 14, default_pins = 3;
    int quad = app->selected >= 0 && component_has_flag(app->bench.parts[app->selected].type, COMPONENT_QUAD_GATE);
    app->probe_pin = (app->probe_pin + 1) % (quad ? quad_gate_pins : default_pins);
}

/* The whole interface switches at once; texts already on screen are rebuilt from their ids next frame. */
static void choose_language(AppState *app, UiLanguage language)
{
    ui_text_set_language(language);
    app_packing_refresh(app);
    app->status = tr(language == UI_LANG_PT_BR ? TXT_LANGUAGE_NAME_PT_BR : TXT_LANGUAGE_NAME_EN_US);
}

static void input_settings(AppState *app, SDL_Window *window, float x, float y)
{
    int swatch = app_layout_wire_swatch_at(x, y);
    if (swatch >= 0 && swatch < WIRE_COLOR_COUNT)
    {
        choose_wire_color(app, swatch);
        return;
    }
    switch (app_layout_setting_at(x, y))
    {
    case APP_SETTING_DARK_THEME:
        app->dark_theme = !app->dark_theme;
        break;
    case APP_SETTING_FONT_SMALLER:
        if (app->gui_font_size > APP_FONT_MIN)
            --app->gui_font_size;
        break;
    case APP_SETTING_FONT_LARGER:
        if (app->gui_font_size < APP_FONT_MAX)
            ++app->gui_font_size;
        break;
    case APP_SETTING_SHOW_GRID:
        app->show_grid = !app->show_grid;
        break;
    case APP_SETTING_ROUND_WIRES:
        app->round_wires = !app->round_wires;
        break;
    case APP_SETTING_HANDLES:
        app->show_handles = !app->show_handles;
        break;
    case APP_SETTING_PIN_LABELS:
        app->show_pin_labels = !app->show_pin_labels;
        break;
    case APP_SETTING_SNAP_GRID:
        app->snap_grid = !app->snap_grid;
        break;
    case APP_SETTING_GRID_SPACING:
        start_settings_edit(app, window, APP_EDIT_GRID, app->grid_spacing);
        break;
    case APP_SETTING_SNAP_BOARD:
        app->snap_board = !app->snap_board;
        break;
    case APP_SETTING_SHOW_CURRENT:
        toggle_wire_current(app);
        break;
    case APP_SETTING_PROBE_MEASURE:
        app->probe_current = !app->probe_current;
        break;
    case APP_SETTING_PROBE_PIN:
        cycle_probe_pin(app);
        break;
    case APP_SETTING_STEP:
        start_settings_edit(app, window, APP_EDIT_STEP, app->transient_step);
        break;
    case APP_SETTING_STOP:
        start_settings_edit(app, window, APP_EDIT_STOP, app->transient_stop);
        break;
    case APP_SETTING_LANGUAGE_PT_BR:
        choose_language(app, UI_LANG_PT_BR);
        break;
    case APP_SETTING_LANGUAGE_EN_US:
        choose_language(app, UI_LANG_EN_US);
        break;
    case APP_SETTING_RESET:
        app_settings_reset(app);
        app->status = tr(TXT_SETTINGS_RESTORED);
        break;
    case APP_SETTING_CLOSE:
        app->settings = 0;
        break;
    default:
        break;
    }
}

static int input_examples(AppState *app, float x, float y)
{
    if (!app->examples_open)
        return 0;
    int card = app_layout_example_card_at(x, y);
    if (card >= 0)
        app_commands_load_example(app, card);
    else if (app_rect_contains(app_layout_examples_close(), x, y))
        app->examples_open = 0;
    return 1;
}

static int input_projects(AppState *app, float x, float y)
{
    if (!app->projects_open)
        return 0;
    int slot = app_layout_project_slot_at(x, y);
    if (slot >= 0)
        app->save_slot = slot;
    else if (app_rect_contains(app_layout_project_save(), x, y))
        app_projects_save(app);
    else if (app_rect_contains(app_layout_project_load(), x, y))
        app_projects_load(app);
    else if (app_rect_contains(app_layout_project_close(), x, y))
        app->projects_open = 0;
    return 1;
}

static void input_environment_position(AppState *app, float x)
{
    if (app->dragging_environment < 0 || app->dragging_environment >= app->bench.part_count)
        return;
    bench_set_control(&app->bench, app->dragging_environment, app_layout_environment_position(x));
}

static void push_shortcut(SDL_Keycode key)
{
    SDL_Event shortcut = {0};
    shortcut.type = SDL_EVENT_KEY_DOWN;
    shortcut.key.key = key;
    SDL_PushEvent(&shortcut);
}

static int selected_plain_part(const AppState *app)
{
    return !app->settings && !app->projects_open && !app->examples_open && app->selected >= 0 && !app->bench.parts[app->selected].group;
}

static int input_click_environment(AppState *app, float x, float y)
{
    if (!selected_plain_part(app) || !component_environment(app->bench.parts[app->selected].type) ||
        !app_rect_contains(app_layout_environment_slider(), x, y))
        return 0;
    if (!bench_history_record(app->history, &app->bench))
    {
        app->status = tr(TXT_APP_HISTORY_FAILED);
        return 1;
    }
    app->dragging_environment = app->selected;
    input_environment_position(app, x);
    app->status = tr(TXT_APP_ENVIRONMENT_DRAG);
    return 1;
}

static int input_click_inspector(AppState *app, SDL_Window *window, float x, float y)
{
    if (!selected_plain_part(app))
        return 0;
    const Part *part = &app->bench.parts[app->selected];
    int swatch = part->type == PART_LED ? app_layout_led_swatch_at(x, y) : -1;
    if (swatch >= 0)
    {
        if (part->states != (unsigned)swatch && bench_history_record(app->history, &app->bench))
            bench_set_states(&app->bench, app->selected, (unsigned)swatch);
        app->status = tr(TXT_APP_LED_COLOR_CHANGED);
        return 1;
    }
    if (app_rect_contains(app_layout_inspector_scale(), x, y))
    {
        start_settings_edit(app, window, APP_EDIT_SCALE, bench_part_scale(part) * 100);
        return 1;
    }
    if (app_code_editable(app, app->selected) && app_rect_contains(app_layout_inspector_value(), x, y))
    {
        app_code_open(app, window, app->selected);
        return 1;
    }
    if (app_rect_contains(app_layout_inspector_value(), x, y))
    {
        push_shortcut(SDLK_E);
        return 1;
    }
    if (part->type != PART_BREADBOARD && app_rect_contains(app_layout_inspector_rotate(), x, y))
    {
        push_shortcut(SDLK_R);
        return 1;
    }
    return 0;
}

static int input_click_header(AppState *app, float x, float y)
{
    switch (app_layout_header_at(x, y))
    {
    case APP_HEADER_PROJECTS:
        app_editing_key(app, SDLK_F3, 0);
        return 1;
    case APP_HEADER_EXAMPLES:
        app->examples_open = !app->examples_open;
        app->settings = 0;
        app->projects_open = 0;
        return 1;
    case APP_HEADER_SETTINGS:
        app->settings = !app->settings;
        app->projects_open = 0;
        app->examples_open = 0;
        app->dragging = -1;
        app->drag_wire = -1;
        return 1;
    case APP_HEADER_PLAY:
        push_shortcut(SDLK_SPACE);
        return 1;
    default:
        return 0;
    }
}

static int input_click_trace(AppState *app, float x, float y)
{
    if (!sim_state_has_result(&app->sim, SIM_RESULT_TRACE) || !app->show_trace || x < 840 || x > 1240 || y < 384 ||
        y > 628)
        return 0;
    if (x >= 918 && x <= 1220 && y >= 431 && y <= 565)
    {
        app->sim.trace_index = spice_trace_nearest(&app->sim.trace, (x - 918) / 302.0);
        app->sim.sim_running = 0;
        app->status = sim_state_trace_text(&app->sim);
    }
    return 1;
}

/* With a single ordinary part selected, choosing from the catalog swaps it in place. */
static int input_swap_selected(AppState *app, PartType type)
{
    /* Same type: nothing to swap, so the click places a new one as usual. */
    if (!selected_plain_part(app) || app->bench.parts[app->selected].type == PART_BREADBOARD ||
        app->bench.parts[app->selected].type == type || bench_selection_count(&app->selection, &app->bench) > 1)
        return 0;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench) || !bench_swap_type(&app->bench, app->selected, type))
    {
        app->status = tr(TXT_APP_SWAP_FAILED);
        return 1;
    }
    bench_history_record(app->history, &before);
    app->placing = -1;
    app->status = tr(TXT_APP_PART_SWAPPED);
    return 1;
}

static int input_click_palette(AppState *app, float x, float y)
{
    int tab = app_layout_palette_tab_at(x, y);
    if (tab >= 0)
    {
        app->palette_page = tab;
        app->placing = -1;
        app->palette_offset = 0;
        app->packed_placing = 0;
        app_packing_refresh(app);
        return 1;
    }
    if (x >= APP_PALETTE_WIDTH || y < APP_HEADER_HEIGHT)
        return 0;
    int slot = app_layout_palette_item_at(x, y);
    if (slot < 0)
        return 1;
    if (app->palette_page == 2)
    {
        if (app->library_available[slot] && app_rect_contains(app_layout_palette_delete(slot), x, y))
        {
            app_packing_delete(app, slot);
            return 1;
        }
        app->library_delete_armed = -1;
        app_packing_choose(app, slot);
        return 1;
    }
    int item = slot + app->palette_offset;
    if (item >= palette_count(app->palette_page))
        return 1;
    app->packed_placing = 0;
    app->pending.component = -1;
    if (input_swap_selected(app, palette_part(app->palette_page, item)))
        return 1;
    app->placing = palette_part(app->palette_page, item);
    app->status = tr(TXT_APP_CLICK_TO_PLACE);
    return 1;
}

static int input_click_controls(AppState *app, SDL_Window *window, float x, float y)
{
    if (input_click_environment(app, x, y))
        return 1;
    if (app_editor_click(app, window, x, y))
        return 1;
    if (input_click_inspector(app, window, x, y))
        return 1;
    if (input_click_header(app, x, y) || input_projects(app, x, y) || input_examples(app, x, y))
        return 1;
    if (app->settings)
    {
        input_settings(app, window, x, y);
        return 1;
    }
    return input_click_trace(app, x, y) || input_click_palette(app, x, y);
}

static int input_click_knee(AppState *app, float x, float y, int clicks)
{
    int i;
    if (app->pending.component < 0 && app->placing < 0 && app->show_handles)
    {
        int k;
        for (i = 0; i < app->bench.wire_count && app->drag_wire < 0; ++i)
        {
            if (bench_group_wire_internal(&app->bench, &app->bench.wires[i]))
                continue;
            for (k = 0; k < app->bench.wires[i].knee_count; ++k)
            {
                WirePoint point = app->bench.wires[i].knees[k];
                if (fabsf(x - point.x) < 8 / app->view.zoom && fabsf(y - point.y) < 8 / app->view.zoom)
                {
                    app->drag_wire = i;
                    app->drag_knee = k;
                    break;
                }
            }
        }
        if (app->drag_wire >= 0)
        {
            if (clicks >= 2)
            {
                bench_history_record(app->history, &app->bench);
                bench_remove_knee(&app->bench, app->drag_wire, app->drag_knee);
                app->drag_wire = -1;
                app->status = tr(TXT_APP_BEND_REMOVED);
            }
            else
            {
                app->move_recorded = 0;
                app->status = tr(TXT_APP_DRAG_BEND);
            }
            return 1;
        }
    }
    return 0;
}

static int snap_suspended(void)
{
    return (SDL_GetModState() & SDL_KMOD_ALT) != 0;
}

static void input_snap_grid(const AppState *app, float *x, float *y)
{
    if (!app->snap_grid || snap_suspended())
        return;
    BenchGrid grid = bench_grid_default();
    grid.spacing = app->grid_spacing;
    double snapped_x, snapped_y;
    if (bench_grid_snap(&grid, *x, *y, &snapped_x, &snapped_y))
    {
        *x = (float)snapped_x;
        *y = (float)snapped_y;
    }
}

static void snap_part_terminals(const AppState *app, int component, float *x, float *y);

static void input_place_part(AppState *app, float x, float y)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return;
    }
    int added = bench_add(&app->bench, (PartType)app->placing, x, y);
    if (added >= 0)
    {
        int snapped = app->snap_board && !snap_suspended() && bench_snap_to_breadboard(&app->bench, added);
        if (!snapped)
        {
            input_snap_grid(app, &x, &y);
            snap_part_terminals(app, added, &x, &y);
            bench_move_part(&app->bench, added, x, y);
        }
        bench_history_record(app->history, &before);
        app->selected_wire = -1;
        app->status =
            snapped ? tr(TXT_APP_PART_SNAPPED) : tr(TXT_APP_PART_ADDED);
        app->placing = -1;
    }
    else
        report(app, TXT_APP_PART_LIMIT, WORKBENCH_MAX_COMPONENTS);
}

/* A terminal hidden under another part body cannot be hovered or clicked: what you see is what you hit.
 * X toggles transparent bodies (app->xray), which makes every terminal reachable.
 * Breadboards are drawn first, so any part body hides their holes; parts hide those drawn before them. */
static int terminal_covered(const Workbench *bench, int owner, float x, float y)
{
    const float edge_margin = 6;
    int owner_is_board = bench->parts[owner].type == PART_BREADBOARD;
    for (int i = bench->part_count - 1; i >= 0; --i)
    {
        const Part *part = &bench->parts[i];
        if (i == owner || part->group || part->type == PART_BREADBOARD || (!owner_is_board && i < owner))
            continue;
        WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
        float half_width, half_height;
        component_body_extent(part->type, &half_width, &half_height);
        if (fabsf(local.x) < half_width - edge_margin && fabsf(local.y) < half_height - edge_margin)
            return 1;
    }
    return 0;
}

static Pin input_pin_at(const Workbench *bench, float x, float y, float zoom, int see_through)
{
    for (int i = bench->part_count - 1; i >= 0; --i)
        if (bench->parts[i].group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p))
                {
                    float px, py;
                    bench_terminal_position(bench, (Pin){i, p}, &px, &py);
                    if ((x - px) * (x - px) + (y - py) * (y - py) <= 64)
                        return (Pin){i, p};
                }

    int i, hit = -1, hitpin = -1;
    int pass;
    for (pass = 0; pass < 2 && hit < 0; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            if (bench->parts[i].group)
                continue;
            int p, board = bench->parts[i].type == PART_BREADBOARD;
            float radius = 12.0f;
            if (board)
                radius = 4.0f;
            else if (component_has_flag(bench->parts[i].type, COMPONENT_QUAD_GATE))
                radius = 5.0f;
            radius = fmaxf(4, radius * fminf(1, bench_part_scale(&bench->parts[i]))) / zoom;
            if (board != pass)
                continue;
            for (p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
            {
                float px, py;
                bench_pin_position(&bench->parts[i], p, &px, &py);
                if ((x - px) * (x - px) + (y - py) * (y - py) <= radius * radius &&
                    (see_through || !terminal_covered(bench, i, px, py)))
                {
                    hit = i;
                    hitpin = p;
                    break;
                }
            }
            if (hit >= 0)
                break;
        }
    return (Pin){hit, hitpin};
}

static void input_connect_pin(AppState *app, Pin clicked)
{
    app->selected_wire = -1;
    if (app->pending.component < 0)
    {
        app->pending = clicked;
        app->route_count = 0;
        app->status = tr(TXT_APP_WIRE_STARTED);
    }
    else if (app->pending.component == clicked.component && app->pending.pin == clicked.pin)
    {
        app->pending.component = -1;
        app->route_count = 0;
        app->status = tr(TXT_APP_WIRE_CANCELED);
    }
    else
    {
        Workbench before BENCH_AUTO_CLEAR = {0};
        if (!bench_snapshot_clone(&before, &app->bench))
        {
            app->status = tr(TXT_APP_OUT_OF_MEMORY);
            return;
        }
        int result = bench_toggle_wire(&app->bench, app->pending, clicked);
        if (result == 1)
        {
            int wire = app->bench.wire_count - 1;
            bench_set_wire_route(&app->bench, wire, app->route, app->route_count);
            bench_set_wire_color(&app->bench, wire, app->wire_color);
        }
        if (result)
            bench_history_record(app->history, &before);
        if (result == 1)
            app->status = tr(TXT_APP_WIRE_CONNECTED);
        else if (result == 2)
            app->status = tr(TXT_APP_WIRE_DELETED);
        else
            app->status = tr(TXT_APP_CONNECT_FAILED);
        app->pending.component = -1;
        app->route_count = 0;
    }
}

/* Only the round cap presses a push button; the rest of the body selects and drags it. */
#define BUTTON_CAP_RADIUS 14.0f
static int button_cap_hit(const Part *part, float x, float y)
{
    WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
    return hypotf(local.x, local.y + 4) <= BUTTON_CAP_RADIUS;
}

/* Double click on a DIP lever toggles that contact; on an instrument it cycles the mode. */
static int input_toggle_device(AppState *app, int hit, float x, float y)
{
    const Part *part = &app->bench.parts[hit];
    const ComponentDevice *device = component_device(part->type);
    if (!device || (!device->switch_count && device->mode_count < 2))
        return 0;
    unsigned states = (part->states + 1) % (unsigned)(device->mode_count > 1 ? device->mode_count : 1);
    if (device->switch_count)
    {
        WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
        int contact = component_switch_at(part->type, local.x);
        if (contact < 0)
            return 0;
        states = part->states ^ (1u << contact);
    }
    if (!bench_history_record(app->history, &app->bench))
    {
        app->status = tr(TXT_APP_HISTORY_FAILED);
        return 1;
    }
    bench_set_states(&app->bench, hit, states);
    const char *mode = component_mode_label(part->type, states);
    app->status = mode ? mode : tr(TXT_APP_CONTACT_TOGGLED);
    return 1;
}

static void input_select(AppState *app, float x, float y, int clicks)
{
    int hit;
    hit = part_at(&app->bench, x, y, app->view.zoom);
    if (hit < 0 || !app->selection.parts[hit])
        bench_selection_clear(&app->selection);
    if (hit >= 0)
        app->selection.parts[hit] = 1;
    bench_selection_expand_groups(&app->selection, &app->bench);
    app->selected = hit;
    app->selected_wire = -1;
    if (hit < 0 || app->bench.parts[hit].type == PART_BREADBOARD)
    {
        app->selected_wire = hit_wire(&app->bench, x, y, 7.0f / app->view.zoom, app->round_wires);
        if (app->selected_wire >= 0)
        {
            app->selected = -1;
            app->wire_color = app->bench.wires[app->selected_wire].color;
            app->status = tr(TXT_APP_WIRE_SELECTED);
            if (clicks >= 2)
                input_insert_knee(app, x, y);
            return;
        }
    }
    if (hit >= 0)
    {
        if (app->bench.parts[hit].type == PART_BUTTON && button_cap_hit(&app->bench.parts[hit], x, y))
        {
            bench_set_on(&app->bench, hit, 1);
            app->held_button = hit;
            app->status = tr(TXT_APP_BUTTON_PRESSED);
            return;
        }
        if (clicks >= 2 && app_code_editable(app, hit))
        {
            app_code_open(app, NULL, hit);
            return;
        }
        if (!app->bench.parts[hit].group && clicks >= 2 && input_toggle_device(app, hit, x, y))
            return;
        if (!app->bench.parts[hit].group && clicks >= 2 &&
            (component_electrical_type(app->bench.parts[hit].type) == PART_SOURCE || app->bench.parts[hit].type == PART_SWITCH ||
             app->bench.parts[hit].type == PART_CLOCK || app->bench.parts[hit].type == PART_DPST ||
             app->bench.parts[hit].type == PART_FUNCTION_GENERATOR))
        {
            bench_history_record(app->history, &app->bench);
            bench_set_on(&app->bench, hit, !app->bench.parts[hit].on);
            app->status = tr(TXT_APP_STATE_CHANGED);
        }
        else
        {
            app->dragging = hit;
            app->move_recorded = 0;
            app->offset_x = x - app->bench.parts[hit].x;
            app->offset_y = y - app->bench.parts[hit].y;
            app->status = tr(TXT_APP_DRAG_PART);
        }
    }
    else
    {
        app->pending.component = -1;
        app->selecting_area = 1;
        app->area_start = (WirePoint){x, y};
        app->area_end = app->area_start;
        app->status = tr(TXT_APP_DRAG_AREA);
    }
}

static int input_click_lead(AppState *app, float x, float y, int clicks)
{
    float tolerance = 8 / app->view.zoom;
    for (int i = app->bench.part_count - 1; i >= 0; --i)
    {
        const Part *part = &app->bench.parts[i];
        if (part->group)
            continue;
        for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
        {
            size_t count = bench_lead_count(part, pin);
            for (size_t point = 0; point < count; ++point)
            {
                WirePoint p = bench_lead_point(part, pin, point);
                if (hypotf(x - p.x, y - p.y) > tolerance)
                    continue;
                app->selected = i;
                app->pending.component = -1;
                if (clicks >= 2 && point + 1 < count)
                {
                    Workbench before BENCH_AUTO_CLEAR = {0};
                    if (!bench_snapshot_clone(&before, &app->bench))
                    {
                        app->status = tr(TXT_APP_OUT_OF_MEMORY);
                        return 1;
                    }
                    if (!bench_lead_remove(&app->bench, i, pin, point) ||
                        !bench_history_record(app->history, &before))
                    {
                        bench_replace(&app->bench, &before);
                        app->status = tr(TXT_APP_LEG_BEND_REMOVE_FAILED);
                    }
                    else
                        app->status = tr(TXT_APP_LEG_BEND_REMOVED);
                }
                else
                {
                    app->drag_lead = (Pin){i, pin};
                    app->drag_lead_point = point;
                    app->move_recorded = 0;
                    app->status = tr(TXT_APP_DRAG_LEG);
                }
                return 1;
            }
            if (clicks < 2)
                continue;
            size_t slot;
            WirePoint projected;
            if (bench_lead_pick(part, pin, app->round_wires ? 18 : 0, (WirePoint){x, y}, tolerance, &slot, &projected))
            {
                Workbench before BENCH_AUTO_CLEAR = {0};
                if (!bench_snapshot_clone(&before, &app->bench))
                {
                    app->status = tr(TXT_APP_OUT_OF_MEMORY);
                    return 1;
                }
                if (!bench_lead_insert(&app->bench, i, pin, slot, projected) ||
                    !bench_history_record(app->history, &before))
                {
                    bench_replace(&app->bench, &before);
                    app->status = tr(TXT_APP_LEG_BEND_ADD_FAILED);
                }
                else
                    app->status = tr(TXT_APP_LEG_BEND_ADDED);
                app->selected = i;
                return 1;
            }
        }
    }
    return 0;
}

static void input_click(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int w, h, hitpin = -1;
    float x, y;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    x = event->button.x * 1280.0f / w;
    y = event->button.y * 720.0f / h;
    if (input_click_controls(app, window, x, y))
        return;
    if (x < 220 || y < 70 || y > 660)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    if (SDL_GetModState() & SDL_KMOD_SHIFT)
    {
        app->selecting_area = 1;
        app->area_start = (WirePoint){x, y};
        app->area_end = app->area_start;
        app->dragging = -1;
        app->drag_wire = -1;
        app->pending.component = -1;
        app->placing = -1;
        return;
    }
    if (SDL_GetModState() & SDL_KMOD_ALT)
    {
        if (input_click_lead(app, x, y, event->button.clicks))
            return;
        input_select(app, x, y, 1);
        return;
    }
    if (input_click_knee(app, x, y, event->button.clicks))
        return;
    if (app->packed_placing)
    {
        input_snap_grid(app, &x, &y);
        app_packing_place(app, x, y);
        if (!app->packed_placing && app->selected >= 0)
        {
            const Part *placed = &app->bench.parts[app->selected];
            float snapped_x = placed->x, snapped_y = placed->y;
            snap_part_terminals(app, app->selected, &snapped_x, &snapped_y);
            bench_selection_move(&app->bench, &app->selection, snapped_x - placed->x, snapped_y - placed->y);
        }
        return;
    }
    if (app->placing >= 0)
    {
        input_place_part(app, x, y);
        return;
    }
    Pin clicked = input_pin_at(&app->bench, x, y, app->view.zoom, app->xray);
    hitpin = clicked.pin;
    if (hitpin >= 0)
    {
        input_connect_pin(app, clicked);
        return;
    }
    if (app->pending.component >= 0)
    {
        if (app->route_count < WIRE_MAX_KNEES)
        {
            app->route[app->route_count++] = (WirePoint){x, y};
            app->status = tr(TXT_APP_ROUTE_BEND_ADDED);
        }
        else
            report(app, TXT_APP_BEND_LIMIT, WIRE_MAX_KNEES);
        return;
    }
    input_select(app, x, y, event->button.clicks);
}

static void input_release(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    app->dragging_environment = -1;
    (void)window;
    (void)event;

    if (app->held_button >= 0 && app->held_button < app->bench.part_count &&
        app->bench.parts[app->held_button].type == PART_BUTTON)
        bench_set_on(&app->bench, app->held_button, 0);
    app->held_button = -1;
    app->drag_lead.component = -1;
    app->selecting_area = 0;
    if (bench_selection_count(&app->selection, &app->bench) <= 1 && app->dragging >= 0 && app->move_recorded &&
        !app->bench.parts[app->dragging].group && app->snap_board && !snap_suspended() && bench_snap_to_breadboard(&app->bench, app->dragging))
        app->status = tr(TXT_APP_PART_SNAPPED);
    app->dragging = -1;
    app->drag_wire = -1;
}

static WirePoint snap_lead_tip(const AppState *app, int component, WirePoint point)
{
    float best = 9 / app->view.zoom;
    WirePoint result = point;
    for (int i = 0; i < app->bench.part_count; ++i)
    {
        if (i == component || app->bench.parts[i].group != app->bench.parts[component].group)
            continue;
        for (int pin = 0; pin < bench_pin_count(app->bench.parts[i].type); ++pin)
        {
            WirePoint target;
            bench_pin_position(&app->bench.parts[i], pin, &target.x, &target.y);
            float distance = hypotf(target.x - point.x, target.y - point.y);
            if (distance < best)
            {
                best = distance;
                result = target;
            }
        }
    }
    return result;
}

static void input_drag_lead(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    int component = app->drag_lead.component, pin = app->drag_lead.pin;
    if (component < 0 || component >= app->bench.part_count)
        return;
    WirePoint world = {event->motion.x * 1280.0f / width, event->motion.y * 720.0f / height};
    bench_view_to_world(&app->view, world.x, world.y, &world.x, &world.y);
    if (app->drag_lead_point + 1 == bench_lead_count(&app->bench.parts[component], pin))
        world = snap_lead_tip(app, component, world);
    if (!app->move_recorded)
    {
        if (!bench_history_record(app->history, &app->bench))
        {
            app->status = tr(TXT_APP_HISTORY_FAILED);
            return;
        }
        app->move_recorded = 1;
    }
    if (!bench_lead_move(&app->bench, component, pin, app->drag_lead_point, world))
        app->status = tr(TXT_APP_LEG_MOVE_FAILED);
}

static void input_drag_wire(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0 && app->drag_wire < app->bench.wire_count)
    {
        float x = event->motion.x * 1280.0f / w, y = event->motion.y * 720.0f / h;
        WirePoint *point = &app->bench.wires[app->drag_wire].knees[app->drag_knee];
        bench_view_to_world(&app->view, x, y, &x, &y);
        input_snap_grid(app, &x, &y);
        if (point->x != x || point->y != y)
        {
            if (!app->move_recorded)
            {
                bench_history_record(app->history, &app->bench);
                app->move_recorded = 1;
            }
            bench_move_knee(&app->bench, app->drag_wire, app->drag_knee, (WirePoint){x, y});
        }
    }
}

/* While a part is dragged, a terminal near another part's terminal pulls the part so both coincide
 * exactly; coincident terminals are an electrical contact (bench_find_contact), like a breadboard hole. */
#define TERMINAL_SNAP_PIXELS 10.0f
static void snap_part_terminals(const AppState *app, int component, float *x, float *y)
{
    const Workbench *bench = &app->bench;
    const Part *moving = &bench->parts[component];
    float dx = *x - moving->x, dy = *y - moving->y;
    float best = TERMINAL_SNAP_PIXELS / app->view.zoom, shift_x = 0, shift_y = 0;
    for (int member = 0; member < bench->part_count; ++member)
    {
        const Part *candidate = &bench->parts[member];
        if (moving->group ? candidate->group != moving->group : member != component)
            continue;
        for (int own = 0; own < bench_pin_count(candidate->type); ++own)
        {
            if (candidate->group && !bench_pin_exposed(candidate, own))
                continue;
            float px, py;
            bench_terminal_position(bench, (Pin){member, own}, &px, &py);
            px += dx;
            py += dy;
            for (int other = 0; other < bench->part_count; ++other)
            {
                const Part *target = &bench->parts[other];
                if (other == component || (moving->group && target->group == moving->group) ||
                    (target->type == PART_BREADBOARD && !app->snap_board))
                    continue;
                for (int pin = 0; pin < bench_pin_count(target->type); ++pin)
                {
                    if (target->group && !bench_pin_exposed(target, pin))
                        continue;
                    float tx, ty;
                    bench_terminal_position(bench, (Pin){other, pin}, &tx, &ty);
                    float distance = hypotf(tx - px, ty - py);
                    if (distance < best)
                    {
                        best = distance;
                        shift_x = tx - px;
                        shift_y = ty - py;
                    }
                }
            }
        }
    }
    *x += shift_x;
    *y += shift_y;
}

static void input_drag_part(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        float x = event->motion.x * 1280.0f / w, y = event->motion.y * 720.0f / h;
        bench_view_to_world(&app->view, x, y, &x, &y);
        x -= app->offset_x;
        y -= app->offset_y;
        const Part *dragged = &app->bench.parts[app->dragging];
        if (dragged->group)
        {
            const BenchGroup *group = &app->bench.groups[dragged->group - 1];
            float body_x = group->x + x - dragged->x;
            float body_y = group->y + y - dragged->y;
            input_snap_grid(app, &body_x, &body_y);
            x = dragged->x + body_x - group->x;
            y = dragged->y + body_y - group->y;
            snap_part_terminals(app, app->dragging, &x, &y);
        }
        else
        {
            Part candidate = *dragged;
            candidate.x = x;
            candidate.y = y;
            int board_snap = app->snap_board && !snap_suspended() &&
                             bench_selection_count(&app->selection, &app->bench) <= 1 &&
                             bench_breadboard_snap_position(&app->bench, &candidate, &x, &y);
            if (!board_snap)
                input_snap_grid(app, &x, &y);
            snap_part_terminals(app, app->dragging, &x, &y);
        }
        if (app->bench.parts[app->dragging].x != x || app->bench.parts[app->dragging].y != y)
        {
            if (!app->move_recorded)
            {
                bench_history_record(app->history, &app->bench);
                app->move_recorded = 1;
            }
            if (bench_selection_count(&app->selection, &app->bench) > 1 || app->bench.parts[app->dragging].group)
                bench_selection_move(&app->bench, &app->selection, x - app->bench.parts[app->dragging].x,
                                     y - app->bench.parts[app->dragging].y);
            else
                bench_move_part(&app->bench, app->dragging, x, y);
        }
    }
}

static void input_mouse_position(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    float x = event->motion.x * 1280.0f / width, y = event->motion.y * 720.0f / height;
    bench_view_to_world(&app->view, x, y, &app->mouse_world.x, &app->mouse_world.y);
    app->mouse_screen = (WirePoint){x, y};
    app->mouse_position_valid = 1;
    if (!app->selecting_area)
        return;
    app->area_end = app->mouse_world;
    bench_selection_rect(&app->selection, &app->bench, app->area_start, app->area_end);
    app->selected = -1;
    for (int i = 0; i < app->bench.part_count; ++i)
        if (app->selection.parts[i])
            app->selected = i;
    report(app, TXT_APP_PARTS_SELECTED, bench_selection_count(&app->selection, &app->bench));
}

/* Coalesce hover hit testing to one pass per frame; drag/model events still all execute. */
static void input_refresh_hover(AppState *app)
{
    app->hovered_pin = (Pin){-1, -1};
    app->hover_revision = app->bench.revision;
    float x = app->mouse_screen.x, y = app->mouse_screen.y;
    if (x < 220 || y < 70 || y >= 660 || app->settings || app->projects_open || app->examples_open || app->editing >= 0 ||
        app->selecting_area || app->editor_confirm_close || app->panning || app->dragging >= 0 || app->drag_wire >= 0)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    app->hovered_pin = input_pin_at(&app->bench, x, y, app->view.zoom, app->xray);
}

/* Dispatch in priority order: text editing consumes input before workspace interaction. */
void app_input_poll(AppState *app, SDL_Window *window)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        /* Finish draining this context before routing to another editor. */
        if (app->editor_open_requested || app->editor_close_requested)
            continue;
        if (e.type == SDL_EVENT_MOUSE_MOTION)
            input_mouse_position(app, window, &e);
        if (e.type != SDL_EVENT_QUIT && app->editing < 0 && app_code_event(app, window, &e))
            continue;
        if (e.type == SDL_EVENT_QUIT)
            app->running = 0;
        else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            input_release(app, window, &e);
        else if (app->editing >= 0 && e.type == SDL_EVENT_TEXT_INPUT)
        {
            input_text(app, window, &e);
        }
        else if (app->editing >= 0 && e.type == SDL_EVENT_KEY_DOWN)
        {
            input_edit_key(app, window, &e);
        }
        else if (app->editing >= 0 && (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION))
        {
            continue;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
        {
            app_commands_key(app, window, e.key.key, e.key.mod);
        }
        else if ((app->projects_open || app->examples_open) && e.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
            continue;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->selecting_area)
            continue;
        else if (e.type == SDL_EVENT_MOUSE_WHEEL && !app->settings && !app->editor_confirm_close && app->editing < 0)
        {
            input_zoom(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && !app->settings && !app->editor_confirm_close &&
                 app->editing < 0)
        {
            input_rotate(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE && !app->settings && !app->editor_confirm_close &&
                 app->editing < 0)
        {
            input_pan_start(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_MIDDLE)
            app->panning = 0;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->panning && app->editing < 0)
        {
            input_pan(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            input_click(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
        {
            input_release(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->dragging_environment >= 0)
        {
            input_environment_position(app, app->mouse_screen.x);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->drag_lead.component >= 0)
        {
            input_drag_lead(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->drag_wire >= 0)
        {
            input_drag_wire(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->dragging >= 0)
        {
            input_drag_part(app, window, &e);
        }
    }
    app_code_validate(app, window);
    if (app->mouse_position_valid)
        input_refresh_hover(app);
}
