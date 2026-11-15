#include "app_commands.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
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
    assert(app.bench.revision == unchanged && strstr(app.status, "preservada"));
    remove("build/test-slots/slot-9.elec");

    free(app.history);
    return 0;
}
