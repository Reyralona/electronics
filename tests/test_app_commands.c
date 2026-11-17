#include "app_commands.h"
#include "app_packing.h"
#include "workbench_pack.h"
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

int main(void)
{
    check_chip_rotation_flow();
    check_trace_waits_for_live_batch();
    static AppState app;
    app_state_init(&app);
    assert(app.history);
    app_commands_key(&app, NULL, SDLK_F2, 0);
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

    app_commands_key(&app, NULL, SDLK_F2, 0);
    assert(app.bench.parts[2].type == PART_DFF);
    app_commands_key(&app, NULL, SDLK_F2, 0);
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
    app.show_grid = 0;
    app.wire_color = 4;
    app_commands_key(&app, NULL, SDLK_S, SDL_KMOD_CTRL);
    assert(app.saved_slots[8]);
    bench_remove(&app.bench, 0);
    app.show_grid = 1;
    app.wire_color = 0;
    app_commands_key(&app, NULL, SDLK_L, SDL_KMOD_CTRL);
    assert(app.bench.part_count == original_count && !app.show_grid && app.wire_color == 4);
    FILE *invalid = fopen("build/test-slots/slot-9.elec", "w");
    assert(invalid);
    fputs("invalid slot", invalid);
    fclose(invalid);
    uint64_t unchanged = app.bench.revision;
    app_commands_key(&app, NULL, SDLK_L, SDL_KMOD_CTRL);
    assert(app.bench.revision == unchanged && !strcmp(app.status, tr(TXT_APP_PROJECT_LOAD_FAILED)));
    remove("build/test-slots/slot-9.elec");

    for (int i = 1; i <= 8; ++i)
    {
        char path[128];
        snprintf(path, sizeof(path), "build/test-blocks/block-%d.elec", i);
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
    remove("build/test-blocks/block-1.elec");

    app_state_dispose(&app);
    return 0;
}
