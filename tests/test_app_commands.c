#include "mcu_board.h"
#include <direct.h>
#include "app_commands.h"
#include "app_code_editor.h"
#include "app_packing.h"
#include "app_editing.h"
#include "workbench_pack.h"
#include "workbench_file.h"
#include "ui_text.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static void check_chip_rotation_flow(void)
{
    static AppState app;
    app_state_init(&app);
    assert(bench_add(&app.bench, PART_SWITCH, 500, 300) == 0);
    BenchSelection selection = {0};
    selection.parts[0] = 1;
    char message[256];
    assert(bench_pack_selection(&app.bench, &selection, message, sizeof(message)));
    app.selected = 0;
    float x, y, rx, ry;
    bench_terminal_position(&app.bench, (Pin){0, 0}, &x, &y);
    /* Create a chip, select it, press R, undo, redo, then complete four turns. */
    app_commands_key(&app, NULL, SDLK_R, 0);
    bench_terminal_position(&app.bench, (Pin){0, 0}, &rx, &ry);
    assert(hypotf(rx - x, ry - y) > 1);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    bench_terminal_position(&app.bench, (Pin){0, 0}, &rx, &ry);
    assert(hypotf(rx - x, ry - y) < .001f);
    app_commands_key(&app, NULL, SDLK_Y, SDL_KMOD_CTRL);
    app.selected = 0; /* Undo/redo clears selection; the person selects the chip again. */
    for (int turn = 0; turn < 3; ++turn)
        app_commands_key(&app, NULL, SDLK_R, 0);
    bench_terminal_position(&app.bench, (Pin){0, 0}, &rx, &ry);
    assert(hypotf(rx - x, ry - y) < .001f);
    app_state_dispose(&app);
}

static float pin_distance(const Workbench *bench, Pin a, float x, float y)
{
    float px, py;
    bench_terminal_position(bench, a, &px, &py);
    return hypotf(px - x, py - y);
}

/* A person mirrors parts to swap their terminals (requested 17/11/2026): M or the Espelhar button,
 * undo/redo, a rotated part, a chip, save/reopen, and the board that cannot be mirrored. */
static void check_mirror_flow(void)
{
    static AppState app;
    app_state_init(&app);
    assert(bench_add(&app.bench, PART_RESISTOR, 500, 300) == 0);
    assert(bench_add(&app.bench, PART_BREADBOARD, 400, 600) == 1);
    float x0, y0, x1, y1;
    bench_terminal_position(&app.bench, (Pin){0, 0}, &x0, &y0);
    bench_terminal_position(&app.bench, (Pin){0, 1}, &x1, &y1);
    /* 1. Select the resistor and press M: its two terminals trade places. */
    app.selected = 0;
    app_commands_key(&app, NULL, SDLK_M, 0);
    assert(app.bench.parts[0].mirrored && !strcmp(app.status, tr(TXT_APP_PART_MIRRORED)));
    assert(pin_distance(&app.bench, (Pin){0, 0}, x1, y1) < .001f);
    assert(pin_distance(&app.bench, (Pin){0, 1}, x0, y0) < .001f);
    /* 2. Undo puts them back; redo mirrors again. */
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(!app.bench.parts[0].mirrored && pin_distance(&app.bench, (Pin){0, 0}, x0, y0) < .001f);
    app_commands_key(&app, NULL, SDLK_Y, SDL_KMOD_CTRL);
    assert(app.bench.parts[0].mirrored);
    /* 3. A rotated, mirrored part still maps clicks back to the same local point. */
    app.selected = 0;
    app_commands_key(&app, NULL, SDLK_R, 0);
    const Part *part = &app.bench.parts[0];
    WirePoint local = {12, -5}, back = bench_part_to_local(part, bench_part_to_world(part, local));
    assert(fabsf(back.x - local.x) < .001f && fabsf(back.y - local.y) < .001f);
    /* 4. Opposite case: the breadboard is not mirrored and says so. */
    app.selected = 1;
    app_commands_key(&app, NULL, SDLK_M, 0);
    assert(!app.bench.parts[1].mirrored && !strcmp(app.status, tr(TXT_APP_BOARD_NO_MIRROR)));
    /* 5. A chip: its terminals move to the other side of the package. */
    assert(bench_add(&app.bench, PART_SWITCH, 900, 300) == 2);
    BenchSelection selection = {0};
    selection.parts[2] = 1;
    char message[256];
    assert(bench_pack_selection(&app.bench, &selection, message, sizeof(message)));
    const BenchGroup *group = &app.bench.groups[app.bench.parts[2].group - 1];
    float cx, cy;
    bench_terminal_position(&app.bench, (Pin){2, 0}, &cx, &cy);
    app.selected = 2;
    app_commands_key(&app, NULL, SDLK_M, 0);
    assert(group->mirrored);
    assert(pin_distance(&app.bench, (Pin){2, 0}, 2 * group->x - cx, cy) < .001f);
    /* 6. Save and reopen: the mirror of the part and of the chip is kept. */
    FILE *file = tmpfile();
    assert(file);
    WorkbenchSettings settings = workbench_settings_default();
    assert(workbench_write_project(file, &app.bench, &settings));
    rewind(file);
    Workbench reopened = {0};
    assert(workbench_read_project(file, &reopened, &settings));
    fclose(file);
    assert(reopened.parts[0].mirrored && !reopened.parts[1].mirrored);
    assert(reopened.groups[reopened.parts[2].group - 1].mirrored);
    bench_snapshot_clear(&reopened);
    app_state_dispose(&app);
}

static void check_trace_waits_for_live_batch(void)
{
    static AppState app;
    app_state_init(&app);
    assert(bench_add(&app.bench, PART_FUNCTION_GENERATOR, 300, 300) == 0);
    assert(bench_add(&app.bench, PART_OSCILLOSCOPE, 600, 300) == 1);
    app.selected = 1;
    app.dc_job.thread = (SDL_Thread *)1;
    app.dc_job.automatic = 1;
    app.dc_job.continuous = 1;
    /* Person selects the scope and presses T while live calculation is active. */
    app_commands_key(&app, NULL, SDLK_T, 0);
    assert(!app.sim.sim_running);
    assert(app.trace_pending_kind == 1 && app.trace_pending_index == 1);
    assert(!strcmp(app.status, tr(TXT_APP_CALCULATING_CURVE)));
    app.dc_job.thread = NULL;
    app_state_dispose(&app);
}

static void check_long_code_flow(void)
{
    static AppState app;
    app_state_init(&app);
    assert(bench_add(&app.bench, PART_ARDUINO_UNO, 300, 200) == 0);
    char *source = malloc(20001);
    assert(source);
    memset(source, ' ', 20000);
    source[20000] = 0;
    assert(bench_set_code(&app.bench, 0, source));
    /* Reopen a long program: the editor refuses it and Execute cannot shorten the source. */
    assert(workbench_save("build/test-long-code.elec", &app.bench));
    assert(workbench_load("build/test-long-code.elec", &app.bench));
    app_code_open(&app, NULL, 0);
    assert(app.code_part == -1);
    assert(!strcmp(app.status, tr(TXT_MCU_CODE_TOO_LONG)));
    assert(!app_code_apply(&app));
    assert(!strcmp(app.bench.parts[0].code, source));
    /* A source that fits in bytes may still overflow when tabs become indentation spaces. */
    memset(source, '\t', MCU_CODE_CAPACITY / 4);
    source[MCU_CODE_CAPACITY / 4] = 0;
    assert(bench_set_code(&app.bench, 0, source));
    app_code_open(&app, NULL, 0);
    assert(app.code_part == -1 && !strcmp(app.bench.parts[0].code, source));
    /* The largest plain draft fits, applies without truncation, saves and reopens intact. */
    memset(source, ' ', MCU_CODE_CAPACITY - 1);
    source[MCU_CODE_CAPACITY - 1] = 0;
    assert(bench_set_code(&app.bench, 0, source));
    app_code_open(&app, NULL, 0);
    assert(app.code_part == 0 && !strcmp(app.code_text, source));
    assert(!app_code_apply(&app));
    app.code_text[0] = '\n';
    source[0] = '\n';
    assert(app_code_apply(&app));
    assert(workbench_save("build/test-long-code.elec", &app.bench));
    assert(workbench_load("build/test-long-code.elec", &app.bench));
    assert(!strcmp(app.bench.parts[0].code, source));
    assert(!remove("build/test-long-code.elec"));
    free(source);
    app_state_dispose(&app);
}

static void check_project_overwrite_flow(void)
{
    static AppState app;
    app_state_init(&app);
    app.selftest = 1;
    app.save_slot = 7;
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-8.elec"));
    /* Save a resistor, add an LED, then try saving over the occupied slot. */
    assert(bench_add(&app.bench, PART_RESISTOR, 400, 300) == 0);
    app_projects_save(&app);
    assert(app.saved_slots[7]);
    assert(bench_add(&app.bench, PART_LED, 600, 300) == 1);
    app_projects_save(&app);
    Workbench saved = {0};
    WorkbenchSettings settings;
    assert(workbench_load_project(APP_TEST_PATH(APP_TEST_SLOTS, "slot-8.elec"), &saved, &settings));
    assert(saved.part_count == 1);
    bench_snapshot_clear(&saved);
    app_commands_key(&app, NULL, SDLK_ESCAPE, 0);
    assert(app.bench.part_count == 2);
    assert(app.project_overwrite == -1);
    /* Confirm targets the slot shown, even if another selection changes underneath. */
    app_projects_save(&app);
    assert(app.project_overwrite == 7);
    app.save_slot = 6;
    app_projects_confirm(&app, 1);
    assert(workbench_load_project(APP_TEST_PATH(APP_TEST_SLOTS, "slot-8.elec"), &saved, &settings));
    assert(saved.part_count == 2);
    bench_snapshot_clear(&saved);
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-8.elec"));
    app_state_dispose(&app);
}

static void check_project_growth_and_fit_flow(void)
{
    static AppState app;
    app_state_init(&app);
    app.selftest = 1;
    assert(bench_add(&app.bench, PART_LED, -400, -200) == 0);
    assert(bench_add(&app.bench, PART_RESISTOR, 1600, 900) == 1);
    for (int slot = 0; slot < 11; ++slot)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/slot-%d.elec", app_test_directory(APP_TEST_SLOTS), slot + 1);
        remove(path);
    }
    for (int slot = 0; slot < 11; ++slot)
    {
        app.save_slot = slot;
        app_projects_save(&app);
        assert(app.saved_slots[slot]);
        assert(app.project_slot_count >= slot + 2);
    }
    app.projects_open = 1;
    app.project_page = 1;
    app_editing_key(&app, SDLK_5, 0);
    assert(app.save_slot == 10);
    app.view = (BenchView){1, 0, 0};
    app_projects_load(&app);
    assert(!app.projects_open && app.bench.part_count == 2);
    for (int part = 0; part < app.bench.part_count; ++part)
    {
        float x, y;
        bench_view_to_screen(&app.view, app.bench.parts[part].x, app.bench.parts[part].y, &x, &y);
        assert(x >= BENCH_VIEW_LEFT && x <= BENCH_VIEW_LEFT + BENCH_VIEW_WIDTH);
        assert(y >= BENCH_VIEW_TOP && y <= BENCH_VIEW_TOP + BENCH_VIEW_HEIGHT);
    }
    if (!getenv("BANCADA_KEEP_PROJECT_FIXTURE"))
        for (int slot = 0; slot < 11; ++slot)
        {
            char path[128];
            snprintf(path, sizeof(path), "%s/slot-%d.elec", app_test_directory(APP_TEST_SLOTS), slot + 1);
            remove(path);
        }
    app_state_dispose(&app);
}

static void check_project_replaces_editor_and_runtime(void)
{
    static AppState app;
    static Workbench project;
    _mkdir(app_test_directory(APP_TEST_SLOTS));
    app_state_init(&app);
    app.selftest = 1;
    app.save_slot = 10;
    assert(bench_add(&app.bench, PART_ARDUINO_UNO, 500, 300) == 0);
    assert(bench_set_code(&app.bench, 0, "void setup(){}void loop(){delay(11);}"));
    app_code_open(&app, NULL, 0);
    snprintf(app.code_text, sizeof(app.code_text), "void setup(){}void loop(){delay(22);}");
    app.code_dirty = 1;
    assert(bench_add(&project, PART_ARDUINO_UNO, 500, 300) == 0);
    const char *code_b = "void setup(){}void loop(){delay(33);}";
    assert(bench_set_code(&project, 0, code_b));
    WorkbenchSettings settings = workbench_settings_default();
    assert(workbench_save_project(APP_TEST_PATH(APP_TEST_SLOTS, "slot-11.elec"), &project, &settings));
    app_projects_refresh(&app);
    app.save_slot = 11;
    app_projects_load(&app);
    assert(app.code_part == 0 && app.code_dirty && strstr(app.code_text, "delay(22)"));
    app.save_slot = 10;
    app_projects_load(&app);
    assert(app.code_part == -1 && !strcmp(app.bench.parts[0].code, code_b));
    assert(!app_code_apply(&app) && !strcmp(app.bench.parts[0].code, code_b));
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(strstr(app.bench.parts[0].code, "delay(22)"));
    app_commands_key(&app, NULL, SDLK_Y, SDL_KMOD_CTRL);
    assert(!strcmp(app.bench.parts[0].code, code_b));
    const char *program = "void setup(){pinMode(13,OUTPUT);}void loop(){digitalWrite(13,HIGH);delay(1);}";
    assert(bench_set_code(&app.bench, 0, program));
    mcu_runtime_reset(app.mcu);
    mcu_runtime_step(app.mcu, &app.bench, NULL, 1);
    int io = mcu_board_io(mcu_board(PART_ARDUINO_UNO), 13);
    assert(app.bench.parts[0].io_mode[io] == MCU_OUTPUT);
    bench_move_part(&app.bench, 0, 600, 300);
    mcu_runtime_step(app.mcu, &app.bench, NULL, 2);
    assert(app.bench.parts[0].io_mode[io] == MCU_OUTPUT);
    assert(workbench_save_project(APP_TEST_PATH(APP_TEST_SLOTS, "slot-11.elec"), &app.bench, &settings));
    app_projects_load(&app);
    mcu_runtime_step(app.mcu, &app.bench, NULL, 2);
    assert(app.bench.parts[0].io_mode[io] == MCU_OUTPUT);
    app_code_open(&app, NULL, 0);
    strcpy(app.code_text, "void setup(){}void loop(){delay(44);}");
    app.code_dirty = 1;
    app_commands_load_example(&app, 0);
    assert(app.code_part == -1);
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-11.elec"));
    bench_snapshot_clear(&project);
    app_state_dispose(&app);
}

static void failed_code_history_keeps_draft(void)
{
    static AppState app;
    app_state_init(&app);
    int board = bench_add(&app.bench, PART_ARDUINO_UNO, 400, 300);
    app.code_part = board;
    strcpy(app.code_text, "void setup(){} void loop(){delay(123);}");
    app.code_dirty = 1;
    BenchHistory *history = app.history;
    app.history = NULL;
    uint64_t revision = app.bench.revision;
    assert(!app_code_apply(&app));
    assert(app.code_dirty && app.code_part == board && app.bench.revision == revision);
    assert(!app.bench.parts[board].code);
    app.history = history;
    assert(app_code_apply(&app) && !app.code_dirty);
    assert(bench_history_undo(app.history, &app.bench) && !app.bench.parts[board].code);
    app_state_dispose(&app);
}

int main(void)
{
    failed_code_history_keeps_draft();
    check_project_replaces_editor_and_runtime();
    check_project_overwrite_flow();
    check_project_growth_and_fit_flow();
    check_long_code_flow();
    check_chip_rotation_flow();
    check_mirror_flow();
    check_trace_waits_for_live_batch();
    static AppState app;
    app_state_init(&app);
    assert(app.history);
    app_commands_key(&app, NULL, SDLK_F2, SDL_KMOD_CTRL);
    assert(app.bench.part_count == 4 && app.selected == 3);
    app.selected = 2;
    double original = app.bench.parts[2].value;
    app_commands_key(&app, NULL, SDLK_PLUS, 0);
    assert(app.bench.parts[2].value == original * 2);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].value == original);
    app_commands_key(&app, NULL, SDLK_Y, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].value == original * 2);

    app.selected = 2;
    int orientation = app.bench.parts[2].rotation;
    app_commands_key(&app, NULL, SDLK_R, 0);
    assert(app.bench.parts[2].rotation != orientation);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].rotation == orientation);

    app.selected = 2;
    app_commands_key(&app, NULL, SDLK_DELETE, 0);
    assert(app.bench.part_count == 3 && app.selected == -1);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.part_count == 4 && app.bench.parts[2].type == PART_RESISTOR);

    app_commands_key(&app, NULL, SDLK_F2, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].type == PART_DFF);
    app_commands_key(&app, NULL, SDLK_F2, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].type == PART_74HC00);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.parts[2].type == PART_DFF);
    int wires = app.bench.wire_count;
    assert(wires > 0);
    app.selected = -1;
    app.selected_wire = 0;
    app_commands_key(&app, NULL, SDLK_DELETE, 0);
    assert(app.bench.wire_count == wires - 1 && app.selected_wire == -1);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.wire_count == wires);

    sim_state_set_result(&app.sim, SIM_RESULT_TRACE, 1, app.bench.revision);
    app.sim.trace.count = 2;
    app.sim.trace_index = 0;
    app.sim.sim_running = 0;
    app_commands_key(&app, NULL, SDLK_B, 0);
    assert(app.sim.trace_index == 0);
    app_commands_key(&app, NULL, SDLK_N, 0);
    app_commands_key(&app, NULL, SDLK_N, 0);
    assert(app.sim.trace_index == 1);
    app_commands_key(&app, NULL, SDLK_SPACE, 0);
    assert(app.sim.sim_running && app.sim.trace_index == 0);
    app_commands_key(&app, NULL, SDLK_N, 0);
    assert(app.sim.trace_index == 0);

    int original_count = app.bench.part_count;
    app_commands_key(&app, NULL, SDLK_A, SDL_KMOD_CTRL);
    app_commands_key(&app, NULL, SDLK_C, SDL_KMOD_CTRL);
    app.mouse_world = (WirePoint){1000, 500};
    app_commands_key(&app, NULL, SDLK_V, SDL_KMOD_CTRL);
    assert(app.bench.part_count == original_count * 2);
    app_commands_key(&app, NULL, SDLK_DELETE, 0);
    assert(app.bench.part_count == original_count);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.part_count == original_count * 2);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.part_count == original_count);

    app.selftest = 1;
    app.save_slot = 8;
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-9.elec"));
    app.show_grid = 0;
    app.wire_color = 4;
    app_commands_key(&app, NULL, SDLK_S, SDL_KMOD_CTRL);
    assert(app.saved_slots[8]);
    bench_remove(&app.bench, 0);
    app.show_grid = 1;
    app.wire_color = 0;
    app_commands_key(&app, NULL, SDLK_L, SDL_KMOD_CTRL);
    assert(app.bench.part_count == original_count && !app.show_grid && app.wire_color == 4);
    FILE *invalid = fopen(APP_TEST_PATH(APP_TEST_SLOTS, "slot-9.elec"), "w");
    assert(invalid);
    fputs("invalid slot", invalid);
    fclose(invalid);
    uint64_t unchanged = app.bench.revision;
    app_commands_key(&app, NULL, SDLK_L, SDL_KMOD_CTRL);
    assert(app.bench.revision == unchanged && !strcmp(app.status, tr(TXT_APP_PROJECT_LOAD_FAILED)));
    remove(APP_TEST_PATH(APP_TEST_SLOTS, "slot-9.elec"));

    for (int i = 1; i <= 8; ++i)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/block-%d.elec", app_test_directory(APP_TEST_BLOCKS), i);
        remove(path);
    }
    app_commands_key(&app, NULL, SDLK_A, SDL_KMOD_CTRL);
    app_commands_key(&app, NULL, SDLK_G, SDL_KMOD_CTRL);
    assert(app.bench.groups[0].active && app.library_available[0]);
    app_packing_choose(&app, 0);
    assert(app.packed_placing);
    app_packing_place(&app, 1100, 500);
    assert(app.bench.part_count == original_count * 2 && app.bench.groups[1].active);
    assert(app.bench.groups[1].x == 1100 && app.bench.groups[1].y == 500);
    app_commands_key(&app, NULL, SDLK_G, SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
    assert(!app.bench.groups[1].active && app.bench.groups[0].active);
    app_commands_key(&app, NULL, SDLK_Z, SDL_KMOD_CTRL);
    assert(app.bench.groups[1].active);
    remove(APP_TEST_PATH(APP_TEST_BLOCKS, "block-1.elec"));

    app_state_dispose(&app);
    return 0;
}
