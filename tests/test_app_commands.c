#include "app_commands.h"
#include <assert.h>
#include <stdlib.h>

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

    app.sim.trace_valid = 1;
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

    free(app.history);
    return 0;
}
