#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include "workbench.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_process.h"
#include "spice_job.h"
#include "spice_trace.h"
#include "workbench_history.h"
#include "workbench_view.h"
#include "sim_auto.h"
#include "dev_capture.h"
#include "app_state.h"
#include "app_editor.h"
#include "app_text_utf8.h"
#include "app_input.h"
#include "app_render.h"
#include "app_simulation.h"
#include "dev_scenarios.h"

/* ngspice lives next to the executable (build/spice-local); fall back to the path relative to the project root. */
static void find_ngspice(char *path, size_t capacity)
{
    const char *base = SDL_GetBasePath();
    snprintf(path, capacity, "build/spice-local/ucrt64/bin/ngspice_con.exe");
    if (base)
    {
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%sspice-local/ucrt64/bin/ngspice_con.exe", base);
        if (GetFileAttributesA(candidate) != INVALID_FILE_ATTRIBUTES)
            snprintf(path, capacity, "%s", candidate);
    }
}

static void set_window_icon(SDL_Window *window)
{
    SDL_Surface *icon = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
        return;
    Uint32 background = SDL_MapSurfaceRGBA(icon, 18, 31, 44, 255);
    Uint32 body = SDL_MapSurfaceRGBA(icon, 36, 158, 165, 255);
    Uint32 pins = SDL_MapSurfaceRGBA(icon, 230, 239, 242, 255);
    SDL_FillSurfaceRect(icon, NULL, background);
    SDL_Rect rectangle = {8, 8, 16, 16};
    SDL_FillSurfaceRect(icon, &rectangle, body);
    for (int pin = 0; pin < 3; ++pin)
    {
        rectangle = (SDL_Rect){3, 10 + pin * 5, 6, 2};
        SDL_FillSurfaceRect(icon, &rectangle, pins);
        rectangle.x = 23;
        SDL_FillSurfaceRect(icon, &rectangle, pins);
    }
    rectangle = (SDL_Rect){12, 12, 8, 2};
    SDL_FillSurfaceRect(icon, &rectangle, pins);
    rectangle = (SDL_Rect){15, 12, 2, 8};
    SDL_FillSurfaceRect(icon, &rectangle, pins);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

int main(int argc, char **argv)
{
    SDL_Window *window;
    SDL_GLContext context;
    GLuint font;
    static AppState app;
    app_state_init(&app);
    app.selftest = argc > 1 && strcmp(argv[1], "--self-test-history") == 0;
    app.capture = argc > 1 && dev_capture_requested(argv[1]);
    app.auto_capture =
        app.capture && (strcmp(argv[1], "--capture-burn") == 0 || strncmp(argv[1], "--capture-auto", 14) == 0);
    app.sim.auto_enabled = (!app.selftest && !app.capture) || app.auto_capture;
    find_ngspice(app.ngspice, sizeof(app.ngspice));

    if (dev_scenario_prepare(&app, argc, argv))
        return 1;
    if (!app.history)
    {
        fprintf(stderr, "Memoria insuficiente para historico.\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        app_state_dispose(&app);
        return 1;
    }
    Uint64 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (app.selftest)
        window_flags |= SDL_WINDOW_HIDDEN;
    window = SDL_CreateWindow("Bancada", 1280, 720, window_flags);
    if (!window)
    {
        fprintf(stderr, "Janela: %s\n", SDL_GetError());
        SDL_Quit();
        app_state_dispose(&app);
        return 1;
    }
    set_window_icon(window);
    context = SDL_GL_CreateContext(window);
    if (!context)
    {
        fprintf(stderr, "OpenGL: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        app_state_dispose(&app);
        return 1;
    }
    font = app_render_font_create();
    SDL_GL_SetSwapInterval(1);
    if (dev_scenario_analyze(&app, argv))
        return 1;
    AppState *active = &app;
    while (app.running)
    {
        int width, height;
        if (app.selftest)
            dev_scenario_queue(&app, window);
        app_input_poll(active, window);
        active = app_editor_route(active);
        if (app.capture && !strcmp(argv[1], "--capture-editor-name") && active->editor_parent)
            app_editor_begin_name(active, window);
        if (app.selftest)
            dev_scenario_validate(&app);
        app_simulation_update(active);
        SDL_GetWindowSizeInPixels(window, &width, &height);
        app_render_draw(active, font, width, height);
        SDL_GL_SwapWindow(window);
        if (app.capture)
        {
            if (!dev_capture_save("build/bancada-capture.bmp", width, height))
            {
                fprintf(stderr, "Captura: %s\n", SDL_GetError());
                app.exit_code = 1;
            }
            app.running = 0;
        }
    }
    app_editor_dispose(active);
    spice_job_join(&app.dc_job);
    if (app.selftest)
    {
        remove("build/gui-settings-test.elec");
        remove("build/test-blocks/block-1.elec");
    }
    app_state_dispose(&app);
    app_text_utf8_clear();
    if (font)
        glDeleteLists(font, 128);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return app.exit_code;
}
