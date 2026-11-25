#include "app_code_editor.h"
#include "app_input.h"
#include "app_controls.h"
#include "app_palette.h"
#include "component_registry.h"
#include "app_editing.h"
#include "app_packing.h"
#include "workbench_pack.h"
#include "ui_text.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void drain(AppState *app, SDL_Window *window)
{
    /* SDL may end a poll turn at a sentinel preceding an event just injected by the test. */
    for (int turn = 0; turn < 8; ++turn)
    {
        app_input_poll(app, window);
        if (!SDL_HasEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT) &&
            !SDL_HasEvents(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP))
            return;
    }
    assert(!"UI events were not consumed");
}

static void key(AppState *app, SDL_Window *window, SDL_Keycode code, SDL_Keymod mod)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = code;
    event.key.mod = mod;
    event.key.down = true;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.scancode = SDL_GetScancodeFromKey(code, NULL);
    assert(SDL_PushEvent(&event));
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    assert(SDL_PushEvent(&event));
    drain(app, window);
}

static void click(AppState *app, SDL_Window *window, AppRect rect)
{
    int width, height;
    assert(SDL_GetWindowSize(window, &width, &height));
    WirePoint point = app_layout_from_ui(width, height, app_rect_center_x(rect), app_rect_center_y(rect));
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.x = point.x;
    event.button.y = point.y;
    assert(SDL_PushEvent(&event));
    drain(app, window);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    assert(SDL_PushEvent(&event));
    drain(app, window);
}

static void type(AppState *app, SDL_Window *window, const char *text)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    event.text.windowID = SDL_GetWindowID(window);
    assert(SDL_PushEvent(&event));
    drain(app, window);
}

static void dense_led_selection_flow(AppState *app, SDL_Window *window)
{
    assert(bench_add(&app->bench, PART_LED, 500, 350) == 0);
    assert(bench_add(&app->bench, PART_LED, 520, 350) == 1);
    click(app, window, (AppRect){500, 340, 1, 1});
    assert(app->selected == 0);
    click(app, window, (AppRect){520, 340, 1, 1});
    assert(app->selected == 1);
    assert(app_input_part_at(&app->bench, 565, 340, 1) == -1);
    bench_snapshot_clear(&app->bench);
    app->selected = -1;
    bench_selection_clear(&app->selection);
}

static void search_flow(AppState *app, SDL_Window *window)
{
    /* Find a board from Basics, pick it, place it; clearing restores that category. */
    key(app, window, SDLK_F, SDL_KMOD_CTRL);
    type(app, window, "ARDUINO");
    assert(!strcmp(app->palette_search, "ARDUINO"));
    assert(palette_filtered_count(0, app->palette_search) == 1);
    key(app, window, SDLK_RETURN, 0);
    click(app, window, app_layout_palette_item(0));
    assert(app->placing == PART_ARDUINO_UNO);
    click(app, window, (AppRect){500, 350, 2, 2});
    assert(app->bench.part_count == 1 && app->bench.parts[0].type == PART_ARDUINO_UNO);
    click(app, window, app_layout_search_clear());
    assert(!app->palette_search[0] && app->palette_page == 0);
    /* Opposite: no match cannot pick a hidden card or change the circuit. */
    key(app, window, SDLK_F, SDL_KMOD_CTRL);
    type(app, window, "no-such-part");
    key(app, window, SDLK_RETURN, 0);
    int count = app->bench.part_count;
    click(app, window, app_layout_palette_item(0));
    assert(app->placing < 0 && app->bench.part_count == count);
    click(app, window, app_layout_search_clear());
}

static void focus_and_save_flow(AppState *app, SDL_Window *window)
{
    /* Tab focuses Projects without changing catalog; Enter opens the dialog. */
    key(app, window, SDLK_TAB, 0);
    assert(app->palette_page == 0 && app->keyboard_focus == 0);
    key(app, window, SDLK_RETURN, 0);
    assert(app->projects_open);
    /* Focus stays in the dialog; reverse traversal reaches its close control. */
    key(app, window, SDLK_TAB, SDL_KMOD_SHIFT);
    AppRect focus = app_controls_focus(app);
    AppRect close = app_layout_project_close();
    assert(focus.x == close.x && focus.y == close.y);
    key(app, window, SDLK_SPACE, 0);
    assert(!app->projects_open);
    /* Save, change the circuit, then cancel and confirm replacing the same slot. */
    app->save_slot = 5;
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-6.elec"));
    key(app, window, SDLK_S, SDL_KMOD_CTRL);
    assert(app->saved_slots[5]);
    assert(bench_add(&app->bench, PART_LED, 650, 350) == 1);
    key(app, window, SDLK_S, SDL_KMOD_CTRL);
    assert(app->project_overwrite == 5);
    uint64_t revision = app->bench.revision;
    key(app, window, SDLK_TAB, 0);
    SDL_Event repeat;
    SDL_zero(repeat);
    repeat.type = SDL_EVENT_KEY_DOWN;
    repeat.key.key = SDLK_RETURN;
    repeat.key.repeat = true;
    assert(SDL_PushEvent(&repeat));
    drain(app, window);
    assert(app->project_overwrite == 5); /* Holding Enter must not activate the focused action. */
    key(app, window, SDLK_RETURN, 0);
    assert(app->project_overwrite < 0 && app->bench.revision == revision);
    Workbench saved = {0};
    WorkbenchSettings settings;
    assert(workbench_load_project(APP_TEST_PATH(APP_TEST_SLOTS, "slot-6.elec"), &saved, &settings));
    assert(saved.part_count == 1);
    bench_snapshot_clear(&saved);
    key(app, window, SDLK_S, SDL_KMOD_CTRL);
    assert(app->project_overwrite == 5);
    click(app, window, app_layout_confirm_button(0));
    key(app, window, SDLK_L, SDL_KMOD_CTRL);
    assert(app->bench.part_count == 2 && app->project_parts[5] == 2);
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-6.elec"));
}

static void actions_and_scaling_flow(AppState *app, SDL_Window *window)
{
    /* Undo/redo visible buttons share the existing history, including at another aspect ratio. */
    int before = app->bench.part_count;
    click(app, window, app_layout_action(APP_ACTION_UNDO));
    click(app, window, app_layout_action(APP_ACTION_REDO));
    assert(app->bench.part_count == before);
    assert(SDL_SetWindowSize(window, 1000, 900));
    assert(SDL_SyncWindow(window));
    click(app, window, app_layout_header(APP_HEADER_SETTINGS));
    assert(app->settings);
    int dark = app->dark_theme;
    click(app, window, app_layout_setting(APP_SETTING_DARK_THEME));
    assert(app->dark_theme != dark);
    assert(app->show_connection_pins);
    click(app, window, app_layout_setting(APP_SETTING_CONNECTION_PINS));
    assert(!app->show_connection_pins);
    int scale = app->gui_font_size;
    click(app, window, app_layout_setting(APP_SETTING_FONT_LARGER));
    if (app->gui_font_size == scale)
        assert(!strcmp(app->status, tr(TXT_UI_SCALE_UNAVAILABLE)) || !strcmp(app->status, tr(TXT_UI_WINDOW_SIZE_FAILED)));
    else
    {
        assert(app->gui_font_size == scale + 1);
        assert(SDL_SyncWindow(window));
        click(app, window, app_layout_setting(APP_SETTING_DARK_THEME));
        assert(app->dark_theme == dark);
    }
    key(app, window, SDLK_ESCAPE, 0);
    assert(!app->settings);
    /* A disabled chip action with no selection has no effect. */
    app->selected = -1;
    bench_selection_clear(&app->selection);
    before = app->bench.part_count;
    click(app, window, app_layout_action(APP_ACTION_CHIP));
    assert(app->bench.part_count == before && !app->bench.groups[0].active);
}

static void graph_options_flow(AppState *app, SDL_Window *window)
{
    /* Change the terminal on the plotted part, even after selecting another part. */
    bench_snapshot_clear(&app->bench);
    bench_selection_clear(&app->selection);
    assert(bench_add(&app->bench, PART_CLOCK, 400, 300) == 0);
    assert(bench_add(&app->bench, PART_RESISTOR, 650, 300) == 1);
    assert(bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0}));
    app->sim.auto_enabled = 1;
    app->selected = 0;
    app->selected_wire = -1;
    app->probe_pin = 0;
    app->probe_current = 0;
    click(app, window, app_layout_action(APP_ACTION_GRAPH));
    assert(app->live_graph.active && app->live_graph.probe.target.component == 0);
    app->selected = 1;
    click(app, window, app_layout_graph_pin());
    assert(app->live_graph.probe.target.component == 0 && app->live_graph.probe.target.pin == 1);
    assert(app->selected == 1);
    click(app, window, app_layout_graph_close());
    assert(!app->live_graph.active);
    /* A wire measures current; its disabled quantity control cannot pretend to measure voltage. */
    app->selected = -1;
    app->selected_wire = 0;
    click(app, window, app_layout_action(APP_ACTION_GRAPH));
    assert(app->live_graph.probe.kind == SIM_LIVE_PROBE_WIRE_CURRENT);
    click(app, window, app_layout_graph_measure());
    assert(!app->settings && app->live_graph.probe.kind == SIM_LIVE_PROBE_WIRE_CURRENT);
    click(app, window, app_layout_graph_close());
    assert(!app->live_graph.active);
}

static void values_and_chip_flow(AppState *app, SDL_Window *window)
{
    /* Add a resistor, reject invalid text beside its field, confirm a value, then undo. */
    click(app, window, app_layout_palette_item(0));
    assert(app->placing == PART_RESISTOR);
    click(app, window, (AppRect){750, 450, 2, 2});
    click(app, window, (AppRect){750, 450, 2, 2}); /* Select the newly placed part before editing. */
    int part = app->selected;
    assert(part >= 0 && app->bench.parts[part].type == PART_RESISTOR);
    double original = app->bench.parts[part].value;
    click(app, window, app_layout_inspector_value());
    for (int i = 0; i < 32; ++i)
        key(app, window, SDLK_BACKSPACE, 0);
    type(app, window, "bad");
    click(app, window, app_layout_value_button(1));
    assert(app->editing == part && app->edit_error && app->bench.parts[part].value == original);
    for (int i = 0; i < 3; ++i)
        key(app, window, SDLK_BACKSPACE, 0);
    type(app, window, "2200");
    click(app, window, app_layout_value_button(1));
    assert(app->editing < 0 && app->bench.parts[part].value == 2200);
    click(app, window, app_layout_action(APP_ACTION_UNDO));
    assert(app->bench.parts[part].value == original);
    /* Create chip uses the same selection and library identity as Ctrl+G. */
    app->selected = part;
    app_packing_refresh(app);
    int available[BLOCK_LIBRARY_SLOTS];
    memcpy(available, app->library_available, sizeof(available));
    click(app, window, app_layout_action(APP_ACTION_CHIP));
    assert(app->bench.parts[part].group && app->palette_page == 2);
    for (int i = 0; i < BLOCK_LIBRARY_SLOTS; ++i)
    {
        if (available[i] || !app->library_available[i])
            continue;
        char path[128];
        snprintf(path, sizeof(path), "%s/block-%d.elec", app_test_directory(APP_TEST_BLOCKS), i + 1);
        assert(remove(path) == 0);
    }
}

static void multi_value_flow(AppState *app, SDL_Window *window)
{
    /* Select only clicked resistors, edit them together, then undo the entire edit. */
    assert(bench_add(&app->bench, PART_RESISTOR, 450, 300) == 0);
    assert(bench_add(&app->bench, PART_RESISTOR, 650, 300) == 1);
    assert(bench_add(&app->bench, PART_RESISTOR, 850, 300) == 2);
    click(app, window, (AppRect){450, 300, 1, 1});
    SDL_SetModState(SDL_KMOD_CTRL);
    click(app, window, (AppRect){850, 300, 1, 1});
    SDL_SetModState(0);
    assert(app->selection.parts[0] && !app->selection.parts[1] && app->selection.parts[2]);
    key(app, window, SDLK_E, 0);
    assert(app->editing >= 0);
    key(app, window, SDLK_A, SDL_KMOD_CTRL);
    type(app, window, "200");
    key(app, window, SDLK_RETURN, 0);
    assert(app->editing < 0);
    assert(app->bench.parts[0].value == 200 && app->bench.parts[2].value == 200);
    assert(app->bench.parts[1].value == 1000);
    key(app, window, SDLK_Z, SDL_KMOD_CTRL);
    assert(app->bench.parts[0].value == 1000 && app->bench.parts[2].value == 1000);
    /* Ctrl+click removes one; a plain click replaces the selection. */
    click(app, window, (AppRect){450, 300, 1, 1});
    SDL_SetModState(SDL_KMOD_CTRL);
    click(app, window, (AppRect){850, 300, 1, 1});
    click(app, window, (AppRect){450, 300, 1, 1});
    SDL_SetModState(0);
    assert(!app->selection.parts[0] && app->selection.parts[2]);
    click(app, window, (AppRect){650, 300, 1, 1});
    assert(bench_selection_count(&app->selection, &app->bench) == 1 && app->selected == 1);
    /* Mixed species cannot silently edit just the last clicked part. */
    assert(bench_add(&app->bench, PART_CAPACITOR, 850, 450) == 3);
    SDL_SetModState(SDL_KMOD_CTRL);
    click(app, window, (AppRect){850, 450, 1, 1});
    SDL_SetModState(0);
    key(app, window, SDLK_E, 0);
    assert(app->editing < 0);
    bench_snapshot_clear(&app->bench);
    bench_selection_clear(&app->selection);
    app->selected = -1;
}

static void catalog_categories_cover_every_part(void)
{
    int seen[PART_TYPE_COUNT] = {0};
    for (int page = 0; page < PALETTE_PAGES; ++page)
    {
        assert(palette_page_name(page)[0]);
        if (page == 2)
            continue;
        for (int item = 0; item < palette_count(page); ++item)
            ++seen[palette_part(page, item)];
    }
    for (size_t type = 0; type < component_registry_count(); ++type)
        assert(seen[type] == 1);
    assert(palette_part(8, 3) == PART_NOTE);
    assert(palette_part(6, 0) == PART_SOURCE);
}

static void failed_code_send_keeps_message(SDL_Window *window)
{
    static AppState app;
    app_state_init(&app);
    int board = bench_add(&app.bench, PART_ARDUINO_UNO, 400, 300);
    app.code_part = board;
    strcpy(app.code_text, "void setup(){} void loop(){delay(123);}");
    app.code_dirty = 1;
    BenchHistory *history = app.history;
    app.history = NULL;
    key(&app, window, SDLK_RETURN, SDL_KMOD_CTRL);
    assert(app.code_dirty && !app.bench.parts[board].code);
    assert(!strcmp(app.status, tr(TXT_APP_HISTORY_FAILED)));
    click(&app, window, app_layout_code_run());
    assert(app.code_dirty && !strcmp(app.status, tr(TXT_APP_HISTORY_FAILED)));
    app.history = history;
    key(&app, window, SDLK_RETURN, SDL_KMOD_CTRL);
    assert(!app.code_dirty && app.bench.parts[board].code);
    key(&app, window, SDLK_RETURN, SDL_KMOD_CTRL);
    assert(!strcmp(app.status, tr(TXT_MCU_NO_CHANGES)));
    app_state_dispose(&app);
}

int main(void)
{
    catalog_categories_cover_every_part();
    static Workbench dense_leds;
    assert(bench_add(&dense_leds, PART_LED, 500, 350) == 0);
    assert(bench_add(&dense_leds, PART_LED, 520, 350) == 1);
    assert(app_input_part_at(&dense_leds, 500, 340, 1) == 0);
    assert(app_input_part_at(&dense_leds, 520, 340, 1) == 1);
    assert(app_input_part_at(&dense_leds, 565, 340, 1) == -1);
    assert(app_input_part_at(&dense_leds, 500, 340, 7) == 0);
    static Workbench dense_passives;
    assert(bench_add(&dense_passives, PART_RESISTOR, 500, 350) == 0);
    assert(bench_add(&dense_passives, PART_CAPACITOR, 530, 350) == 1);
    assert(app_input_part_at(&dense_passives, 500, 350, 1) == 0);
    assert(app_input_part_at(&dense_passives, 530, 350, 1) == 1);
    assert(app_input_part_at(&dense_passives, 500, 370, 1) == -1);
    assert(SDL_Init(SDL_INIT_VIDEO));
    assert(palette_filtered_count(0, "JUNCAO") == 1);
    assert(palette_filtered_part(0, "JUNÇÃO", 0) == PART_JUNCTION);
    SDL_Window *window = SDL_CreateWindow("UI flow", 1280, 720, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    assert(window);
    failed_code_send_keeps_message(window);
    static AppState app;
    app_state_init(&app);
    app.selftest = 1;
    app_input_poll(&app, window);
    search_flow(&app, window);
    focus_and_save_flow(&app, window);
    actions_and_scaling_flow(&app, window);
    values_and_chip_flow(&app, window);
    graph_options_flow(&app, window);
    bench_snapshot_clear(&app.bench);
    app.selected = -1;
    app.view = (BenchView){1, 0, 0};
    dense_led_selection_flow(&app, window);
    multi_value_flow(&app, window);
    /* A small body between enlarged connection targets must still select the part. */
    assert(bench_add(&app.bench, PART_RESISTOR, 500, 350) == 0);
    assert(bench_set_scale(&app.bench, 0, .3f));
    assert(bench_lead_move(&app.bench, 0, 0, 0, (WirePoint){493, 350}));
    assert(bench_lead_move(&app.bench, 0, 1, 0, (WirePoint){507, 350}));
    click(&app, window, (AppRect){500, 350, 1, 1});
    assert(app.selected == 0 && app.pending.component < 0);
    click(&app, window, (AppRect){507, 350, 1, 1});
    assert(app.pending.component == 0 && app.pending.pin == 1);
    key(&app, window, SDLK_ESCAPE, 0);
    bench_snapshot_clear(&app.bench);
    bench_selection_clear(&app.selection);
    app.selected = -1;
    click(&app, window, app_layout_palette_tab(8));
    click(&app, window, app_layout_palette_item(3));
    assert(app.placing == PART_NOTE);
    WirePoint note_center;
    bench_view_to_screen(&app.view, 500, 350, &note_center.x, &note_center.y);
    click(&app, window, (AppRect){note_center.x, note_center.y, 1, 1});
    assert(app.bench.part_count == 1 && app.bench.parts[0].type == PART_NOTE);
    app.selected = 0;
    assert(app.selected == 0 && bench_pin_count(app.bench.parts[0].type) == 0);
    click(&app, window, app_layout_inspector_value());
    assert(app.editing == APP_EDIT_NOTE);
    type(&app, window, "Escolha o endereco antes de gravar.");
    key(&app, window, SDLK_RETURN, 0);
    assert(!strcmp(app.bench.parts[0].code, "Escolha o endereco antes de gravar."));
    click(&app, window, app_layout_color_swatch(BENCH_COLOR_GREEN));
    assert(app.bench.parts[0].states != 0);
    unsigned background = app.bench.parts[0].states;
    AppRect foreground = app_layout_color_swatch(BENCH_COLOR_BLUE);
    foreground.y += 48;
    click(&app, window, foreground);
    assert((app.bench.parts[0].states & 255) == background);
    assert(app.bench.parts[0].states >> 8);
    key(&app, window, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.parts[0].states == background);
    /* Person drags the note corner; the opposite corner stays put, undo restores the size. */
    float left = app.bench.parts[0].x - 120;
    float top = app.bench.parts[0].y - 55;
    SDL_Event resize;
    SDL_zero(resize);
    resize.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    resize.button.button = SDL_BUTTON_LEFT;
    resize.button.clicks = 2;
    resize.button.windowID = SDL_GetWindowID(window);
    WirePoint corner;
    int window_width, window_height;
    assert(SDL_GetWindowSize(window, &window_width, &window_height));
    bench_view_to_screen(&app.view, left + 234, top + 104, &corner.x, &corner.y);
    corner = app_layout_from_ui(window_width, window_height, corner.x, corner.y);
    resize.button.x = corner.x;
    resize.button.y = corner.y;
    assert(SDL_PushEvent(&resize));
    drain(&app, window);
    resize.type = SDL_EVENT_MOUSE_MOTION;
    resize.motion.windowID = SDL_GetWindowID(window);
    bench_view_to_screen(&app.view, left + 354, top + 159, &corner.x, &corner.y);
    corner = app_layout_from_ui(window_width, window_height, corner.x, corner.y);
    resize.motion.x = corner.x;
    resize.motion.y = corner.y;
    assert(SDL_PushEvent(&resize));
    drain(&app, window);
    resize.type = SDL_EVENT_MOUSE_BUTTON_UP;
    resize.button.button = SDL_BUTTON_LEFT;
    assert(SDL_PushEvent(&resize));
    drain(&app, window);
    assert(fabsf(bench_part_scale(&app.bench.parts[0]) - 1.5f) < .01f);
    assert(fabsf(app.bench.parts[0].x - 120 * 1.5f - left) < .01f);
    assert(fabsf(app.bench.parts[0].y - 55 * 1.5f - top) < .01f);
    key(&app, window, SDLK_Z, SDL_KMOD_CTRL);
    assert(bench_part_scale(&app.bench.parts[0]) == 1);

    /* Person creates a chip, shrinks it in the inspector, rejects an impossible size, and reopens it. */
    bench_snapshot_clear(&app.bench);
    bench_selection_clear(&app.selection);
    assert(bench_add(&app.bench, PART_RESISTOR, 500, 350) == 0);
    app.selection.parts[0] = 1;
    assert(app_packing_key(&app, SDLK_G, SDL_KMOD_CTRL));
    assert(app.bench.parts[0].group == 1);
    app.selected = 0;
    app.inspector_minimized = 0;
    SDL_Event wheel;
    SDL_zero(wheel);
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = -1;
    wheel.wheel.windowID = SDL_GetWindowID(window);
    int wheel_width, wheel_height;
    assert(SDL_GetWindowSize(window, &wheel_width, &wheel_height));
    WirePoint wheel_point = app_layout_from_ui(wheel_width, wheel_height, 500, 300);
    wheel.wheel.mouse_x = wheel_point.x;
    wheel.wheel.mouse_y = wheel_point.y;
    SDL_SetModState(SDL_KMOD_CTRL);
    assert(SDL_PushEvent(&wheel));
    drain(&app, window);
    SDL_SetModState(0);
    assert(bench_group_width(&app.bench, 1) == 90);
    assert(fabsf(bench_group_height(&app.bench, 1) / 90 - 44.0f / 98) < .001f);
    click(&app, window, app_layout_inspector_scale());
    assert(app.editing == APP_EDIT_GROUP_SCALE);
    key(&app, window, SDLK_A, SDL_KMOD_CTRL);
    type(&app, window, "25");
    key(&app, window, SDLK_RETURN, 0);
    assert(app.editing == -1 && bench_group_width(&app.bench, 1) == 20);
    assert(fabsf(bench_group_height(&app.bench, 1) / 20 - 44.0f / 98) < .001f);
    click(&app, window, app_layout_inspector_scale());
    key(&app, window, SDLK_A, SDL_KMOD_CTRL);
    type(&app, window, "1");
    key(&app, window, SDLK_RETURN, 0);
    assert(app.editing == APP_EDIT_GROUP_SCALE && bench_group_width(&app.bench, 1) == 20);
    key(&app, window, SDLK_ESCAPE, 0);
    Workbench loaded = {0};
    FILE *project = tmpfile();
    assert(project && workbench_write(project, &app.bench));
    rewind(project);
    assert(workbench_read(project, &loaded));
    fclose(project);
    assert(bench_group_width(&loaded, 1) == 20);
    assert(fabsf(bench_group_height(&loaded, 1) / 20 - 44.0f / 98) < .001f);
    bench_snapshot_clear(&loaded);

    app_state_dispose(&app);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("UI flow: search, focus, safe replacement, actions and proportional input passed.");
    return 0;
}
