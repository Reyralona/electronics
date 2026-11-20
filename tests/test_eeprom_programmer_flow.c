#include "app_input.h"
#include "app_simulation.h"
#include "app_editing.h"
#include "app_layout.h"
#include "component_devices.h"
#include "rom_image.h"
#include <windows.h>
#include <direct.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void drain(AppState *app, SDL_Window *window)
{
    for (int i = 0; i < 8; ++i)
        app_input_poll(app, window);
}

static void mouse(AppState *app, SDL_Window *window, WirePoint world, int clicks, int down)
{
    WirePoint screen;
    bench_view_to_screen(&app->view, world.x, world.y, &screen.x, &screen.y);
    int width, height;
    assert(SDL_GetWindowSize(window, &width, &height));
    screen = app_layout_from_ui(width, height, screen.x, screen.y);
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = (Uint8)clicks;
    event.button.x = screen.x;
    event.button.y = screen.y;
    assert(SDL_PushEvent(&event));
    drain(app, window);
}

static void click(AppState *app, SDL_Window *window, int part, float offset)
{
    WirePoint point = {app->bench.parts[part].x + offset, app->bench.parts[part].y};
    mouse(app, window, point, 2, 1);
    mouse(app, window, point, 2, 0);
}

static void wait_for(AppState *app, double seconds)
{
    double target = app->sim.live_time + seconds;
    Uint64 started = SDL_GetTicks();
    do
    {
        app_simulation_update(app);
        if (app->sim.auto_failed)
            fprintf(stderr, "%s\n", app->status);
        assert(!app->sim.auto_failed);
        if (app->sim.live_time >= target && app->sim.result_revision == app->bench.revision)
        {
            app_simulation_flush(app);
            return;
        }
        SDL_Delay(1);
    } while (SDL_GetTicks() - started < 15000);
    assert(!"EEPROM UI simulation timed out");
}

static int output(const AppState *app)
{
    int byte = 0, bit = 0;
    for (int i = 0; i < app->bench.part_count && bit < 8; ++i)
        if (app->bench.parts[i].type == PART_LED)
        {
            byte |= (app->leds[i] > 0) << bit;
            ++bit;
        }
    assert(bit == 8);
    return byte;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    char fixture[MAX_PATH], solver[MAX_PATH], original[MAX_PATH];
    assert(GetFullPathNameA(argv[1], sizeof(fixture), fixture, NULL));
    assert(GetFullPathNameA(argv[2], sizeof(solver), solver, NULL));
    assert(GetCurrentDirectoryA(sizeof(original), original));
    _mkdir("build/test-eeprom-ui");
    assert(SetCurrentDirectoryA("build/test-eeprom-ui"));
    _mkdir("build");
    _mkdir("build/test-slots");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("EEPROM flow", 1280, 720, SDL_WINDOW_HIDDEN);
    assert(window);
    static AppState app;
    app_state_init(&app);
    app.selftest = 1;
    app.sim.auto_enabled = 1;
    snprintf(app.ngspice, sizeof(app.ngspice), "%s", solver);
    WorkbenchSettings settings;
    assert(workbench_load_project(fixture, &app.bench, &settings));
    assert(workbench_save_project("build/test-slots/slot-1.elec", &app.bench, &settings));
    app_projects_refresh(&app);
    app_projects_load(&app);
    int dips[5], dip_count = 0, mode = -1, button = -1, rom = -1, source = -1;
    for (int i = 0; i < app.bench.part_count; ++i)
    {
        PartType type = app.bench.parts[i].type;
        if (type == PART_DIP_SWITCH)
            dips[dip_count++] = i;
        if (type == PART_SWITCH)
            mode = i;
        if (type == PART_BUTTON)
            button = i;
        if (type == PART_AT28C16)
            rom = i;
        if (type == PART_SOURCE)
            source = i;
    }
    assert(dip_count == 5 && mode >= 0 && button >= 0 && rom >= 0 && source >= 0);
    drain(&app, window);
    wait_for(&app, .006);
    assert(output(&app) == 0xFF);
    /* Person chooses address 2 and flips the eight data levers from A5 to 5A. */
    const ComponentDevice *dip = component_device(PART_DIP_SWITCH);
    click(&app, window, dips[0], dip->pins[1][0]);
    for (int bank = 3; bank < 5; ++bank)
        for (int bit = 0; bit < 4; ++bit)
            click(&app, window, dips[bank], dip->pins[bit][0]);
    assert(app.bench.parts[dips[0]].states == 2 && app.bench.parts[dips[3]].states == 10 &&
           app.bench.parts[dips[4]].states == 5);
    click(&app, window, mode, 0);
    wait_for(&app, .002);
    WirePoint cap = {app.bench.parts[button].x, app.bench.parts[button].y};
    mouse(&app, window, cap, 1, 1);
    assert(app.bench.parts[button].on);
    wait_for(&app, .002);
    mouse(&app, window, cap, 1, 0);
    assert(!app.bench.parts[button].on);
    wait_for(&app, .002);
    click(&app, window, mode, 0);
    wait_for(&app, .002);
    assert(output(&app) == 0x5A);
    click(&app, window, dips[0], dip->pins[0][0]);
    wait_for(&app, .002);
    assert(output(&app) == 0xFF);
    click(&app, window, dips[0], dip->pins[0][0]);
    wait_for(&app, .002);
    click(&app, window, source, 0);
    wait_for(&app, .002);
    assert(output(&app) == 0);
    click(&app, window, source, 0);
    wait_for(&app, .002);
    assert(output(&app) == 0x5A);
    /* Saving flushes the worker; reopening must load the written byte, not the initial FF. */
    app_projects_save(&app);
    assert(app.project_overwrite == 0);
    app_projects_confirm(&app, 1);
    app_projects_load(&app);
    RomImage image;
    assert(rom_image_parse(app.bench.parts[rom].code, &image, NULL));
    assert(image.data[2] == 0x5A && image.data[3] == 0xFF);
    wait_for(&app, .002);
    assert(output(&app) == 0x5A);
    app_state_dispose(&app);
    SDL_DestroyWindow(window);
    SDL_Quit();
    assert(remove("build/test-slots/slot-1.elec") == 0);
    assert(SetCurrentDirectoryA(original));
    puts("EEPROM person flow: open, choose address/data, press/release write, read another address, power cycle, save/reopen passed.");
    return 0;
}
