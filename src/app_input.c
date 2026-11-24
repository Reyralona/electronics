#include "bench_scope.h"
#include "bench_path.h"
#include "component_registry.h"
#include "chip_docs.h"
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
#include "app_controls.h"
#include "code_text.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_trace.h"
#include <math.h>
_Static_assert(APP_EXAMPLE_CARDS == WORKBENCH_EXAMPLE_COUNT, "one examples card per example");
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void snap_part_terminals(const AppState *app, int component, float *x, float *y);

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
    if (!app_history_record(app, &before))
        return;
    app->show_handles = 1;
    app->status = tr(TXT_APP_BEND_ADDED);
}

int app_input_part_at(const Workbench *bench, float x, float y, float zoom)
{
    for (int g = WORKBENCH_MAX_COMPONENTS - 1; g >= 0; --g)
        if (bench->groups[g].active && !bench->groups[g].parent)
        {
            WirePoint local = bench_group_to_local(&bench->groups[g], (WirePoint){x, y});
            if (fabsf(local.x) < bench_group_width(bench, g + 1) * .5f + 11 &&
                fabsf(local.y) < bench_group_height(bench, g + 1) * .5f)
                return bench_group_first(bench, g + 1);
        }

    int pass, i;
    for (pass = 0; pass < 2; ++pass)
    {
        int closest = -1;
        float closest_distance = INFINITY;
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            if (part->group)
                continue;
            int board = bench_is_board(part->type);
            if (board != pass)
                continue;
            WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
            float half_width, half_height;
            component_body_extent(part->type, &half_width, &half_height);
            if (part->type == PART_LED)
                local.y += 6;
            float minimum_hit = 6 / (zoom * bench_part_scale(part));
            half_width = fmaxf(half_width, minimum_hit);
            half_height = fmaxf(half_height, minimum_hit);
            if (fabsf(local.x) < half_width && fabsf(local.y) < half_height)
            {
                float nx = local.x / half_width;
                float ny = local.y / half_height;
                float distance = nx * nx + ny * ny;
                if (distance < closest_distance)
                {
                    closest_distance = distance;
                    closest = i;
                }
            }
        }
        if (closest >= 0)
            return closest;
    }
    return -1;
}

static int editing_name(const AppState *app)
{
    return app->editing == APP_EDIT_NOTE || app->editing == APP_EDIT_GROUP_NAME || app->editing == APP_EDIT_PORT_NAME;
}

/* Double-click on a chip terminal: type its name (Enter confirms, empty removes, Esc cancels). */
static void start_port_name_edit(AppState *app, SDL_Window *window, Pin pin)
{
    if (!SDL_StartTextInput(window))
    {
        app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
        return;
    }
    app->pending.component = -1;
    app->route_count = 0;
    app->name_target_pin = pin;
    app->editing = APP_EDIT_PORT_NAME;
    app->edit_error = 0;
    app->edit_select_all = 1;
    bench_port_name(&app->bench.parts[pin.component], pin.pin, app->edit_text, sizeof(app->edit_text));
    app->status = tr(TXT_APP_TYPE_TERMINAL_NAME);
}

static int commit_port_name(AppState *app)
{
    const char *name = app->edit_text + strspn(app->edit_text, " ");
    if (name[0] && !bench_port_name_valid(name))
    {
        report(app, TXT_APP_TERMINAL_NAME_INVALID, BENCH_PORT_NAME_CAPACITY - 1);
        return 0;
    }
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 0;
    }
    if (!bench_set_port_name(&app->bench, app->name_target_pin, name))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 0;
    }
    if (app->bench.revision != before.revision)
        if (!app_history_record(app, &before))
            return 0;
    app->status = tr(name[0] ? TXT_APP_TERMINAL_NAMED : TXT_APP_TERMINAL_NAME_REMOVED);
    return 1;
}

static int commit_name(AppState *app)
{
    if (app->editing == APP_EDIT_NOTE)
    {
        if (!bench_history_record(app->history, &app->bench))
        {
            app->status = tr(TXT_APP_HISTORY_FAILED);
            return 0;
        }
        return bench_set_code(&app->bench, app->selected, app->edit_text);
    }
    if (app->editing == APP_EDIT_PORT_NAME)
        return commit_port_name(app);
    return app_editor_set_name(app, app->edit_text);
}

static void input_text(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    app->edit_error = 0;
    (void)window;

    if (editing_name(app))
    {
        const char *input = event->text.text;
        size_t before = app->edit_select_all ? 0 : strlen(app->edit_text);
        if ((app->editing != APP_EDIT_NOTE && !bench_group_name_valid(input) && strspn(input, " ") != strlen(input)) ||
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
    size_t length = app->edit_select_all ? 0 : strlen(app->edit_text);
    app->edit_select_all = 0;
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
    if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
        app->edit_error = 1;

    if (editing_name(app))
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
            if (commit_name(app))
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
    else if (event->key.key == SDLK_A && (event->key.mod & SDL_KMOD_CTRL))
        app->edit_select_all = 1;
    else if (event->key.key == SDLK_BACKSPACE)
    {
        size_t length = app->edit_select_all ? 0 : strlen(app->edit_text);
        if (app->edit_select_all)
            app->edit_text[0] = 0;
        app->edit_select_all = 0;
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
            int valid = app_selection_same_type(app) && bench_parse_value(app->edit_text, &value) &&
                        value >= PART_SCALE_MIN * 100 && value <= PART_SCALE_MAX * 100;
            if (valid && app_selection_set_scale(app, (float)(value / 100)))
            {
                if (before.revision != app->bench.revision)
                    if (!app_history_record(app, &before))
                        return;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = tr(TXT_APP_SIZE_CHANGED);
            }
            else if (valid)
            {
                bench_replace(&app->bench, &before);
                app->status = tr(TXT_APP_OUT_OF_MEMORY);
            }
            else
                report(app, TXT_APP_SIZE_RANGE, PART_SCALE_MIN * 100.0, PART_SCALE_MAX * 100.0);
        }
        else if (app->editing == APP_EDIT_GROUP_SCALE)
        {
            int group = app->selected >= 0 ? app->bench.parts[app->selected].group : 0;
            int valid = group && bench_parse_value(app->edit_text, &value) &&
                        value >= 20 * 100.0 / 98 && value <= 10000 * 100.0 / 98;
            if (valid)
            {
                float old_width = bench_group_width(&app->bench, group);
                float width = (float)(98 * value / 100);
                float height = bench_group_height(&app->bench, group) * width / old_width;
                height = fmaxf(28, height);
                if (app->snap_board && width != old_width)
                    width = fmaxf(20, roundf((width + 22) / BENCH_DIP_PITCH) * BENCH_DIP_PITCH - 22);
                valid = bench_group_resize(&app->bench, group, width, height);
                if (valid && app->snap_board && !app->bench.groups[group - 1].rotation_diagonal)
                {
                    float x = app->bench.parts[app->selected].x;
                    float y = app->bench.parts[app->selected].y;
                    snap_part_terminals(app, app->selected, &x, &y);
                    BenchSelection selection = {0};
                    selection.parts[app->selected] = 1;
                    bench_selection_move(&app->bench, &selection,
                                         x - app->bench.parts[app->selected].x,
                                         y - app->bench.parts[app->selected].y);
                }
            }
            if (valid)
            {
                if (before.revision != app->bench.revision && !app_history_record(app, &before))
                    return;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = tr(TXT_APP_CHIP_RESIZED);
            }
            else
                app->status = tr(TXT_APP_CHIP_SIZE_INVALID);
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
        else if (bench_parse_value(app->edit_text, &value) && app_selection_set_value(app, app->editing, value))
        {
            if (before.revision != app->bench.revision)
                if (!app_history_record(app, &before))
                    return;
            app->editing = -1;
            SDL_StopTextInput(window);
            app->status = tr(TXT_APP_VALUE_CHANGED);
        }
        else
            app->status = tr(TXT_APP_VALUE_INVALID);
    }
}

/* Shift+wheel over a bench supply turns the knob under the cursor: voltage in 0.1 V steps,
 * current limit in 10 mA steps. Returns 0 when the cursor is not on a supply. */
static int input_turn_supply_knob(AppState *app, float x, float y, float direction)
{
    int hit = app_input_part_at(&app->bench, x, y, app->view.zoom);
    if (hit < 0 || app->bench.parts[hit].type != PART_SOURCE || app->bench.parts[hit].group)
        return 0;
    const Part *part = &app->bench.parts[hit];
    WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
    int current = local.x >= (SOURCE_VOLTAGE_KNOB_X + SOURCE_CURRENT_KNOB_X) / 2;
    double sign = direction > 0 ? 1 : -1;
    if (!bench_history_record(app->history, &app->bench))
    {
        app->status = tr(TXT_APP_HISTORY_FAILED);
        return 1;
    }
    app->selected = hit;
    if (current)
    {
        double amps = component_source_limit(part) + sign * SOURCE_CURRENT_STEP;
        bench_set_control(&app->bench, hit, fmin(1, fmax(0, amps / SOURCE_CURRENT_LIMIT_MAX)));
    }
    else
    {
        /* The voltage parameter excludes exactly 0; the smallest value still reads 0.0 V. */
        double volts = fmin(SOURCE_VOLTAGE_MAX, fmax(1e-12, part->value + sign * SOURCE_VOLTAGE_STEP));
        bench_set_value(&app->bench, hit, volts);
    }
    app->status = tr(TXT_APP_VALUE_CHANGED);
    return 1;
}

static void input_zoom(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    float x, y, direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        WirePoint point = app_layout_to_ui(w, h, event->wheel.mouse_x, event->wheel.mouse_y);
        x = point.x;
        y = point.y;
        if (x < 220 && y >= 140 && direction != 0)
        {
            int last = palette_filtered_count(app->palette_page, app->palette_search) - APP_PALETTE_VISIBLE;
            if (last < 0)
                last = 0;
            app->palette_offset += direction < 0 ? 1 : -1;
            if (app->palette_offset < 0)
                app->palette_offset = 0;
            if (app->palette_offset > last)
                app->palette_offset = last;
            return;
        }
        if (x >= BENCH_VIEW_LEFT && y >= BENCH_VIEW_TOP && y < BENCH_VIEW_BOTTOM && direction != 0 && (SDL_GetModState() & SDL_KMOD_CTRL))
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
                SDL_Keymod modifiers = SDL_GetModState();
                if (modifiers & SDL_KMOD_SHIFT)
                    height = fminf(10000, fmaxf(28, height * factor));
                else if (modifiers & SDL_KMOD_ALT)
                    width = fminf(10000, fmaxf(20, width * factor));
                else
                {
                    width = fminf(10000, fmaxf(20, width * factor));
                    height = fminf(10000, fmaxf(28, height * factor));
                }
                if (app->snap_board && width != bench_group_width(&app->bench, group))
                    width = fmaxf(20, roundf((width + 22) / BENCH_DIP_PITCH) * BENCH_DIP_PITCH - 22);
                if (!bench_group_resize(&app->bench, group, width, height))
                    app->status = tr(TXT_APP_CHIP_SIZE_INVALID);
                else if (app->snap_board && !app->bench.groups[group - 1].rotation_diagonal)
                {
                    float x = app->bench.parts[app->selected].x;
                    float y = app->bench.parts[app->selected].y;
                    snap_part_terminals(app, app->selected, &x, &y);
                    BenchSelection selection = {0};
                    selection.parts[app->selected] = 1;
                    bench_selection_move(&app->bench, &selection,
                                         x - app->bench.parts[app->selected].x,
                                         y - app->bench.parts[app->selected].y);
                    if (!bench_history_record(app->history, &before))
                    {
                        bench_replace(&app->bench, &before);
                        app->status = tr(TXT_APP_HISTORY_FAILED);
                    }
                    else
                        app->status = tr(TXT_APP_CHIP_RESIZED);
                }
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
                if (app->snap_board && !bench_is_board(app->bench.parts[app->selected].type))
                    bench_snap_to_breadboard(&app->bench, app->selected);
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
        if (x >= BENCH_VIEW_LEFT && y >= BENCH_VIEW_TOP && y < BENCH_VIEW_BOTTOM && direction != 0 &&
            (SDL_GetModState() & SDL_KMOD_SHIFT) && input_turn_supply_knob(app, x, y, direction))
            return;
        if (x >= BENCH_VIEW_LEFT && y >= BENCH_VIEW_TOP && y < BENCH_VIEW_BOTTOM && direction != 0 && (SDL_GetModState() & SDL_KMOD_SHIFT) &&
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
        if (x >= BENCH_VIEW_LEFT && y >= BENCH_VIEW_TOP && y < BENCH_VIEW_BOTTOM && direction != 0)
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
    WirePoint point = app_layout_to_ui(w, h, event->button.x, event->button.y);
    x = point.x;
    y = point.y;
    if (x < BENCH_VIEW_LEFT || y < BENCH_VIEW_TOP || y > BENCH_VIEW_BOTTOM)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    hit = app_input_part_at(&app->bench, x, y, app->view.zoom);
    if (hit >= 0 && !bench_is_board(app->bench.parts[hit].type))
    {
        SDL_Event shortcut = {0};
        app->selected = hit;
        app->selected_wire = -1;
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_R;
        shortcut.key.mod = SDL_GetModState();
        SDL_PushEvent(&shortcut);
    }
}

static void input_pan_start(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    WirePoint point = app_layout_to_ui(w, h, event->button.x, event->button.y);
    if (w > 0 && h > 0 && point.x >= BENCH_VIEW_LEFT && point.y >= BENCH_VIEW_TOP && point.y < BENCH_VIEW_BOTTOM)
        app->panning = 1;
}

static void input_pan(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        AppRect viewport = app_layout_viewport(w, h);
        app->view.pan_x += event->motion.xrel * app_layout_canvas_width() / viewport.w;
        app->view.pan_y += event->motion.yrel * app_layout_canvas_height() / viewport.h;
    }
}

static void start_settings_edit(AppState *app, SDL_Window *window, int target, double value)
{
    if (!SDL_StartTextInput(window))
        return;
    app->editing = target;
    app->edit_error = 0;
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
    if (!app_history_record(app, &app->bench))
        return;
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
    int pins = app->selected >= 0 ? bench_pin_count(app->bench.parts[app->selected].type) : 2;
    app->probe_pin = pins > 0 ? (app->probe_pin + 1) % pins : 0;
}

/* The whole interface switches at once; texts already on screen are rebuilt from their ids next frame. */
static void choose_language(AppState *app, UiLanguage language)
{
    ui_text_set_language(language);
    app_packing_refresh(app);
    app->status = tr(language == UI_LANG_PT_BR ? TXT_LANGUAGE_NAME_PT_BR : TXT_LANGUAGE_NAME_EN_US);
}

/* Window units per canvas unit at the chosen interface scale (15 = 100%) and the display's own scale. */
float app_input_window_scale(const AppState *app, SDL_Window *window)
{
    float display = SDL_GetWindowDisplayScale(window), density = SDL_GetWindowPixelDensity(window);
    float content = display > 0 && density > 0 ? display / density : 1;
    return content * (float)app->gui_font_size / 15;
}

/* A maximized or full-screen window keeps its size: the canvas grows or shrinks inside it. A normal window
 * is resized to the design canvas at that scale. A scale whose design canvas does not fit is refused. */
int app_input_apply_scale(AppState *app, SDL_Window *window)
{
    float scale = app_input_window_scale(app, window);
    int width = (int)ceil(APP_DESIGN_WIDTH * scale);
    int height = (int)ceil(APP_DESIGN_HEIGHT * scale);
    if (SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))
    {
        int current_width, current_height;
        if (!SDL_GetWindowSize(window, &current_width, &current_height))
        {
            app->status = tr(TXT_UI_WINDOW_SIZE_FAILED);
            return 0;
        }
        if (width > current_width || height > current_height)
        {
            app->status = tr(TXT_UI_SCALE_UNAVAILABLE);
            return 0;
        }
        return 1;
    }
    SDL_Rect bounds;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds))
    {
        app->status = tr(TXT_UI_WINDOW_SIZE_FAILED);
        return 0;
    }
    if (width > bounds.w || height > bounds.h - 40)
    {
        app->status = tr(TXT_UI_SCALE_UNAVAILABLE);
        return 0;
    }
    if (!SDL_SetWindowMinimumSize(window, 1024, 576) || !SDL_SetWindowSize(window, width, height))
    {
        app->status = tr(TXT_UI_WINDOW_SIZE_FAILED);
        return 0;
    }
    return 1;
}

static void input_settings(AppState *app, SDL_Window *window, float x, float y)
{
    int previous_scale = app->gui_font_size;
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
    case APP_SETTING_CONNECTION_PINS:
        app->show_connection_pins = !app->show_connection_pins;
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
    case APP_SETTING_SIM_SPEED_SLOWER:
    case APP_SETTING_SIM_SPEED_FASTER:
    {
        static const double speeds[] = {WORKBENCH_SPEED_MIN, .002, .005, .01, .02, .05, .1, .25, .5, WORKBENCH_SPEED_MAX};
        int index = 0;
        while (index + 1 < (int)(sizeof(speeds) / sizeof(speeds[0])) &&
               speeds[index] < app->simulation_speed - 1e-9)
            ++index;
        if (app_layout_setting_at(x, y) == APP_SETTING_SIM_SPEED_SLOWER && index > 0)
            --index;
        if (app_layout_setting_at(x, y) == APP_SETTING_SIM_SPEED_FASTER &&
            index + 1 < (int)(sizeof(speeds) / sizeof(speeds[0])))
            ++index;
        app->simulation_speed = speeds[index];
        app->live_last_scheduled = SDL_GetTicks();
        break;
    }
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
    if (app->gui_font_size != previous_scale && !app_input_apply_scale(app, window))
        app->gui_font_size = previous_scale;
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
    {
        int selected = app->project_page * PROJECT_SLOTS_PER_PAGE + slot;
        if (selected < app->project_slot_count)
            app->save_slot = selected;
    }
    else if (app_rect_contains(app_layout_project_previous(), x, y) && app->project_page > 0)
    {
        app->project_page--;
        app->save_slot = app->project_page * PROJECT_SLOTS_PER_PAGE;
    }
    else if (app_rect_contains(app_layout_project_next(), x, y) &&
             (app->project_page + 1) * PROJECT_SLOTS_PER_PAGE < app->project_slot_count)
    {
        app->project_page++;
        app->save_slot = app->project_page * PROJECT_SLOTS_PER_PAGE;
    }
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

/* Buttons run the same command as their key, directly: a key event pushed into the SDL queue was
   not delivered, so clicking the Value field stopped opening the editor (reported 18/11/2026). */
static void run_shortcut(AppState *app, SDL_Window *window, SDL_Keycode key)
{
    app_commands_key(app, window, key, SDL_KMOD_NONE);
}

static int selected_plain_part(const AppState *app)
{
    return !app->settings && !app->projects_open && !app->examples_open && app->selected >= 0 && !app->bench.parts[app->selected].group;
}

static int panels_closed(const AppState *app)
{
    return !app->settings && !app->projects_open && !app->examples_open && app->code_part < 0 &&
           !app->editor_confirm_close;
}

static int wire_inspector_visible(const AppState *app)
{
    return panels_closed(app) && app->selected < 0 && app->selected_wire >= 0 &&
           app->selected_wire < app->bench.wire_count;
}

static int inspector_visible(const AppState *app)
{
    return panels_closed(app) && (app->selected >= 0 || wire_inspector_visible(app));
}

/* Wire panel: a swatch recolors the selected wire; other clicks on the panel stay on it. */
static int input_click_wire_inspector(AppState *app, float x, float y)
{
    if (!wire_inspector_visible(app) || app->inspector_minimized)
        return 0;
    int swatch = app_layout_color_swatch_at(x, y);
    if (swatch >= 0)
    {
        choose_wire_color(app, swatch);
        return 1;
    }
    return app_rect_contains(app_layout_wire_inspector(), x, y);
}

/* Minimize collapses the inspector to a tab; clicking the tab expands it. */
static int input_click_inspector_toggle(AppState *app, float x, float y)
{
    if (!inspector_visible(app))
        return 0;
    AppRect target = app->inspector_minimized ? app_layout_inspector_tab() : app_layout_inspector_minimize();
    if (!app_rect_contains(target, x, y))
        return 0;
    app->inspector_minimized = !app->inspector_minimized;
    app->status = tr(app->inspector_minimized ? TXT_APP_INSPECTOR_MINIMIZED : TXT_APP_INSPECTOR_EXPANDED);
    return 1;
}

static int input_click_environment(AppState *app, float x, float y)
{
    if (app->inspector_minimized || !selected_plain_part(app) || !component_environment(app->bench.parts[app->selected].type) ||
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

static int note_resize_hit(const Part *part, float x, float y, float zoom)
{
    float scale = bench_part_scale(part);
    return part->type == PART_NOTE &&
        fabsf(x - (part->x + 120 * scale - 6)) < 10 / zoom &&
        fabsf(y - (part->y + 55 * scale - 6)) < 10 / zoom;
}

static void start_note_edit(AppState *app, SDL_Window *window)
{
    if (!window)
        window = SDL_GetKeyboardFocus();
    if (!SDL_StartTextInput(window))
    {
        app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
        return;
    }
    app->editing = APP_EDIT_NOTE;
    app->edit_select_all = 1;
    app->edit_error = 0;
    snprintf(app->edit_text, sizeof(app->edit_text), "%s", app->bench.parts[app->selected].code ? app->bench.parts[app->selected].code : "");
}

static int input_click_inspector(AppState *app, SDL_Window *window, float x, float y)
{
    if (app->inspector_minimized || !selected_plain_part(app))
        return 0;
    const Part *part = &app->bench.parts[app->selected];
    if (part->type == PART_NOTE)
    {
        if (app_rect_contains(app_layout_inspector_value(), x, y))
        {
            start_note_edit(app, window);
            return 1;
        }
        for (int row = 0; row < 2; ++row)
        {
            int tone = app_layout_color_swatch_at(x, y - row * 48);
            if (tone < 0)
                continue;
            if (!bench_history_record(app->history, &app->bench))
            {
                app->status = tr(TXT_APP_HISTORY_FAILED);
                return 1;
            }
            unsigned shift = row ? 8 : 0;
            unsigned encoded = (unsigned)(tone + BENCH_COLOR_COUNT - (row ? BENCH_COLOR_BLACK : BENCH_COLOR_YELLOW)) % BENCH_COLOR_COUNT;
            bench_set_states(&app->bench, app->selected, (part->states & ~(255u << shift)) | (encoded << shift));
            return 1;
        }
    }
    int swatch = part->type == PART_LED ? app_layout_color_swatch_at(x, y) : -1;
    if (swatch >= COMPONENT_LED_COLORS)
        swatch = -1;
    if (swatch >= 0)
    {
        if (!app_selection_same_type(app))
            return 1;
        if (!bench_history_record(app->history, &app->bench))
        {
            app->status = tr(TXT_APP_HISTORY_FAILED);
            return 1;
        }
        for (int i = 0; i < app->bench.part_count; ++i)
            if (i == app->selected || app->selection.parts[i])
                bench_set_states(&app->bench, i, (unsigned)swatch);
        app->status = tr(TXT_APP_LED_COLOR_CHANGED);
        return 1;
    }
    if (app_rect_contains(app_layout_inspector_scale(), x, y))
    {
        if (app_selection_same_type(app))
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
        run_shortcut(app, window, SDLK_E);
        return 1;
    }
    if (!bench_is_board(part->type) && app_rect_contains(app_layout_inspector_rotate(), x, y))
    {
        run_shortcut(app, window, SDLK_R);
        return 1;
    }
    if (!bench_is_board(part->type) && app_rect_contains(app_layout_inspector_mirror(), x, y))
    {
        run_shortcut(app, window, SDLK_M);
        return 1;
    }
    if (chip_doc(part->type) && app_rect_contains(app_layout_inspector_doc(), x, y))
    {
        run_shortcut(app, window, SDLK_I);
        return 1;
    }
    return 0;
}

/* The IC sheet is modal: any click is taken; the close button or a click outside closes it. */
static int input_doc_window(AppState *app, float x, float y)
{
    if (app->doc_type < 0)
        return 0;
    if (app_rect_contains(app_layout_doc_close(), x, y) || !app_rect_contains(app_layout_doc_panel(), x, y))
        app->doc_type = -1;
    return 1;
}

static int input_click_header(AppState *app, float x, float y)
{
    switch (app_layout_header_at(x, y))
    {
    case APP_HEADER_PROJECTS:
        app_editing_key(app, SDLK_F3, 0);
        return 1;
    case APP_HEADER_EXAMPLES:
        app_state_toggle_panel(app, APP_PANEL_EXAMPLES);
        return 1;
    case APP_HEADER_SETTINGS:
        app_state_toggle_panel(app, APP_PANEL_SETTINGS);
        app->dragging = -1;
        app->drag_wire = -1;
        return 1;
    case APP_HEADER_PLAY:
        run_shortcut(app, NULL, SDLK_SPACE);
        return 1;
    default:
        return 0;
    }
}

static int graph_visible(const AppState *app)
{
    return app_controls_graph_visible(app);
}

/* Clicks inside the graph window never reach the bench below it. */
static int input_click_trace(AppState *app, float x, float y)
{
    if (graph_visible(app) && (app_rect_contains(app_layout_graph_measure(), x, y) ||
                               app_rect_contains(app_layout_graph_pin(), x, y)))
    {
        if (app->live_graph.active && app->live_graph.probe.kind == SIM_LIVE_PROBE_WIRE_CURRENT &&
            app_rect_contains(app_layout_graph_measure(), x, y))
            return 1;
        if (!app->live_graph.active || app->live_graph.probe.kind == SIM_LIVE_PROBE_WIRE_CURRENT)
        {
            app_state_toggle_panel(app, APP_PANEL_SETTINGS);
            return 1;
        }
        int selected = app->selected;
        int wire = app->selected_wire;
        app->selected = app->live_graph.probe.target.component;
        app->selected_wire = -1;
        if (app_rect_contains(app_layout_graph_measure(), x, y))
            app->probe_current = !app->probe_current;
        else
            cycle_probe_pin(app);
        app_commands_key(app, NULL, SDLK_T, 0);
        app->selected = selected;
        app->selected_wire = wire;
        return 1;
    }
    if (!graph_visible(app) || !app_rect_contains(app_layout_graph_panel(), x, y))
        return 0;
    if (app_rect_contains(app_layout_graph_close(), x, y))
    {
        app_commands_close_graph(app);
        return 1;
    }
    WirePoint offset = app_layout_graph_offset(); /* plot area designed at 918..1220 x 431..565 */
    float plot_x = x - offset.x, plot_y = y - offset.y;
    if (!app->live_graph.active && plot_x >= 918 && plot_x <= 1220 && plot_y >= 431 && plot_y <= 565)
    {
        app->sim.trace_index = spice_trace_nearest(&app->sim.trace, (plot_x - 918) / 302.0);
        app->sim.sim_running = 0;
        app->status = sim_state_trace_text(&app->sim);
    }
    return 1;
}

/* With a single ordinary part selected, choosing from the catalog swaps it in place. */
static int input_swap_selected(AppState *app, PartType type)
{
    /* Same type: nothing to swap, so the click places a new one as usual. */
    if (!selected_plain_part(app) || bench_is_board(app->bench.parts[app->selected].type) ||
        app->bench.parts[app->selected].type == type || bench_selection_count(&app->selection, &app->bench) > 1)
        return 0;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench) || !bench_swap_type(&app->bench, app->selected, type))
    {
        app->status = tr(TXT_APP_SWAP_FAILED);
        return 1;
    }
    if (!app_history_record(app, &before))
        return 1;
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
    if (app->palette_page == 2 && !app->palette_search[0])
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
    if (item >= palette_filtered_count(app->palette_page, app->palette_search))
        return 1;
    app->packed_placing = 0;
    app->pending.component = -1;
    if (input_swap_selected(app, palette_filtered_part(app->palette_page, app->palette_search, item)))
        return 1;
    app->placing = palette_filtered_part(app->palette_page, app->palette_search, item);
    app->status = tr(TXT_APP_CLICK_TO_PLACE);
    return 1;
}

static int input_click_controls(AppState *app, SDL_Window *window, float x, float y)
{
    if (app->project_overwrite >= 0)
    {
        int choice = app_layout_confirm_button_at(x, y);
        if (choice == 0 || choice == 2)
            app_projects_confirm(app, choice == 0);
        return 1;
    }
    if (app->editing >= 0)
    {
        SDL_Event key;
        SDL_zero(key);
        key.key.key = app_rect_contains(app_layout_value_button(1), x, y) ? SDLK_RETURN : SDLK_ESCAPE;
        if (app_rect_contains(app_layout_value_button(1), x, y) || app_rect_contains(app_layout_value_button(0), x, y))
            input_edit_key(app, window, &key);
        return 1;
    }
    if (app->editor_confirm_close)
        return app_editor_click(app, window, x, y);
    if (input_doc_window(app, x, y))
        return 1;
    if (app->settings || app->projects_open || app->examples_open)
    {
        if ((!app->editor_parent || app_layout_header_at(x, y) == APP_HEADER_PLAY) && input_click_header(app, x, y))
            return 1;
        if (input_projects(app, x, y) || input_examples(app, x, y))
            return 1;
        input_settings(app, window, x, y);
        return 1;
    }
    if (panels_closed(app))
    {
        if (app_rect_contains(app_layout_search_clear(), x, y))
        {
            app->palette_search[0] = 0;
            app->palette_offset = 0;
            return 1;
        }
        if (app_rect_contains(app_layout_search(), x, y))
        {
            if (SDL_StartTextInput(window))
                app->palette_search_active = 1;
            else
                app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
            return 1;
        }
        for (int action = 0; action < APP_ACTION_COUNT; ++action)
        {
            if (!app_rect_contains(app_layout_action((AppAction)action), x, y))
                continue;
            if (!app_controls_action_enabled(app, (AppAction)action))
                return 1;
            static const SDL_Keycode keys[] = {SDLK_Z, SDLK_Y, SDLK_N, SDLK_G, SDLK_T, SDLK_T};
            SDL_Keymod mod = action <= APP_ACTION_REDO || action == APP_ACTION_CHIP ? SDL_KMOD_CTRL : SDL_KMOD_NONE;
            if (action == APP_ACTION_CURVE)
                mod = SDL_KMOD_SHIFT;
            app_commands_key(app, window, keys[action], mod);
            return 1;
        }
    }
    if (input_click_inspector_toggle(app, x, y))
        return 1;
    if (input_click_environment(app, x, y))
        return 1;
    if (!app->inspector_minimized && app->selected >= 0 && app->bench.parts[app->selected].group &&
        app_rect_contains(app_layout_inspector_scale(), x, y))
    {
        int group = app->bench.parts[app->selected].group;
        start_settings_edit(app, window, APP_EDIT_GROUP_SCALE,
                            bench_group_width(&app->bench, group) * 100 / 98);
        return 1;
    }
    if (app_editor_click(app, window, x, y))
        return 1;
    if (input_click_inspector(app, window, x, y) || input_click_wire_inspector(app, x, y))
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
                    int selected_count = bench_selection_count(&app->selection, &app->bench) +
                                         bench_selection_knee_count(&app->selection, &app->bench);
                    app->dragging_selection_knee = (app->selection.knees[i] & (1u << k)) && selected_count > 1;
                    break;
                }
            }
        }
        if (app->drag_wire >= 0)
        {
            if (clicks >= 2)
            {
                if (!app_history_record(app, &app->bench))
                    return 1;
                bench_remove_knee(&app->bench, app->drag_wire, app->drag_knee);
                bench_selection_clear(&app->selection);
                app->drag_wire = -1;
                app->dragging_selection_knee = 0;
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

static int grid_only(void)
{
    return (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
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
        int snapped = app->snap_board && !snap_suspended() && !grid_only() &&
                      bench_snap_to_breadboard(&app->bench, added);
        if (!snapped)
        {
            input_snap_grid(app, &x, &y);
            if (!snap_suspended() && !grid_only())
                snap_part_terminals(app, added, &x, &y);
            bench_move_part(&app->bench, added, x, y);
        }
        if (!app_history_record(app, &before))
            return;
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
    int owner_is_board = bench_is_board(bench->parts[owner].type);
    for (int i = bench->part_count - 1; i >= 0; --i)
    {
        const Part *part = &bench->parts[i];
        if (i == owner || part->group || bench_is_board(part->type) || (!owner_is_board && i < owner))
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
    Pin hit = {-1, -1};
    for (int pass = 0; pass < 3; ++pass)
    {
        float best = INFINITY;
        for (int i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            int kind = part->group ? 0 : bench_is_board(part->type) ? 2 : 1;
            if (kind != pass)
                continue;
            float radius = pass == 2 || component_has_flag(part->type, COMPONENT_DIP) ? 7 : 12;
            radius /= zoom;
            for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
            {
                if (part->group && !bench_pin_exposed(part, pin))
                    continue;
                float px, py;
                bench_terminal_position(bench, (Pin){i, pin}, &px, &py);
                float distance = (x - px) * (x - px) + (y - py) * (y - py);
                if (distance < best && distance <= radius * radius &&
                    (part->group || see_through || !terminal_covered(bench, i, px, py)))
                {
                    hit = (Pin){i, pin};
                    best = distance;
                }
            }
        }
        if (hit.component >= 0)
            return hit;
    }
    return hit;
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
            if (!app_history_record(app, &before))
                return;
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
    hit = app_input_part_at(&app->bench, x, y, app->view.zoom);
    if (hit < 0 || !app->selection.parts[hit])
        bench_selection_clear(&app->selection);
    if (hit >= 0)
        app->selection.parts[hit] = 1;
    bench_selection_expand_groups(&app->selection, &app->bench);
    app->selected = hit;
    app->selected_wire = -1;
    if (hit < 0 || bench_is_board(app->bench.parts[hit].type))
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
        if (clicks >= 2 && app->bench.parts[hit].type == PART_NOTE &&
            !note_resize_hit(&app->bench.parts[hit], x, y, app->view.zoom))
        {
            app->selected = hit;
            start_note_edit(app, NULL);
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
            if (!app_history_record(app, &app->bench))
                return;
            bench_set_on(&app->bench, hit, !app->bench.parts[hit].on);
            app->status = tr(TXT_APP_STATE_CHANGED);
        }
        else
        {
            app->dragging = hit;
            app->move_recorded = 0;
            app->drag_snap_mode = 0;
            app->offset_x = x - app->bench.parts[hit].x;
            app->offset_y = y - app->bench.parts[hit].y;
            const Part *part = &app->bench.parts[hit];
            float scale = bench_part_scale(part);
            app->resizing_note = note_resize_hit(part, x, y, app->view.zoom);
            if (app->resizing_note)
            {
                app->offset_x = part->x - 120 * scale;
                app->offset_y = part->y - 55 * scale;
            }
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

/* Splits the wire under (x, y) with a junction. Returns its index, -1 when no wire is there, or -2 when the
 * bench is full (the status says why). History is recorded by the caller. */
#define NO_WIRE_HERE -1
#define JUNCTION_REFUSED -2
static int split_wire_at(AppState *app, float x, float y)
{
    int index = hit_wire(&app->bench, x, y, 7.0f / app->view.zoom, app->round_wires);
    if (index < 0)
        return NO_WIRE_HERE;
    const Wire *wire = &app->bench.wires[index];
    WirePoint start, end, projected;
    int slot;
    bench_terminal_position(&app->bench, wire->a, &start.x, &start.y);
    bench_terminal_position(&app->bench, wire->b, &end.x, &end.y);
    if (!wire_pick_segment(wire, start, end, app->round_wires ? 18.0f : 0, (WirePoint){x, y},
                           7.0f / app->view.zoom, &slot, &projected))
        return NO_WIRE_HERE;
    int junction = bench_split_wire(&app->bench, index, slot, projected);
    if (junction >= 0)
        return junction;
    report(app, TXT_APP_JUNCTION_LIMIT, WORKBENCH_MAX_COMPONENTS, WORKBENCH_MAX_WIRES);
    return JUNCTION_REFUSED;
}

/* With a wire in progress, a click on another wire ends it there through a new junction. */
static int input_end_wire_on_wire(AppState *app, float x, float y)
{
    if (app->pending.component < 0)
        return 0;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 1;
    }
    int junction = split_wire_at(app, x, y);
    if (junction < 0)
        return junction == JUNCTION_REFUSED;
    Pin target = {junction, 0};
    int result = bench_toggle_wire(&app->bench, app->pending, target);
    if (result != 1)
    {
        bench_replace(&app->bench, &before);
        app->status = tr(TXT_APP_CONNECT_FAILED);
        return 1;
    }
    int wire = app->bench.wire_count - 1;
    bench_set_wire_route(&app->bench, wire, app->route, app->route_count);
    bench_set_wire_color(&app->bench, wire, app->wire_color);
    if (!app_history_record(app, &before))
        return 1;
    app->pending.component = -1;
    app->route_count = 0;
    app->status = tr(TXT_APP_WIRE_JOINED);
    return 1;
}

/* Alt+click on a wire: new junction there, and a wire starts from it. */
static int input_branch_from_wire(AppState *app, float x, float y)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = tr(TXT_APP_OUT_OF_MEMORY);
        return 1;
    }
    int junction = split_wire_at(app, x, y);
    if (junction < 0)
        return junction == JUNCTION_REFUSED;
    if (!app_history_record(app, &before))
        return 1;
    app->selected_wire = -1;
    app->pending = (Pin){junction, 0};
    app->route_count = 0;
    app->status = tr(TXT_APP_WIRE_BRANCH_STARTED);
    return 1;
}

/* Alt+drag on a junction moves it; the wires follow. */
static int input_click_junction(AppState *app, float x, float y)
{
    Pin pin = input_pin_at(&app->bench, x, y, app->view.zoom, app->xray);
    if (pin.component < 0 || app->bench.parts[pin.component].type != PART_JUNCTION)
        return 0;
    app->selected = pin.component;
    app->dragging = pin.component;
    app->move_recorded = 0;
    app->offset_x = x - app->bench.parts[pin.component].x;
    app->offset_y = y - app->bench.parts[pin.component].y;
    app->pending.component = -1;
    app->status = tr(TXT_APP_DRAG_PART);
    return 1;
}

/* Alt+drag on a chip terminal rearranges it along the package edges (left/right, any order). */
static int input_click_chip_terminal(AppState *app, float x, float y)
{
    Pin pin = input_pin_at(&app->bench, x, y, app->view.zoom, app->xray);
    if (bench_group_port_index(&app->bench, pin) < 0)
        return 0;
    if (bench_group_ports(&app->bench, app->bench.parts[pin.component].group) > BENCH_GROUP_LAYOUT_MAX)
    {
        app->status = tr(TXT_APP_CHIP_TERMINALS_TOO_MANY);
        return 1;
    }
    app->drag_port = pin;
    app->move_recorded = 0;
    app->selected = pin.component;
    app->pending.component = -1;
    app->status = tr(TXT_APP_DRAG_CHIP_TERMINAL);
    return 1;
}

/* Side from the pointer's side of the package; slot from its height, counted as if already moved there. */
static void chip_terminal_target(const AppState *app, int group, int index, WirePoint world, int *side, int *slot)
{
    WirePoint local = bench_group_to_local(&app->bench.groups[group - 1], world);
    int counts[2] = {0, 0}, from_side = BENCH_PORT_LEFT;
    for (int other = 0; other < bench_group_ports(&app->bench, group); ++other)
    {
        int other_side, other_slot, side_count;
        bench_group_port_place(&app->bench, group, other, &other_side, &other_slot, &side_count);
        ++counts[other_side];
        if (other == index)
            from_side = other_side;
    }
    *side = local.x < 0 ? BENCH_PORT_LEFT : BENCH_PORT_RIGHT;
    int rows = counts[*side] + (*side != from_side);
    *slot = (int)lroundf(local.y / BENCH_GROUP_PIN_SPACING + (rows - 1) * .5f);
}

static void input_drag_chip_terminal(AppState *app)
{
    Pin pin = app->drag_port;
    int index = bench_group_port_index(&app->bench, pin);
    if (index < 0)
        return;
    int group = app->bench.parts[pin.component].group, side, slot;
    chip_terminal_target(app, group, index, app->mouse_world, &side, &slot);
    int from_side, from_slot, side_count;
    bench_group_port_place(&app->bench, group, index, &from_side, &from_slot, &side_count);
    if (side == from_side && slot == from_slot)
        return;
    if (!app->move_recorded)
    {
        if (!bench_history_record(app->history, &app->bench))
        {
            app->status = tr(TXT_APP_HISTORY_FAILED);
            return;
        }
        app->move_recorded = 1;
    }
    bench_group_move_port(&app->bench, group, index, side, slot);
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
    WirePoint point = app_layout_to_ui(w, h, event->button.x, event->button.y);
    x = point.x;
    y = point.y;
    if (input_click_controls(app, window, x, y))
        return;
    if (x < BENCH_VIEW_LEFT || y < BENCH_VIEW_TOP || y > BENCH_VIEW_BOTTOM)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    if (SDL_GetModState() & SDL_KMOD_SHIFT)
    {
        if (input_click_knee(app, x, y, event->button.clicks))
            return;
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
        if (input_click_knee(app, x, y, event->button.clicks))
            return;
        if (input_click_chip_terminal(app, x, y) || input_click_junction(app, x, y))
            return;
        if (input_click_lead(app, x, y, event->button.clicks))
            return;
        if (input_branch_from_wire(app, x, y))
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
    if (SDL_GetModState() & SDL_KMOD_CTRL)
    {
        int hit = app_input_part_at(&app->bench, x, y, app->view.zoom);
        if (hit >= 0)
        {
            if (app->selected >= 0 && !bench_selection_count(&app->selection, &app->bench))
                app->selection.parts[app->selected] = 1;
            int group = app->bench.parts[hit].group;
            int selected = !app->selection.parts[hit];
            for (int i = 0; i < app->bench.part_count; ++i)
                if (i == hit || (group && app->bench.parts[i].group == group))
                    app->selection.parts[i] = selected;
            app->selected = -1;
            for (int i = 0; i < app->bench.part_count; ++i)
                if (app->selection.parts[i])
                    app->selected = i;
            app->selected_wire = -1;
            app->pending.component = -1;
        }
        return;
    }
    /* Enlarged terminal hit areas must not swallow a small component's visible body. */
    if (app->pending.component < 0)
    {
        int body = app_input_part_at(&app->bench, x, y, app->view.zoom);
        if (body >= 0 && !app->bench.parts[body].group && !bench_is_board(app->bench.parts[body].type) &&
            app->bench.parts[body].type != PART_JUNCTION)
        {
            const Part *part = &app->bench.parts[body];
            WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
            float half_width, half_height;
            component_body_extent(part->type, &half_width, &half_height);
            if (part->type == PART_LED)
                local.y += 6;
            if (fabsf(local.x) < half_width - 6 && fabsf(local.y) < fmaxf(1, half_height - 6))
            {
                input_select(app, x, y, event->button.clicks);
                return;
            }
        }
    }
    Pin clicked = input_pin_at(&app->bench, x, y, app->view.zoom, app->xray);
    hitpin = clicked.pin;
    if (hitpin >= 0 && event->button.clicks >= 2 &&
        (bench_group_port_index(&app->bench, clicked) >= 0 || app->bench.parts[clicked.component].type == PART_JUNCTION))
    {
        start_port_name_edit(app, window, clicked);
        return;
    }
    if (hitpin >= 0)
    {
        input_connect_pin(app, clicked);
        return;
    }
    if (input_end_wire_on_wire(app, x, y))
        return;
    if (app->pending.component >= 0)
    {
        if (app->route_count < WIRE_MAX_KNEES)
        {
            WirePoint anchor;
            if (app->route_count)
                anchor = app->route[app->route_count - 1];
            else
                bench_terminal_position(&app->bench, app->pending, &anchor.x, &anchor.y);
            input_snap_grid(app, &x, &y);
            bench_grid_align_axis(anchor.x, anchor.y, 8.0f / app->view.zoom, &x, &y);
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
    if (app->drag_port.component >= 0 && app->move_recorded)
        app->status = tr(TXT_APP_CHIP_TERMINAL_MOVED);
    app->drag_port.component = -1;
    app->selecting_area = 0;
    if (bench_selection_count(&app->selection, &app->bench) <= 1 && app->dragging >= 0 && app->move_recorded &&
        !app->bench.parts[app->dragging].group && app->snap_board && app->drag_snap_mode == 0 &&
        bench_snap_to_breadboard(&app->bench, app->dragging))
        app->status = tr(TXT_APP_PART_SNAPPED);
    app->dragging = -1;
    app->resizing_note = 0;
    app->drag_snap_mode = 0;
    app->drag_wire = -1;
    app->dragging_selection_knee = 0;
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
    WirePoint world = app_layout_to_ui(width, height, event->motion.x, event->motion.y);
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
        WirePoint pointer = app_layout_to_ui(w, h, event->motion.x, event->motion.y);
        float x = pointer.x;
        float y = pointer.y;
        WirePoint *point = &app->bench.wires[app->drag_wire].knees[app->drag_knee];
        bench_view_to_world(&app->view, x, y, &x, &y);
        input_snap_grid(app, &x, &y);
        if (!app->dragging_selection_knee)
        {
            WirePoint anchor;
            if (app->drag_knee > 0)
                anchor = app->bench.wires[app->drag_wire].knees[app->drag_knee - 1];
            else
                bench_terminal_position(&app->bench, app->bench.wires[app->drag_wire].a, &anchor.x, &anchor.y);
            float tolerance = SDL_GetModState() & SDL_KMOD_SHIFT ? 1e9f : 8.0f / app->view.zoom;
            bench_grid_align_axis(anchor.x, anchor.y, tolerance, &x, &y);
        }
        if (point->x != x || point->y != y)
        {
            if (!app->move_recorded)
            {
                if (!app_history_record(app, &app->bench))
                    return;
                app->move_recorded = 1;
            }
            if (app->dragging_selection_knee)
                bench_selection_move(&app->bench, &app->selection, x - point->x, y - point->y);
            else
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
                    (bench_is_board(target->type) && !app->snap_board))
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
        app->drag_snap_mode = snap_suspended() ? 2 : grid_only() ? 1 : 0;
        WirePoint point = app_layout_to_ui(w, h, event->motion.x, event->motion.y);
        float x = point.x;
        float y = point.y;
        bench_view_to_world(&app->view, x, y, &x, &y);
        if (app->resizing_note)
        {
            float scale = fmaxf((x + 6 - app->offset_x) / 240, (y + 6 - app->offset_y) / 110);
            scale = fminf(PART_SCALE_MAX, fmaxf(.5f, scale));
            if (!app->move_recorded)
            {
                if (!bench_history_record(app->history, &app->bench))
                {
                    app->status = tr(TXT_APP_HISTORY_FAILED);
                    return;
                }
                app->move_recorded = 1;
            }
            bench_set_scale(&app->bench, app->dragging, scale);
            bench_move_part(&app->bench, app->dragging, app->offset_x + 120 * scale, app->offset_y + 55 * scale);
            return;
        }
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
            if (!snap_suspended() && !grid_only())
                snap_part_terminals(app, app->dragging, &x, &y);
        }
        else
        {
            Part candidate = *dragged;
            candidate.x = x;
            candidate.y = y;
            int board_snap = app->snap_board && !snap_suspended() && !grid_only() &&
                             bench_selection_count(&app->selection, &app->bench) <= 1 &&
                             bench_breadboard_snap_position(&app->bench, &candidate, &x, &y);
            if (!board_snap)
                input_snap_grid(app, &x, &y);
            if (!snap_suspended() && !grid_only())
                snap_part_terminals(app, app->dragging, &x, &y);
        }
        if (app->bench.parts[app->dragging].x != x || app->bench.parts[app->dragging].y != y)
        {
            if (!app->move_recorded)
            {
                if (!app_history_record(app, &app->bench))
                    return;
                app->move_recorded = 1;
            }
            if (bench_selection_count(&app->selection, &app->bench) > 1 ||
                bench_selection_knee_count(&app->selection, &app->bench) > 0 || app->bench.parts[app->dragging].group)
                bench_selection_move(&app->bench, &app->selection, x - app->bench.parts[app->dragging].x,
                                     y - app->bench.parts[app->dragging].y);
            else
                bench_move_part(&app->bench, app->dragging, x, y);
        }
    }
}


static void input_area_update(AppState *app)
{
    app->area_end = app->mouse_world;
    bench_selection_rect(&app->selection, &app->bench, app->area_start, app->area_end);
    app->selected = -1;
    for (int i = 0; i < app->bench.part_count; ++i)
        if (app->selection.parts[i])
            app->selected = i;
    report(app, TXT_APP_AREA_SELECTED, bench_selection_count(&app->selection, &app->bench),
           bench_selection_knee_count(&app->selection, &app->bench));
}

static void input_mouse_position(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    WirePoint point = app_layout_to_ui(width, height, event->motion.x, event->motion.y);
    float x = point.x;
    float y = point.y;
    bench_view_to_world(&app->view, x, y, &app->mouse_world.x, &app->mouse_world.y);
    app->mouse_screen = (WirePoint){x, y};
    app->mouse_position_valid = 1;
    if (!app->selecting_area)
        return;
    input_area_update(app);
}

/* Dragging a selection area against a workspace edge scrolls the bench so far parts can be reached. */
#define AREA_SCROLL_ZONE 32.0f
#define AREA_SCROLL_SPEED 900.0f /* screen pixels per second at full push */
#define AREA_SCROLL_MAX_STEP_MS 50

static void input_area_autoscroll(AppState *app)
{
    if (!app->selecting_area || !app->mouse_position_valid)
    {
        app->area_scroll_ticks = 0;
        return;
    }
    float push_x = bench_view_edge_push(app->mouse_screen.x, BENCH_VIEW_LEFT, BENCH_VIEW_LEFT + BENCH_VIEW_WIDTH,
                                        AREA_SCROLL_ZONE);
    float push_y = bench_view_edge_push(app->mouse_screen.y, BENCH_VIEW_TOP, BENCH_VIEW_TOP + BENCH_VIEW_HEIGHT,
                                        AREA_SCROLL_ZONE);
    uint64_t now = SDL_GetTicks();
    if (!push_x && !push_y)
    {
        app->area_scroll_ticks = 0;
        return;
    }
    uint64_t elapsed = app->area_scroll_ticks ? now - app->area_scroll_ticks : 0;
    app->area_scroll_ticks = now;
    if (elapsed > AREA_SCROLL_MAX_STEP_MS)
        elapsed = AREA_SCROLL_MAX_STEP_MS;
    float step = AREA_SCROLL_SPEED * (float)elapsed / 1000.0f;
    app->view.pan_x -= push_x * step;
    app->view.pan_y -= push_y * step;
    bench_view_to_world(&app->view, app->mouse_screen.x, app->mouse_screen.y, &app->mouse_world.x,
                        &app->mouse_world.y);
    input_area_update(app);
}

static int input_keyboard_focus(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    if (event->type != SDL_EVENT_KEY_DOWN)
        return 0;
    SDL_Keycode key = event->key.key;
    AppControlContext context = app_controls_context(app);
    if (app->keyboard_context != context)
    {
        app->keyboard_context = context;
        app->keyboard_focus = -1;
    }
    if (key == SDLK_TAB)
    {
        if (context == APP_UI_BENCH && (event->key.mod & SDL_KMOD_CTRL))
            return 0;
        if (app->code_part >= 0 && !(event->key.mod & SDL_KMOD_CTRL) && context == APP_UI_CODE)
            return 0;
        AppRect targets[APP_CONTROL_LIMIT];
        int count = app_controls_targets(app, targets);
        int direction = event->key.mod & SDL_KMOD_SHIFT ? -1 : 1;
        if (app->keyboard_focus >= 0)
        {
            app->keyboard_focus = -1;
            for (int i = 0; i < count; ++i)
                if (app_rect_center_x(targets[i]) == app->keyboard_target.x &&
                    app_rect_center_y(targets[i]) == app->keyboard_target.y)
                    app->keyboard_focus = i;
        }
        if (app->keyboard_focus < 0)
            app->keyboard_focus = direction > 0 ? 0 : count - 1;
        else
            app->keyboard_focus = (app->keyboard_focus + direction + count) % count;
        AppRect focus = targets[app->keyboard_focus];
        app->keyboard_target = (WirePoint){app_rect_center_x(focus), app_rect_center_y(focus)};
        if (app->palette_search_active)
        {
            SDL_StopTextInput(window);
            app->palette_search_active = 0;
        }
        return 1;
    }
    AppRect focus = app_controls_focus(app);
    if (focus.w <= 0 || app->palette_search_active)
        return 0;
    if (key == SDLK_ESCAPE)
    {
        app->keyboard_focus = -1;
        return context == APP_UI_BENCH;
    }
    if ((key == SDLK_LEFT || key == SDLK_RIGHT) && app->selected >= 0 &&
        focus.y == app_layout_environment_slider().y)
    {
        if (bench_history_record(app->history, &app->bench))
        {
            double value = app->bench.parts[app->selected].control + (key == SDLK_RIGHT ? .05 : -.05);
            bench_set_control(&app->bench, app->selected, fmin(1, fmax(0, value)));
        }
        return 1;
    }
    if (key != SDLK_RETURN && key != SDLK_KP_ENTER && key != SDLK_SPACE)
        return 0;
    if (event->key.repeat)
        return 1;
    if (context == APP_UI_CODE && app_rect_contains(app_layout_code_run(), app_rect_center_x(focus), app_rect_center_y(focus)))
        app_code_apply(app);
    else if (context == APP_UI_CODE && app_rect_contains(app_layout_code_close(), app_rect_center_x(focus), app_rect_center_y(focus)))
        app_code_close(app, window);
    else
        input_click_controls(app, window, app_rect_center_x(focus), app_rect_center_y(focus));
    app->dragging_environment = -1;
    return 1;
}

static int input_search(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    if (app_controls_context(app) != APP_UI_BENCH)
        return 0;
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_F && (event->key.mod & SDL_KMOD_CTRL))
    {
        if (SDL_StartTextInput(window))
        {
            app->palette_search_active = 1;
            app->palette_search_select_all = 1;
        }
        else
            app->status = tr(TXT_APP_TEXT_INPUT_FAILED);
        return 1;
    }
    if (!app->palette_search_active)
        return 0;
    if (event->type == SDL_EVENT_TEXT_INPUT)
    {
        size_t used = app->palette_search_select_all ? 0 : strlen(app->palette_search);
        if (used + strlen(event->text.text) >= sizeof(app->palette_search))
        {
            app->status = tr(TXT_UI_SEARCH_TOO_LONG);
            return 1;
        }
        app->palette_search[used] = 0;
        int cursor = (int)used;
        code_insert(app->palette_search, sizeof(app->palette_search), &cursor, event->text.text);
        app->palette_search_select_all = 0;
        app->palette_offset = 0;
        app->placing = -1;
        return 1;
    }
    if (event->type == SDL_EVENT_KEY_DOWN)
    {
        if (event->key.key == SDLK_BACKSPACE)
        {
            int cursor = (int)strlen(app->palette_search);
            if (app->palette_search_select_all)
                app->palette_search[0] = 0;
            else
                code_backspace(app->palette_search, &cursor);
            app->palette_search_select_all = 0;
            app->palette_offset = 0;
        }
        else if (event->key.key == SDLK_A && (event->key.mod & SDL_KMOD_CTRL))
            app->palette_search_select_all = 1;
        else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_ESCAPE)
        {
            app->palette_search_active = 0;
            SDL_StopTextInput(window);
        }
        return 1;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        app->palette_search_active = 0;
        SDL_StopTextInput(window);
    }
    return 0;
}

/* Coalesce hover hit testing to one pass per frame; drag/model events still all execute. */
static void input_refresh_hover(AppState *app)
{
    app->hovered_pin = (Pin){-1, -1};
    app->hover_revision = app->bench.revision;
    float x = app->mouse_screen.x, y = app->mouse_screen.y;
    if (x < BENCH_VIEW_LEFT || y < BENCH_VIEW_TOP || y >= BENCH_VIEW_BOTTOM || app->settings || app->projects_open || app->examples_open || app->editing >= 0 ||
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
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            app->keyboard_focus = -1;
        /* Finish draining this context before routing to another editor. */
        if (app->editor_open_requested || app->editor_close_requested)
            continue;
        if (e.type == SDL_EVENT_MOUSE_MOTION)
            input_mouse_position(app, window, &e);
        if (input_keyboard_focus(app, window, &e) || input_search(app, window, &e))
            continue;
        if (app->project_overwrite >= 0 && e.type != SDL_EVENT_QUIT && e.type != SDL_EVENT_KEY_DOWN)
        {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
                input_click(app, window, &e);
            continue;
        }
        if (e.type != SDL_EVENT_QUIT && app->editing < 0 && !app->settings && !app->projects_open &&
            !app->examples_open && !app->editor_confirm_close && app_code_event(app, window, &e))
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
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
                input_click(app, window, &e);
            continue;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
        {
            app_commands_key(app, window, e.key.key, e.key.mod);
        }
        else if ((app->projects_open || app->examples_open || app->doc_type >= 0) &&
                 e.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
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
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->drag_port.component >= 0)
        {
            input_drag_chip_terminal(app);
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
    input_area_autoscroll(app);
    app_code_validate(app, window);
    if (app->mouse_position_valid)
        input_refresh_hover(app);
}
